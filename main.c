#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "fgrab_struct.h"
#include "fgrab_prototyp.h"

#include "clser.h"

/* pco.edge specific */
#include "sc2_defs.h"
#include "sc2_cl.h"
#include "sc2_common.h"
#include "sc2_command.h"
#include "sc2_telegram.h"
#include "sc2_add.h"
#include "PCO_err.h"

void check_error_fg(Fg_Struct *fg, int code)
{
    if (code != FG_OK)
        printf("Error [%i]: %s\n", Fg_getLastErrorNumber(fg), Fg_getLastErrorDescription(fg));
}

void check_error_cl(int code)
{
    static char err[256];

    if (code != CL_OK) {
        unsigned int size;
        clGetErrorText(code, err, &size);
        printf("Error [%i]: %s\n", code, err);
    }
}

struct Fg_Params {
    int parameter;
    const char *desc;
};

static struct Fg_Params params[] = {
    { FG_CAMSTATUS, "Status (1 = active)" },
    { FG_TIMEOUT, "Timeout" },
    { FG_WIDTH, "Width" },
    { FG_HEIGHT, "Height" },
    { FG_PIXELDEPTH, "Depth of pixels (in bits)" },
    { FG_EXPOSURE, "Exposure time (in µs)" },
    { FG_TRIGGERMODE, "Trigger Mode" },
    { 0, NULL }
};

void print_parameters(Fg_Struct *fg, unsigned int dma_index)
{
    int value, ret; 
    for (int i = 0; params[i].desc != NULL; i++) {
        ret = Fg_getParameter(fg, params[i].parameter, &value, dma_index);
        check_error_fg(fg, ret);
        printf(" %s: %i\n", params[i].desc, value);
    }
}

typedef struct pco_edge_t {
    unsigned int num_ports;
    unsigned int baud_rate;

    void *serial_refs[4];
    void *serial_ref;

    PCO_SC2_TIMEOUTS timeouts;
    PCO_SC2_CL_TRANSFER_PARAM transfer;
    SC2_Camera_Description_Response description;
    SC2_Firmware_Versions_Response firmware_version;
} pco_edge;


#define PCO_ERROR_LOG(s) { printf("pco: %s <%s:%i>\n", s, __FILE__, __LINE__); }

uint32_t pco_build_checksum(unsigned char *buffer, int *size)
{
    unsigned char sum = 0;
    unsigned short *b = (unsigned short *) buffer;
    b++;
    int bsize = *b - 1;
    if (bsize > *size)
        return PCO_ERROR_DRIVER_CHECKSUMERROR | PCO_ERROR_DRIVER_CAMERALINK;

    int x = 0;
    for (; x < bsize; x++)
        sum += buffer[x];
    buffer[x] = sum;
    *size = x + 1;
    return PCO_NOERROR;
}

uint32_t pco_test_checksum(unsigned char *buffer, int *size)
{
    unsigned char sum = 0;
    unsigned short *b = (unsigned short *) buffer;
    b++;
    int bsize = *b - 1;
    if (bsize > *size) {
        PCO_ERROR_LOG("checksum error");
        return PCO_ERROR_DRIVER_CHECKSUMERROR | PCO_ERROR_DRIVER_CAMERALINK;
    }

    int x = 0;
    for (; x < bsize; x++)
        sum += buffer[x];

    if (buffer[x] != sum) {
        PCO_ERROR_LOG("checksum error");
        return PCO_ERROR_DRIVER_CHECKSUMERROR | PCO_ERROR_DRIVER_CAMERALINK;
    }

    *size = x + 1;
    return PCO_NOERROR;
}

unsigned int pco_control_command(struct pco_edge_t *pco,
        void *buffer_in, uint32_t size_in,
        void *buffer_out, uint32_t size_out)
{
    unsigned char buffer[PCO_SC2_DEF_BLOCK_SIZE];
    unsigned int size;
    uint16_t com_in, com_out;
    uint32_t err;

    check_error_cl(clFlushPort(pco->serial_ref));
    com_out = 0;
    com_in = *((uint16_t *) buffer_in);
    memset(buffer, 0, PCO_SC2_DEF_BLOCK_SIZE);

    size = size_in;

    /* TODO: build checksum */
    err = pco_build_checksum((unsigned char *) buffer_in, (int *) &size);
    if (err != PCO_NOERROR)
        printf("Something happened... but is ignored in the original code\n");

    check_error_cl(clSerialWrite(pco->serial_ref, (char *) buffer_in, &size, pco->timeouts.command));
    size = sizeof(uint16_t) * 2;

    clSerialRead(pco->serial_ref, (char *) buffer, &size, pco->timeouts.command*2);
    com_out = *((uint16_t*) buffer);

    uint16_t *b = (uint16_t *) buffer;
    b++;
    size = (unsigned int) *b;
    if (size > PCO_SC2_DEF_BLOCK_SIZE)
        size = PCO_SC2_DEF_BLOCK_SIZE;
    size -= sizeof(uint16_t) * 2;

    if ((size < 0) || (com_in != (com_out & 0xFF3F)))
        return PCO_ERROR_DRIVER_IOFAILURE | PCO_ERROR_DRIVER_CAMERALINK;

    clSerialRead(pco->serial_ref, (char *) &buffer[sizeof(uint16_t)*2], &size, pco->timeouts.command*2);

    err = PCO_NOERROR;
    com_out = *((uint16_t *) buffer);
    if ((com_out & RESPONSE_ERROR_CODE) == RESPONSE_ERROR_CODE) {
        SC2_Failure_Response resp;
        memcpy(&resp, buffer, sizeof(SC2_Failure_Response));
        err = resp.dwerrmess;
        if ((resp.dwerrmess & 0xC000FFFF) == PCO_ERROR_FIRMWARE_NOT_SUPPORTED)
            ;
        else
            ;
        size = sizeof(SC2_Failure_Response);
    }
    else
        size = size_out;

    if (err == PCO_NOERROR) {
        if (com_out != (com_in | RESPONSE_OK_CODE))
            err = PCO_ERROR_DRIVER_DATAERROR | PCO_ERROR_DRIVER_CAMERALINK;
    }

    err = pco_test_checksum(buffer, (int *) &size);
    if (err == PCO_NOERROR) {
        size -= 1;
        if (size < size_out)
            size_out = size;
        memcpy(buffer_out, buffer, size_out);
    }
    return err;
}

unsigned int pco_retrieve_baud_rate(struct pco_edge_t *pco)
{
    static uint32_t baudrates[] = { 
        CL_BAUDRATE_9600,
        CL_BAUDRATE_19200,
        CL_BAUDRATE_38400,
        CL_BAUDRATE_57600,
        CL_BAUDRATE_115200,
        CL_BAUDRATE_230400,
        CL_BAUDRATE_460800,
        CL_BAUDRATE_921600,
        0 
    };

    unsigned int err = PCO_NOERROR+1;

    SC2_Simple_Telegram com;
    SC2_Camera_Type_Response resp;
    com.wCode = GET_CAMERA_TYPE;
    com.wSize = sizeof(SC2_Simple_Telegram);
    int baudrate_index = 0;
    while ((err != PCO_NOERROR) && (baudrates[baudrate_index] != 0)) {
        check_error_cl(clSetBaudRate(pco->serial_ref, baudrates[baudrate_index]));
        Sleep(150);
        err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(SC2_Camera_Type_Response));
        if (err != PCO_NOERROR)
            baudrate_index++;
    }

    pco->baud_rate = baudrates[baudrate_index];
    return err;
}

unsigned int pco_retrieve_cl_config(struct pco_edge_t *pco)
{
    SC2_Simple_Telegram com;
    SC2_Get_CL_Configuration_Response resp;
    SC2_Get_Interface_Output_Format com_iface;
    SC2_Get_Interface_Output_Format_Response resp_iface;
    unsigned int err = PCO_NOERROR;

    com.wCode = GET_CL_CONFIGURATION;
    com.wSize = sizeof(SC2_Simple_Telegram);
    err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));

    if (err != PCO_NOERROR)
        printf("GET_CL_CONFIGURATION failed\n");
    else {
        pco->transfer.ClockFrequency = resp.dwClockFrequency;
        pco->transfer.CCline = resp.bCCline;
        pco->transfer.Transmit = resp.bTransmit;
        pco->transfer.DataFormat = resp.bDataFormat;
    }

    com_iface.wCode = GET_INTERFACE_OUTPUT_FORMAT;
    com_iface.wSize = sizeof(com_iface);
    com_iface.wInterface = INTERFACE_CL_SCCMOS;
    err = pco_control_command(pco, &com_iface, sizeof(com_iface), &resp_iface, sizeof(resp_iface));
    if (err == PCO_NOERROR)
        pco->transfer.DataFormat |= resp_iface.wFormat;

    return err;
}

unsigned int pco_set_cl_config(struct pco_edge_t *pco)
{
    SC2_Set_CL_Configuration cl_com;
    SC2_Get_CL_Configuration_Response cl_resp;
    unsigned int err = PCO_NOERROR;

    cl_com.wCode = SET_CL_CONFIGURATION;
    cl_com.wSize = sizeof(cl_com);
    cl_com.dwClockFrequency = pco->transfer.ClockFrequency;
    cl_com.bTransmit = pco->transfer.Transmit & 0xFF;
    cl_com.bCCline = pco->transfer.CCline & 0xFF;
    cl_com.bDataFormat = pco->transfer.DataFormat & 0xFF;

    err = pco_control_command(pco, &cl_com, sizeof(cl_com), &cl_resp, sizeof(cl_resp));
    if (err != PCO_NOERROR) {
        PCO_ERROR_LOG("SET_CL_CONFIGURATION failed");
        return err;
    }

    if ((pco->description.wSensorTypeDESC == SENSOR_CIS2051_V1_FI_BW) ||
        (pco->description.wSensorTypeDESC == SENSOR_CIS2051_V1_BI_BW)) {
        SC2_Set_Interface_Output_Format com_set_if;
        SC2_Set_Interface_Output_Format_Response resp_if;

        com_set_if.wCode = SET_INTERFACE_OUTPUT_FORMAT;
        com_set_if.wSize= sizeof(com_set_if);
        com_set_if.wFormat = pco->transfer.DataFormat & SCCMOS_FORMAT_MASK;
        com_set_if.wInterface = INTERFACE_CL_SCCMOS;
        err = pco_control_command(pco, &com_set_if, sizeof(com_set_if), &resp_if, sizeof(resp_if));
        if (err != PCO_NOERROR)
            PCO_ERROR_LOG("SCCMOS SET_INTERFACE_OUTPUT_FORMAT");
    }
    return err;
}

unsigned int pco_read_property(struct pco_edge_t *pco, uint16_t code, void *dst, uint32_t size)
{
    SC2_Simple_Telegram com;
    com.wCode = code;
    com.wSize = sizeof(com);
    return pco_control_command(pco, &com, sizeof(com), dst, size); 
}

unsigned int pco_get_rec_state(struct pco_edge_t *pco, uint16_t *state)
{
    SC2_Recording_State_Response resp;
    SC2_Simple_Telegram com;
    unsigned int err = PCO_NOERROR;

    com.wCode = GET_RECORDING_STATE;
    com.wSize = sizeof(SC2_Simple_Telegram);
    err = pco_control_command(pco, &com, sizeof(SC2_Simple_Telegram),
        &resp, sizeof(SC2_Recording_State_Response));

    *state =resp.wState;
    return err;
}

unsigned int pco_set_rec_state(struct pco_edge_t *pco, uint16_t state)
{
#define REC_WAIT_TIME 50

    uint16_t g_state, x = 0;
    unsigned int err = PCO_NOERROR;

    pco_get_rec_state(pco, &g_state);

    if (g_state != state) {
        uint32_t s,ns;
        SC2_Set_Recording_State com;
        SC2_Recording_State_Response resp;

        com.wCode = SET_RECORDING_STATE;
        com.wState = state;
        com.wSize = sizeof(SC2_Set_Recording_State);
        err = pco_control_command(pco, &com, sizeof(SC2_Set_Recording_State),
                &resp, sizeof(SC2_Recording_State_Response));

        if(err != PCO_NOERROR)
            return err;

        SC2_COC_Runtime_Response coc;
        pco_read_property(pco, GET_COC_RUNTIME, &coc, sizeof(coc));
        s = coc.dwtime_s;
        ns = coc.dwtime_s;

        ns /= 1000000;
        ns += 1;
        ns += s*1000;

        ns += REC_WAIT_TIME;
        ns /= 50;

        for(int x = 0; x < ns; x++) {
            pco_get_rec_state(pco, &g_state);
            if(g_state == state)
                break;

            Sleep(50);
        }
        if(x >= ns)
            err=PCO_ERROR_TIMEOUT;
    }
    return err;
}

unsigned int pco_set_timestamp_mode(struct pco_edge_t *pco, uint16_t mode)
{
    SC2_Timestamp_Mode_Response resp;
    SC2_Set_Timestamp_Mode com;
    com.wMode = mode;
    com.wCode = SET_TIMESTAMP_MODE;
    com.wSize = sizeof(com);
    return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_set_timebase(struct pco_edge_t *pco, uint16_t delay,uint16_t expos)
{
   SC2_Set_Timebase com;
   SC2_Timebase_Response resp;

   com.wCode = SET_TIMEBASE;
   com.wSize = sizeof(SC2_Set_Timebase);
   com.wTimebaseDelay = delay;
   com.wTimebaseExposure = expos;
   return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_set_delay_exposure(struct pco_edge_t *pco, uint32_t delay,uint32_t expos)
{
   SC2_Set_Delay_Exposure com;
   SC2_Delay_Exposure_Response resp;

   com.wCode = SET_DELAY_EXPOSURE_TIME;
   com.wSize = sizeof(SC2_Set_Delay_Exposure);
   com.dwDelay = delay;
   com.dwExposure = expos;
   return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_arm_camera(struct pco_edge_t *pco)
{
  SC2_Arm_Camera_Response resp;
  SC2_Simple_Telegram com;
  unsigned int err=PCO_NOERROR;
  /* FIXME: is timeout setting neccessary? */
  /*uint32_t time=5000;*/

  /*Set_Timeouts(&time,sizeof(uint32_t));*/
  com.wCode = ARM_CAMERA;
  com.wSize = sizeof(SC2_Simple_Telegram);
  err = pco_control_command(pco, &com, sizeof(SC2_Simple_Telegram), &resp, sizeof(SC2_Arm_Camera_Response));

  /*time=PCO_SC2_COMMAND_TIMEOUT;*/
  /*Set_Timeouts(&time,sizeof(uint32_t));*/
  return err;
}

unsigned int pco_get_actual_size(struct pco_edge_t *pco, uint32_t *width, uint32_t *height)
{
   unsigned int err = PCO_NOERROR;

   SC2_Simple_Telegram com;
   SC2_ROI_Response resp;
   com.wCode = GET_ROI;
   com.wSize = sizeof(SC2_Simple_Telegram);
   err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));

   if(err == PCO_NOERROR) {
       *width = resp.wROI_x1 - resp.wROI_x0 + 1;
       *height = resp.wROI_y1 - resp.wROI_y0 + 1;
   }
   return err;
}


int main(int argc, char const* argv[])
{
    static const char *applet = "libFullAreaGray8.so";

    /* CameraLink specific */
    printf("--- CameraLink ---------\n");

    struct pco_edge_t pco;
    pco.timeouts.command = PCO_SC2_COMMAND_TIMEOUT;
    pco.timeouts.image = PCO_SC2_IMAGE_TIMEOUT_L;
    pco.timeouts.transfer = PCO_SC2_COMMAND_TIMEOUT;
    for (int i = 0; i < 4; i++)
        pco.serial_refs[i] = NULL;

    unsigned int buffer_size, version, err;
    char str[256];

    clGetNumSerialPorts(&pco.num_ports);
    printf(" Ports: %i\n", pco.num_ports);

    clGetManufacturerInfo(str, &buffer_size, &version);
    printf(" Manufacturer: %s\n", str);
    printf(" Version: %x\n", version);

    for (int i = 0; i < pco.num_ports && i < 4; i++) {
        check_error_cl(clGetSerialPortIdentifier(i, str, &buffer_size));
        printf(" Port Identifier (Port %i): %s\n", i, str);

        check_error_cl(clSerialInit(i, &pco.serial_refs[i]));
    }

    /* Reference the first port for easier access */
    pco.serial_ref = pco.serial_refs[0];

    /* Query properties and output them */
    printf(" scanning for baud rate... ");
    fflush(stdout);
    pco_retrieve_baud_rate(&pco);
    printf("index %i\n", pco.baud_rate);

    err = pco_read_property(&pco, GET_CAMERA_DESCRIPTION, &pco.description, sizeof(pco.description));
    if (err != PCO_NOERROR)
        PCO_ERROR_LOG("GET_CAMERA_DESCRIPTION failed");

    if (pco_retrieve_cl_config(&pco) == PCO_NOERROR)
        printf(" Clock frequency: %i MHz\n", pco.transfer.ClockFrequency/1000000);

    SC2_Temperature_Response temperature;
    if (pco_read_property(&pco, GET_TEMPERATURE, &temperature, sizeof(temperature)) == PCO_NOERROR) {
        printf(" CCD temperature: %i°C\n", temperature.sCCDtemp);
        printf(" Camera temperature: %i°C\n", temperature.sCamtemp);
        printf(" Power supply temperature: %i°C\n", temperature.sPStemp);
    }
   
    SC2_Pixelrate_Response pixelrate;
    if (pco_read_property(&pco, GET_PIXELRATE, &pixelrate, sizeof(pixelrate)) == PCO_NOERROR)
        printf(" Pixel rate: %i\n", pixelrate.dwPixelrate);

    /* Setup for recording */
    if (pco_set_rec_state(&pco, 0) != PCO_NOERROR)
        PCO_ERROR_LOG("SET RECORDING STATE failed");

    if (pco_set_timestamp_mode(&pco, 2) != PCO_NOERROR)
        PCO_ERROR_LOG("SET TIMESTAMP failed");

    if (pco_set_timebase(&pco, 1, 1) != PCO_NOERROR)
        PCO_ERROR_LOG("SET TIMEBASE failed");

    /* XXX: here followed pco_set_delay_exposure() with unknown values */

    if (pco.transfer.DataFormat != (SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER|PCO_CL_DATAFORMAT_5x12)) {
        pco.transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x12;

        if (pco_set_cl_config(&pco) != PCO_NOERROR)
            PCO_ERROR_LOG("Setting CameraLink config failed");
    }

    uint32_t width, height;
    if (pco_get_actual_size(&pco, &width, &height) == PCO_NOERROR) {
        printf(" Dimensions: %ix%i\n", width, height);
    }

    /* Frame grabber specific */
    Fg_Struct * fg;

    fg = Fg_Init(applet, 0);
    if (fg != NULL) {
        printf("\n--- Port A -------------\n");
        print_parameters(fg, PORT_A);
        printf("\n--- Port B -------------\n");
        print_parameters(fg, PORT_B);
    }
    else {
        printf("Could not initialize frame grabber with '%s'\n", applet);
    }

    /* Start grabbing */
    printf("\n--- Grabbing one frame on Port A\n");
    int val = FG_CL_8BIT_FULL_10;
    check_error_fg(fg, Fg_setParameter(fg, FG_CAMERA_LINK_CAMTYP, &val, PORT_A));

    val = FG_GRAY;
    check_error_fg(fg, Fg_setParameter(fg, FG_FORMAT, &val, PORT_A));

    val = FREE_RUN;
    check_error_fg(fg, Fg_setParameter(fg, FG_TRIGGERMODE, &val, PORT_A));

    /*
    val = 0xFFFFFFFF;
    check_error_fg(fg, Fg_setParameter(fg, FG_TIMEOUT, &val, PORT_A));
    */

    check_error_fg(fg, Fg_setParameter(fg, FG_WIDTH, &width, PORT_A));
    check_error_fg(fg, Fg_setParameter(fg, FG_HEIGHT, &height, PORT_A));
    printf(" Actual dimensions: %ix%i\n", width, height);

    dma_mem *mem = Fg_AllocMemEx(fg, width*height*sizeof(uint16_t), 4);
    if (mem == NULL) {
        printf(" Couldn't allocate buffer memory\n");
    }

    pco_set_rec_state(&pco, 1);
    Sleep(150);
    check_error_fg(fg, Fg_AcquireEx(fg, 0, 4, ACQ_BLOCK, mem));
    frameindex_t last_frame = Fg_getLastPicNumberEx(fg, PORT_A, mem);
    if (last_frame < 0) {
        printf(" Couldn't retrieve last frame\n");
    }
    else {
        uint16_t *frame = (uint16_t *) Fg_getImagePtrEx(fg, 1, PORT_A, mem);
        printf("%p\n", frame);
        FILE *fp = fopen("out.raw", "wb");
        fwrite(frame, 2*width*height, 1, fp);
        fclose(fp);
    }

    /* Close CameraLink interfaces and frame grabber */
    check_error_fg(fg, Fg_FreeMemEx(fg, mem));
    Fg_FreeGrabber(fg);

    for (int i = 0; (i < 4) && (pco.serial_refs[i] != NULL); i++)
        clSerialClose(pco.serial_refs[i]);
    
    return 0;
}
