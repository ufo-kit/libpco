
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
#include "config.h"

struct pco_t {
    unsigned int num_ports;
    unsigned int baud_rate;

    /**
     * Pointer to image correction function. This is automatically set to the
     * correct internal function, when pco_set_scan_mode() is called.
     */
    void (*reorder_image)(uint16_t *bufout, uint16_t *bufin, int width, int height);

    void *serial_refs[4];
    void *serial_ref;

    PCO_SC2_TIMEOUTS timeouts;
    PCO_SC2_CL_TRANSFER_PARAM transfer;
    SC2_Camera_Description_Response description;
};

void check_error_cl(int code)
{
    static char err[256];

    if (code != CL_OK) {
        unsigned int size;
        clGetErrorText(code, err, &size);
        printf("Error [%i]: %s\n", code, err);
    }
}

#define CHECK_ERR_CL(code) if ((code) != CL_OK) fprintf(stderr, "Error %i at %s:%i\n", (code), __FILE__, __LINE__);

/* Courtesy of PCO AG */
static void decode_line(int width, void *bufout, void* bufin)
{
    uint32_t *lineadr_in = (uint32_t *) bufin;
    uint32_t *lineadr_out = (uint32_t *) bufout;
    uint32_t a;

    for (int x = 0; x < (width*12)/32;) {
        a = (*lineadr_in&0x0000FFF0)>>4;
        a|= (*lineadr_in&0x0000000F)<<24;
        a|= (*lineadr_in&0xFF000000)>>8;
        *lineadr_out = a;
        lineadr_out++;

        a = (*lineadr_in&0x00FF0000)>>12;
        lineadr_in++;
        x++;
        a|= (*lineadr_in&0x0000F000)>>12;
        a|= (*lineadr_in&0x00000FFF)<<16;
        *lineadr_out = a;
        lineadr_out++;

        a = (*lineadr_in&0xFFF00000)>>20;
        a|= (*lineadr_in&0x000F0000)<<8;
        lineadr_in++;
        x++;
        a|= (*lineadr_in&0x0000FF00)<<8;
        *lineadr_out = a;
        lineadr_out++;

        a = (*lineadr_in&0x000000FF)<<4;
        a|= (*lineadr_in&0xF0000000)>>28;
        a|= (*lineadr_in&0x0FFF0000);
        *lineadr_out = a;
        lineadr_out++;
        lineadr_in++;
        x++;
    }
}

static void pco_reorder_image_5x12(uint16_t *bufout, uint16_t *bufin, int width, int height)
{
    uint16_t *line_top = bufout;
    uint16_t *line_bottom = bufout + (height-1)*width;
    uint16_t *line_in = bufin;
    int off = (width*12)/16;

    for (int y = 0; y < height/2; y++) {
        decode_line(width, line_top, line_in);
        line_in += off;
        decode_line(width, line_bottom, line_in);
        line_in += off;
        line_top += width;
        line_bottom -= width;
    }
}

static void pco_reorder_image_5x16(uint16_t *bufout, uint16_t *bufin, int width, int height)
{
    uint16_t *line_top = bufout;
    uint16_t *line_bottom = bufout + (height-1)*width;
    uint16_t *line_in = bufin;

    for (int y = 0; y < height/2; y++) {
        memcpy(line_top, line_in, width*sizeof(uint16_t));
        line_in += width;
        memcpy(line_bottom, line_in, width*sizeof(uint16_t));
        line_in += width;
        line_top += width;
        line_bottom -= width;
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

unsigned int pco_control_command(pco_handle pco,
        void *buffer_in, uint32_t size_in,
        void *buffer_out, uint32_t size_out)
{
    unsigned char buffer[PCO_SC2_DEF_BLOCK_SIZE];
    unsigned int size;
    uint16_t com_in, com_out;
    uint32_t err = PCO_NOERROR;
    int cl_err = CL_OK;

    cl_err = clFlushPort(pco->serial_ref);
    CHECK_ERR_CL(cl_err);

    com_out = 0;
    com_in = *((uint16_t *) buffer_in);
    memset(buffer, 0, PCO_SC2_DEF_BLOCK_SIZE);

    size = size_in;

    err = pco_build_checksum((unsigned char *) buffer_in, (int *) &size);
    if (err != PCO_NOERROR)
        printf("Something happened... but is ignored in the original code\n");

    cl_err = clSerialWrite(pco->serial_ref, (char *) buffer_in, &size, pco->timeouts.command);
    CHECK_ERR_CL(cl_err);
    size = sizeof(uint16_t) * 2;

    pco_msleep(100);

    cl_err = clSerialRead(pco->serial_ref, (char *) buffer, &size, pco->timeouts.command*2);
    com_out = *((uint16_t*) buffer);

    uint16_t *b = (uint16_t *) buffer;
    b++;
    size = (unsigned int) *b;
    if (size > PCO_SC2_DEF_BLOCK_SIZE)
        size = PCO_SC2_DEF_BLOCK_SIZE;
    size -= sizeof(uint16_t) * 2;

    if ((size < 0) || (com_in != (com_out & 0xFF3F)))
        return PCO_ERROR_DRIVER_IOFAILURE | PCO_ERROR_DRIVER_CAMERALINK;

    cl_err = clSerialRead(pco->serial_ref, (char *) &buffer[sizeof(uint16_t)*2], &size, pco->timeouts.command*2);
    CHECK_ERR_CL(cl_err);
    if (cl_err < 0) 
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

static unsigned int pco_set_cl_config(pco_handle pco)
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
        (pco->description.wSensorTypeDESC == SENSOR_CIS2051_V1_BI_BW) ||
        (pco->description.wSensorTypeDESC == SENSOR_CYPRESS_RR_V1_BW)) {
        SC2_Set_Interface_Output_Format req;
        SC2_Set_Interface_Output_Format_Response resp_if;

        req.wCode = SET_INTERFACE_OUTPUT_FORMAT;
        req.wSize= sizeof(req);
        req.wFormat = pco->transfer.DataFormat & SCCMOS_FORMAT_MASK;
        req.wInterface = INTERFACE_CL_SCCMOS;
        err = pco_control_command(pco, &req, sizeof(req), &resp_if, sizeof(resp_if));
        if (err != PCO_NOERROR)
            PCO_ERROR_LOG("SCCMOS SET_INTERFACE_OUTPUT_FORMAT");
    }
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_camera_type(pco_handle pco, uint16_t *type, uint16_t *subtype)
{
    SC2_Camera_Type_Response resp;
    unsigned int err = pco_read_property(pco, GET_CAMERA_TYPE, &resp, sizeof(resp));
    if (err == PCO_NOERROR) {
        *type = resp.wCamType;
        *subtype = resp.wCamSubType;
    }
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_health_state(pco_handle pco, uint32_t *warnings, uint32_t *errors, uint32_t *status)
{
    SC2_Camera_Health_Status_Response resp;
    unsigned int err = pco_read_property(pco, GET_CAMERA_HEALTH_STATUS, &resp, sizeof(resp));
    if (err == PCO_NOERROR) {
        *warnings = resp.dwWarnings; 
        *errors = resp.dwErrors;
        *status = resp.dwStatus;
    }
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_reset(pco_handle pco)
{
    SC2_Reset_Settings_To_Default_Response resp;
    return pco_read_property(pco, RESET_SETTINGS_TO_DEFAULT, &resp, sizeof(resp));
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_temperature(pco_handle pco, uint32_t *ccd, uint32_t *camera, uint32_t *power)
{
    SC2_Temperature_Response resp;
    unsigned int err = pco_read_property(pco, GET_TEMPERATURE, &resp, sizeof(resp));
    if (err == PCO_NOERROR) {
        *ccd = resp.sCCDtemp;
        *camera = resp.sCamtemp;
        *power = resp.sPStemp;
    }
    return err;
}

/**
 * \note since 0.2.0
 * \note Client program has to free memory of *name
 */
unsigned int pco_get_name(pco_handle pco, char **name)
{
    SC2_Camera_Name_Response resp;
    unsigned int err = pco_read_property(pco, GET_CAMERA_NAME, &resp, sizeof(resp));
    if (err == PCO_NOERROR) {
        char *s = (char *) malloc(40);
        strncpy(s, resp.szName, 40);
        *name = s;
    }
    else
        *name = NULL;
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_resolution(pco_handle pco, uint16_t *width_std, uint16_t *height_std, uint16_t *width_ex, uint16_t *height_ex)
{
    *width_std = pco->description.wMaxHorzResStdDESC;
    *height_std = pco->description.wMaxVertResStdDESC;
    *width_ex = pco->description.wMaxHorzResExtDESC;
    *height_ex = pco->description.wMaxVertResExtDESC;
    return PCO_NOERROR;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_available_pixelrates(pco_handle pco, uint32_t rates[4], int *num_rates)
{
    int j = 0;
    for (int i = 0; i < 4; i++)
        if (pco->description.dwPixelRateDESC[i] > 0)
            rates[j++] = pco->description.dwPixelRateDESC[i];

    *num_rates = j-1;
    return PCO_NOERROR;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_pixelrate(pco_handle pco, uint32_t *rate)
{
    SC2_Pixelrate_Response resp;
    unsigned int err = pco_read_property(pco, GET_PIXELRATE, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *rate = resp.dwPixelrate;
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_set_pixelrate(pco_handle pco, uint32_t rate)
{
    SC2_Set_Pixelrate req = { .wCode = SET_PIXELRATE, .wSize = sizeof(req), .dwPixelrate = rate };
    SC2_Pixelrate_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_available_conversion_factors(pco_handle pco, uint16_t factors[4], int *num_rates)
{
    int j = 0;
    for (int i = 0; i < 4; i++)
        if (pco->description.wConvFactDESC[i] > 0)
            factors[j++] = pco->description.wConvFactDESC[i];

    *num_rates = j-1;
    return PCO_NOERROR;
}

unsigned int pco_is_active(pco_handle pco)
{
    SC2_Camera_Type_Response resp;
    return pco_read_property(pco, GET_CAMERA_TYPE, &resp, sizeof(resp)) == PCO_NOERROR;
}

unsigned int pco_set_scan_mode(pco_handle pco, uint32_t mode)
{
    unsigned int err = PCO_NOERROR;
    const uint32_t pixel_clock = pco->description.dwPixelRateDESC[mode];
    if (pixel_clock == 0)
        return PCO_ERROR_IS_ERROR;

    if (mode == PCO_SCANMODE_SLOW) {
        pco->reorder_image = &pco_reorder_image_5x16;
        pco->transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x16;
    }
    else if (mode == PCO_SCANMODE_FAST) {
        pco->reorder_image = &pco_reorder_image_5x12;
        pco->transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x12;
    }

    if ((err = pco_set_cl_config(pco)) != PCO_NOERROR) 
        return err;

    SC2_Set_Pixelrate com;
    com.wCode = SET_PIXELRATE;
    com.wSize = sizeof(SC2_Set_Pixelrate);
    com.dwPixelrate = pixel_clock;
    SC2_Pixelrate_Response resp;

    return pco_control_command(pco, &com, sizeof(SC2_Set_Pixelrate), &resp, sizeof(SC2_Pixelrate_Response));
}

unsigned int pco_get_scan_mode(pco_handle pco, uint32_t *mode)
{
    unsigned int err = PCO_NOERROR;
    SC2_Pixelrate_Response pixelrate;

    if ((err = pco_read_property(pco, GET_PIXELRATE, &pixelrate, sizeof(pixelrate))) == PCO_NOERROR) {
        for (int i = 0; i < 4; i++) {
            if (pixelrate.dwPixelrate == pco->description.dwPixelRateDESC[i]) {
                *mode = i;
                return PCO_NOERROR;
            }
        }
        *mode = 0xFFFF;
        return PCO_ERROR_IS_ERROR;
    }
    return err;
}

static unsigned int pco_scan_and_set_baud_rate(pco_handle pco)
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
    int idx = 0, cl_err;

    while ((err != PCO_NOERROR) && (baudrates[idx][0] != 0)) {
        cl_err = clSetBaudRate(pco->serial_ref, baudrates[idx][0]);
        pco_msleep(300);
        err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(SC2_Camera_Type_Response));
        if (err != PCO_NOERROR)
            idx++;
    }
    pco->baud_rate = baudrates[idx][1];

    /* XXX: in the original code, SC2_Set_CL_Baudrate telegrams were send.
     * However, doing so makes things worse and communication breaks. */

    return err;
}

static unsigned int pco_retrieve_cl_config(pco_handle pco)
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
    com_iface.wInterface = SET_INTERFACE_CAMERALINK;
    err = pco_control_command(pco, &com_iface, sizeof(com_iface), &resp_iface, sizeof(resp_iface));
    if (err == PCO_NOERROR)
        pco->transfer.DataFormat |= resp_iface.wFormat;

    return err;
}

unsigned int pco_read_property(pco_handle pco, uint16_t code, void *dst, uint32_t size)
{
    SC2_Simple_Telegram req = { .wCode = code, .wSize = sizeof(req) };
    return pco_control_command(pco, &req, sizeof(req), dst, size); 
}

/**
 * \note since 0.2.0
 */
unsigned int pco_set_trigger_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Trigger_Mode req = { .wCode = SET_TRIGGER_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Trigger_Mode_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_trigger_mode(pco_handle pco, uint16_t *mode)
{
    SC2_Trigger_Mode_Response resp;
    unsigned int err = pco_read_property(pco, GET_TRIGGER_MODE, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *mode = resp.wMode;
    return err;
}

unsigned int pco_set_auto_transfer(pco_handle pco, int transfer)
{
    pco->transfer.Transmit = transfer ? 1 : 0;
    return pco_set_cl_config(pco);
}

unsigned int pco_get_auto_transfer(pco_handle pco, int *transfer)
{
    *transfer = pco->transfer.Transmit ? 1 : 0;
    return PCO_NOERROR;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_force_trigger(pco_handle pco, uint32_t *success)
{
    SC2_Force_Trigger_Response resp;
    unsigned int err = pco_read_property(pco, FORCE_TRIGGER, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *success = resp.wReturn;
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_storage_mode(pco_handle pco, uint16_t *mode)
{
    SC2_Storage_Mode_Response resp;
    unsigned int err = pco_read_property(pco, GET_STORAGE_MODE, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *mode = resp.wMode;
    return err;
}

unsigned int pco_set_storage_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Storage_Mode req = { .wCode = SET_STORAGE_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Storage_Mode_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

unsigned int pco_get_rec_state(pco_handle pco, uint16_t *state)
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

unsigned int pco_set_rec_state(pco_handle pco, uint16_t state)
{
#define REC_WAIT_TIME 500

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

/**
 * \note since 0.2.0
 */
unsigned int pco_get_acquire_mode(pco_handle pco, uint16_t *mode)
{
    SC2_Acquire_Mode_Response resp;
    unsigned int err = pco_read_property(pco, GET_ACQUIRE_MODE, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *mode = resp.wMode;
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_set_acquire_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Acquire_Mode req = { .wCode = SET_ACQUIRE_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Acquire_Mode_Response resp; 
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

unsigned int pco_set_timestamp_mode(pco_handle pco, uint16_t mode)
{
    SC2_Timestamp_Mode_Response resp;
    SC2_Set_Timestamp_Mode com;
    com.wMode = mode;
    com.wCode = SET_TIMESTAMP_MODE;
    com.wSize = sizeof(com);
    return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_set_timebase(pco_handle pco, uint16_t delay,uint16_t expos)
{
   SC2_Set_Timebase com;
   SC2_Timebase_Response resp;

   com.wCode = SET_TIMEBASE;
   com.wSize = sizeof(SC2_Set_Timebase);
   com.wTimebaseDelay = delay;
   com.wTimebaseExposure = expos;
   return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_set_delay_exposure(pco_handle pco, uint32_t delay, uint32_t exposure)
{
   SC2_Set_Delay_Exposure com;
   SC2_Delay_Exposure_Response resp;

   com.wCode = SET_DELAY_EXPOSURE_TIME;
   com.wSize = sizeof(SC2_Set_Delay_Exposure);
   com.dwDelay = delay;
   com.dwExposure = exposure;
   return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_get_delay_exposure(pco_handle pco, uint32_t *delay, uint32_t *exposure)
{
   unsigned int err = PCO_NOERROR;
   SC2_Delay_Exposure_Response resp;
   if (pco_read_property(pco, GET_DELAY_EXPOSURE_TIME, &resp, sizeof(resp)) == PCO_NOERROR) {
       *delay = resp.dwDelay;
       *exposure = resp.dwExposure;
   }
   return err;
}

unsigned int pco_set_roi(pco_handle pco, uint16_t *window)
{
    SC2_Set_ROI com;
    SC2_ROI_Response resp;

    com.wCode = SET_ROI;
    com.wSize = sizeof(com);
    com.wROI_x0 = window[0];
    com.wROI_y0 = window[1];
    com.wROI_x1 = window[2];
    com.wROI_y1 = window[3];
    return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_get_roi(pco_handle pco, uint16_t *window)
{
    unsigned int err = PCO_NOERROR;
    SC2_ROI_Response resp;
    /*if (pco_read_property(pco, GET_ROI, &resp, sizeof(resp)) == PCO_NOERROR) {*/
    pco_read_property(pco, GET_ROI, &resp, sizeof(resp));
        window[0] = resp.wROI_x0;
        window[1] = resp.wROI_y0;
        window[2] = resp.wROI_x1;
        window[3] = resp.wROI_y1;
    /*}*/
    return err; 
}

unsigned int pco_set_hotpixel_correction(pco_handle pco, uint32_t mode)
{
    SC2_Set_Hot_Pixel_Correction_Mode com;
    SC2_Hot_Pixel_Correction_Mode_Response resp;

    com.wCode = SET_HOT_PIXEL_CORRECTION_MODE;
    com.wSize = sizeof(com);
    com.wMode = mode;
    return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

unsigned int pco_arm_camera(pco_handle pco)
{
    SC2_Simple_Telegram req;
    SC2_Arm_Camera_Response resp;

    req.wCode = ARM_CAMERA;
    req.wSize = sizeof(SC2_Simple_Telegram);
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_num_images(pco_handle pco, uint32_t segment, uint32_t *num_images)
{
    SC2_Number_of_Images req = { .wCode = GET_NUMBER_OF_IMAGES_IN_SEGMENT, .wSize = sizeof(req), .wSegment = segment };
    SC2_Number_of_Images_Response resp;
    unsigned int err = pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *num_images = resp.dwValid;
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_segment_sizes(pco_handle pco, size_t sizes[4])
{
    SC2_Camera_RAM_Segment_Size_Response resp;
    unsigned int err = pco_read_property(pco, GET_CAMERA_RAM_SEGMENT_SIZE, &resp, sizeof(resp));
    if (err == PCO_NOERROR) {
        sizes[0] = resp.dwSegment1Size; 
        sizes[1] = resp.dwSegment2Size; 
        sizes[2] = resp.dwSegment3Size; 
        sizes[3] = resp.dwSegment4Size; 
    }
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_get_active_segment(pco_handle pco, uint16_t *segment)
{
    SC2_Active_RAM_Segment_Response resp;
    unsigned int err = pco_read_property(pco, GET_ACTIVE_RAM_SEGMENT, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *segment = resp.wSegment;
    return err;
}

/**
 * \note since 0.2.0
 */
unsigned int pco_clear_active_segment(pco_handle pco)
{
    SC2_Simple_Telegram req = { .wCode = CLEAR_RAM_SEGMENT, .wSize = sizeof(req) };    
    SC2_Clear_RAM_Segment_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * \note since 0.2.0
 */
unsigned int pco_request_image(pco_handle pco)
{
    SC2_Request_Image req = { .wCode = REQUEST_IMAGE, .wSize = sizeof(req) };
    SC2_Request_Image_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * \note since 0.2.0
 */
unsigned int pco_read_images(pco_handle pco, uint32_t segment, uint32_t start, uint32_t end)
{
    SC2_Read_Images_from_Segment req = { 
        .wCode = READ_IMAGES_FROM_SEGMENT, 
        .wSize = sizeof(req),
        .dwStartImage = start,
        .dwLastImage = end
    };
    SC2_Read_Images_from_Segment_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

unsigned int pco_get_actual_size(pco_handle pco, uint32_t *width, uint32_t *height)
{
   unsigned int err = PCO_NOERROR;

   SC2_Simple_Telegram com;
   SC2_ROI_Response resp;
   com.wCode = GET_ROI;
   com.wSize = sizeof(SC2_Simple_Telegram);
   err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));

   if (err == PCO_NOERROR) {
       *width = resp.wROI_x1 - resp.wROI_x0 + 1;
       *height = resp.wROI_y1 - resp.wROI_y0 + 1;
   }
   return err;
}

pco_reorder_image_t pco_get_reorder_func(pco_handle pco)
{
    return pco->reorder_image;
}

pco_handle pco_init(void)
{
    pco_handle pco = (pco_handle) malloc(sizeof(struct pco_t));
    if (pco == NULL)
        return NULL;

    pco->timeouts.command = PCO_SC2_COMMAND_TIMEOUT;
    pco->timeouts.image = PCO_SC2_IMAGE_TIMEOUT_L;
    pco->timeouts.transfer = PCO_SC2_COMMAND_TIMEOUT;

    for (int i = 0; i < 4; i++)
        pco->serial_refs[i] = NULL;

    if (clGetNumSerialPorts(&pco->num_ports) != CL_OK) {
        fprintf(stderr, "Unable to query number of ports\n");
        goto no_pco;
    }

    if (pco->num_ports > 4)
        pco->num_ports = 4;

    for (int i = 0; i < pco->num_ports; i++) {
        if (clSerialInit(i, &pco->serial_refs[i]) != CL_OK) {
            fprintf(stderr, "Unable to initialize serial connection\n");
            goto no_pco;
        }
    }
    
    /* Reference the first port for easier access */
    pco->serial_ref = pco->serial_refs[0];

    if (pco_scan_and_set_baud_rate(pco) != PCO_NOERROR) {
        fprintf(stderr, "Unable to scan and set baud rate\n");
        goto no_pco;
    }

    pco_set_rec_state(pco, 0);
    pco_retrieve_cl_config(pco);
    pco_set_cl_config(pco);

    if (pco_read_property(pco, GET_CAMERA_DESCRIPTION, &pco->description, sizeof(pco->description)) != PCO_NOERROR)
        goto no_pco;

    return pco;

no_pco:
    free(pco);
    return NULL;
}

void pco_destroy(pco_handle pco)
{
    pco_set_rec_state(pco, 0);
    for (int i = 0; i < pco->num_ports; i++)
        clSerialClose(pco->serial_refs[i]);
    free(pco);
}

