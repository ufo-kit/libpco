
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <clser.h>
#include "libpco.h"

#include "sc2_defs.h"
#include "sc2_cl.h"
#include "sc2_common.h"
#include "sc2_command.h"
#include "sc2_telegram.h"
#include "sc2_add.h"
#include "PCO_err.h"

void check_error_cl(int code)
{
    static char err[256];

    if (code != CL_OK) {
        unsigned int size;
        clGetErrorText(code, err, &size);
        printf("Error [%i]: %s\n", code, err);
    }
}

static uint32_t pco_build_checksum(unsigned char *buffer, int *size)
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

static uint32_t pco_test_checksum(unsigned char *buffer, int *size)
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

static void pco_msleep(int time)
{
    int ret;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    tv.tv_sec = time / 1000;
    tv.tv_usec = (time % 1000) / 1000;
    ret = select(0, NULL, NULL, NULL, &tv);
    if (ret < 0)
        PCO_ERROR_LOG("error in select");
}

struct pco_edge_t *pco_init(void)
{
    /* TODO: check memory allocation */
    struct pco_edge_t *pco = (struct pco_edge_t *) malloc(sizeof(struct pco_edge_t));

    pco->timeouts.command = PCO_SC2_COMMAND_TIMEOUT;
    pco->timeouts.image = PCO_SC2_IMAGE_TIMEOUT_L;
    pco->timeouts.transfer = PCO_SC2_COMMAND_TIMEOUT;

    for (int i = 0; i < 4; i++)
        pco->serial_refs[i] = NULL;

    check_error_cl(clGetNumSerialPorts(&pco->num_ports));

    if (pco->num_ports > 4)
        pco->num_ports = 4;

    for (int i = 0; i < pco->num_ports; i++)
        check_error_cl(clSerialInit(i, &pco->serial_refs[i]));
    
    /* Reference the first port for easier access */
    pco->serial_ref = pco->serial_refs[0];
    return pco;
}

void pco_destroy(struct pco_edge_t *pco)
{
    for (int i = 0; i < pco->num_ports; i++)
        clSerialClose(pco->serial_refs[i]);
    free(pco);
}

unsigned int pco_control_command(struct pco_edge_t *pco,
        void *buffer_in, uint32_t size_in,
        void *buffer_out, uint32_t size_out)
{
    unsigned char buffer[PCO_SC2_DEF_BLOCK_SIZE];
    unsigned int size;
    uint16_t com_in, com_out;
    uint32_t err = PCO_NOERROR;

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

    err = clSerialRead(pco->serial_ref, (char *) &buffer[sizeof(uint16_t)*2], &size, pco->timeouts.command*2);
    if (err < 0) 
        return PCO_ERROR_DRIVER_IOFAILURE | PCO_ERROR_DRIVER_CAMERALINK;

    com_out = *((uint16_t *) buffer);
    if ((com_out & RESPONSE_ERROR_CODE) == RESPONSE_ERROR_CODE) {
        SC2_Failure_Response resp;
        memcpy(&resp, buffer, sizeof(SC2_Failure_Response));
        err = resp.dwerrmess;
        if ((resp.dwerrmess & 0xC000FFFF) == PCO_ERROR_FIRMWARE_NOT_SUPPORTED)
            ;   /* TODO: log message here */
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

unsigned int pco_active(struct pco_edge_t *pco)
{
    SC2_Camera_Type_Response resp;
    return pco_read_property(pco, GET_CAMERA_TYPE, &resp, sizeof(resp)) == PCO_NOERROR;
}

unsigned int pco_scan_and_set_baud_rate(struct pco_edge_t *pco)
{
    static uint32_t baudrates[6][2] = { 
        { CL_BAUDRATE_115200, 115200 },
        { CL_BAUDRATE_57600, 57600 },
        { CL_BAUDRATE_38400, 38400 },
        { CL_BAUDRATE_19200, 19200 },
        { CL_BAUDRATE_9600, 9600 },
        { 0, 0 }
    };

    unsigned int err = PCO_NOERROR+1;

    SC2_Simple_Telegram com;
    SC2_Camera_Type_Response resp;
    com.wCode = GET_CAMERA_TYPE;
    com.wSize = sizeof(SC2_Simple_Telegram);
    int idx = 0;

    while ((err != PCO_NOERROR) && (baudrates[idx][0] != 0)) {
        check_error_cl(clSetBaudRate(pco->serial_ref, baudrates[idx][0]));
        pco_msleep(150);
        err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(SC2_Camera_Type_Response));
        if (err != PCO_NOERROR)
            idx++;
    }
    pco->baud_rate = baudrates[idx][1];

    /* XXX: in the original code, SC2_Set_CL_Baudrate telegrams were send.
     * However, doing so makes things worse and communication breaks. */

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

            pco_msleep(50);
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

unsigned int pco_set_delay_exposure(struct pco_edge_t *pco, uint32_t delay, uint32_t exposure)
{
   SC2_Set_Delay_Exposure com;
   SC2_Delay_Exposure_Response resp;

   com.wCode = SET_DELAY_EXPOSURE_TIME;
   com.wSize = sizeof(SC2_Set_Delay_Exposure);
   com.dwDelay = delay;
   com.dwExposure = exposure;
   return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_get_delay_exposure(struct pco_edge_t *pco, uint32_t *delay, uint32_t *exposure)
{
   uint32_t err = PCO_NOERROR;

   SC2_Delay_Exposure_Response resp;
   if (pco_read_property(pco, GET_DELAY_EXPOSURE_TIME, &resp, sizeof(resp)) == PCO_NOERROR) {
       *delay = resp.dwDelay;
       *exposure = resp.dwExposure;
   }
   return err;
}

unsigned int pco_arm_camera(struct pco_edge_t *pco)
{
  SC2_Arm_Camera_Response resp;
  SC2_Simple_Telegram com;
  unsigned int err = PCO_NOERROR;

  com.wCode = ARM_CAMERA;
  com.wSize = sizeof(SC2_Simple_Telegram);
  err = pco_control_command(pco, &com, sizeof(SC2_Simple_Telegram), &resp, sizeof(SC2_Arm_Camera_Response));

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

