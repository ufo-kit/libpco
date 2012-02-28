/* Copyright (C) 2011, 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
   (Karlsruhe Institute of Technology)

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by the
   Free Software Foundation; either version 2.1 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
   details.

   You should have received a copy of the GNU Lesser General Public License along
   with this library; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA 02110, USA */

/**
 * \mainpage
 *
 * \section intro_sec Introduction
 *
 * libpco is a small wrapper around the PCO message protocol to communicate with
 * CameraLink-based cameras produced by PCO. However, actual frame transfer is
 * still managed by the frame grabber.
 *
 * \section intro_quickstart Quick Start
 *
 * Make sure to connect the camera and the frame grabber with the CameraLink
 * cables in correct order and start the camera. Create a new camera handle by
 * calling
 *
 * \code pco_handle pco = pco_init(); \endcode
 *
 * You can then use this handle to query information about the camera, for
 * example getting the name:
 *
 * \code
 * char *name = NULL;
 * pco_get_name(pco, &name);
 * printf("Camera name %s\n", name);
 * free(name);
 * \endcode
 *
 * To release the camera call
 *
 * \code pco_destroy(pco); \endcode
 *
 * \section api_reference API reference
 *
 * All function definitions can be found in libpco.h.
 *
 */

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
    
    size_t extra_timeout;
};

#define CHECK_ERR_CL(code) if ((code) != CL_OK) fprintf(stderr, "Error %i at %s:%i\n", (code), __FILE__, __LINE__);

/* Courtesy of PCO AG */
static void decode_line(int width, void *bufout, void* bufin)
{
    uint32_t *lineadr_in = (uint32_t *) bufin;
    uint32_t *lineadr_out = (uint32_t *) bufout;
    uint32_t a;

    for (int x = 0; x < (width*12) / 32; x += 3) {
        a = (*lineadr_in & 0x0000FFF0) >> 4;
        a |= (*lineadr_in & 0x0000000F) << 24;
        a |= (*lineadr_in & 0xFF000000) >> 8;
        *lineadr_out = a;
        lineadr_out++;

        a = (*lineadr_in & 0x00FF0000) >> 12;
        lineadr_in++;
        a |= (*lineadr_in & 0x0000F000) >> 12;
        a |= (*lineadr_in & 0x00000FFF) << 16;
        *lineadr_out = a;
        lineadr_out++;

        a = (*lineadr_in & 0xFFF00000) >> 20;
        a |= (*lineadr_in & 0x000F0000) << 8;
        lineadr_in++;
        a |= (*lineadr_in & 0x0000FF00) << 8;
        *lineadr_out = a;
        lineadr_out++;

        a = (*lineadr_in & 0x000000FF) << 4;
        a |= (*lineadr_in & 0xF0000000) >> 28;
        a |= (*lineadr_in & 0x0FFF0000);
        *lineadr_out = a;
        lineadr_out++;
        lineadr_in++;
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
        return PCO_ERROR_DRIVER_CHECKSUMERROR | PCO_ERROR_DRIVER_CAMERALINK;
    }

    int x = 0;
    for (; x < bsize; x++)
        sum += buffer[x];

    if (buffer[x] != sum) {
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
    if (err != PCO_NOERROR)
        return err;

    if ((pco->description.wSensorTypeDESC == SENSOR_CIS2051_V1_FI_BW) ||
        (pco->description.wSensorTypeDESC == SENSOR_CIS2051_V1_BI_BW)) {
        SC2_Set_Interface_Output_Format req;
        SC2_Set_Interface_Output_Format_Response resp_if;

        req.wCode = SET_INTERFACE_OUTPUT_FORMAT;
        req.wSize= sizeof(req);
        req.wFormat = pco->transfer.DataFormat & SCCMOS_FORMAT_MASK;
        req.wInterface = INTERFACE_CL_SCCMOS;
        err = pco_control_command(pco, &req, sizeof(req), &resp_if, sizeof(resp_if));
    }
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

static unsigned int pco_scan_and_set_baud_rate(pco_handle pco)
{
    static uint32_t baudrates[9][2] = { 
        { CL_BAUDRATE_921600, 921600},
        { CL_BAUDRATE_460800, 460800 },
        { CL_BAUDRATE_230400, 230400},
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

static unsigned int pco_read_property(pco_handle pco, uint16_t code, void *dst, uint32_t size)
{
    SC2_Simple_Telegram req = { .wCode = code, .wSize = sizeof(req) };
    return pco_control_command(pco, &req, sizeof(req), dst, size); 
}

static int pco_reset_serial(pco_handle pco)
{
    for (int i = 0; i < pco->num_ports; i++)
        clSerialClose(pco->serial_refs[i]);

    for (int i = 0; i < pco->num_ports; i++)
        clSerialInit(i, &pco->serial_refs[i]);

    pco->serial_ref = pco->serial_refs[0];
    return 0;
}

/**
 * Send control data via CameraLink to camera. This function sends messages as
 * defined in sc2_telegram.h to the camera device via the serial CameraLink
 * connection. However, it is strongly disadvised to use this function directly
 * but rather use one of the pco_set/get functions.
 *
 * @param pco A #pco_handle.
 * @param buffer_in Message structure
 * @param size_in Size of #buffer_in
 * @param buffer_out Memory for results message
 * @param size_out Size of #buffer_out
 * @return Error code or PCO_NOERROR.
 */
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

    /* XXX: The pco.4000 needs at least 3 times the timeout which makes things
     * slow in the beginning. */
    cl_err = clSerialRead(pco->serial_ref, (char *) buffer, &size, pco->timeouts.command * 3 + pco->extra_timeout);
    if (cl_err < 0)
        return PCO_ERROR_DRIVER_IOFAILURE | PCO_ERROR_DRIVER_CAMERALINK;
    
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

/**
 * Read camera type.
 *
 * @param pco A #pco_handle.
 * @param type Type of the camera.
 * @param subtype Sub-type of the camera.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @note Camera types are defined in sc2_defs.h
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
 * Read camera version information.
 *
 * @param pco A #pco_handle
 * @param serial_number Serial number
 * @param hw_major Hardware major version
 * @param hw_minor Hardware minor version
 * @param fw_major Firmware major version
 * @param fw_minor Firmware minor version
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_get_camera_version(pco_handle pco, uint32_t *serial_number, 
        uint16_t *hw_major, uint16_t *hw_minor, uint16_t *fw_major, uint16_t *fw_minor)
{
    SC2_Camera_Type_Response resp;
    unsigned int err = pco_read_property(pco, GET_CAMERA_TYPE, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        *serial_number = resp.dwSerialNumber;
        *hw_major = resp.dwHWVersion >> 16;
        *hw_minor = resp.dwHWVersion & 0xFFFF;
        *fw_major = resp.dwFWVersion >> 16;
        *fw_minor = resp.dwFWVersion & 0xFFFF;
    }
    return err;
}

/**
 * Read health status of the camera.
 *
 * @param pco A #pco_handle.
 * @param warnings Warnings regarding temperature and voltage
 * @param errors Errors regarding temperature and voltage
 * @param status Information about current status.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @note Defines are listed in sc2_defs.h.
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
 * Reset camera settings to default values.
 *
 * @param pco A #pco_handle.
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_reset(pco_handle pco)
{
    SC2_Reset_Settings_To_Default_Response resp;
    return pco_read_property(pco, RESET_SETTINGS_TO_DEFAULT, &resp, sizeof(resp));
}

/**
 * Read temperature of different camera components.
 *
 * @param pco A #pco_handle.
 * @param ccd Temperature of the CCD sensor chip in degree celsius times 10.
 * @param camera Temperature of the camera in degree celsius.
 * @param power Temperature of the power supply in degree celsius.
 * @return Error code or PCO_NOERROR.
 * @note If a specific temperature sensor is not available, the values are
 * undefined.
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
 * Read acceptable cooling range.
 *
 * @param pco A #pco_handle.
 * @param default_temp Default cooling temperature.
 * @param min_temp Minimum cooling temperature.
 * @param max_temp Maximum cooling temperature.
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 * @see pco_set_cooling_temperature()
 */
unsigned int pco_get_cooling_range(pco_handle pco, int16_t *default_temp, int16_t *min_temp, int16_t *max_temp)
{
    *default_temp = pco->description.sDefaultCoolSetDESC;
    *min_temp = pco->description.sMinCoolSetDESC;
    *max_temp = pco->description.sMaxCoolSetDESC;
    return PCO_NOERROR;
}

/**
 * Set cooling temperature of the CCD sensor.
 *
 * @param pco A #pco_handle.
 * @param temperature Target temperature of the CCD sensor.
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 * @note According to pco, this value should be set with a reserve of 5 degree
 * Celsius: \f$\vartheta_\textrm{cool} = \vartheta_\textrm{ambient} -
 * \Delta\vartheta_\textrm{max} + 5^\circ \mathrm{C}\f$.
 * @see pco_get_cooling_range()
 */
unsigned int pco_set_cooling_temperature(pco_handle pco, int16_t temperature)
{
    SC2_Set_Cooling_Setpoint req = { .wCode = SET_COOLING_SETPOINT_TEMPERATURE,
        .sTemp = temperature, .wSize = sizeof(req) };
    SC2_Cooling_Setpoint_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Get cooling temperature.
 * @param pco A #pco_handle.
 * @param temperature Currently set target temperature of the CCD sensor.
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 * @see pco_get_cooling_range()
 */
unsigned int pco_get_cooling_temperature(pco_handle pco, int16_t *temperature)
{
    SC2_Cooling_Setpoint_Response resp;
    unsigned int err = pco_read_property(pco, GET_CAMERA_NAME, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *temperature = resp.sTemp; 
    return err;
}

/**
 * Read the name of the camera.
 *
 * @param pco A #pco_handle.
 * @param name Pointer to zero-terminated string containing the name. Memory is
 * allocated by the library and must be freed by the caller.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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
 * Read camera resolution.
 *
 * @param pco A #pco_handle.
 * @param width_std Standard pixel width.
 * @param height_std Standard pixel height.
 * @param width_ex Extended pixel width.
 * @param height_ex Extended pixel height.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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
 * List all available pixel rates.
 *
 * @param pco A #pco_handle.
 * @param rates Memory for up to four pixel rates.
 * @param num_rates Actual number of available pixel rates.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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
 * Read current pixel rate.
 * @param pco A #pco_handle.
 * @param rate Current pixel rate.
 * @return Error code or PCO_NOERROR.
 * @since 0.2.0
 * @see pco_set_pixelrate().
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
 * Set pixel rate.
 * @param pco A #pco_handle.
 * @param rate Any of
 *    - PIXELRATE_10MHZ
 *    - PIXELRATE_20MHZ
 *    - PIXELRATE_40MHZ
 *    - PIXELRATE_5MHZ
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @note You can only specify a rate that is returned by
 * pco_get_available_pixelrates().
 */
unsigned int pco_set_pixelrate(pco_handle pco, uint32_t rate)
{
    SC2_Set_Pixelrate req = { .wCode = SET_PIXELRATE, .wSize = sizeof(req), .dwPixelrate = rate };
    SC2_Pixelrate_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read conversion factors.
 *
 * @param pco A #pco_handle.
 * @param factors Memory for up to four conversion factors.
 * @param num_rates Actual number of conversion factors that were read-out.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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

/**
 * Check if the camera is active and can be used.
 *
 * @param pco A #pco_handle.
 * @return Non-zero number if active, zero if not.
 */
unsigned int pco_is_active(pco_handle pco)
{
    SC2_Camera_Type_Response resp;
    return pco_read_property(pco, GET_CAMERA_TYPE, &resp, sizeof(resp)) == PCO_NOERROR;
}

/**
 * Set scan and readout mode. There are two modes to read out the sensor chip
 * and that affect the actual pixel clock. Both modes will essentially return 16
 * bit data with approximately 10 and 12 bit dynamic range.
 *
 * @param pco A #pco_handle.
 * @param mode PCO_SCANMODE_SLOW or PCO_SCANMODE_FAST
 * @return Error code or PCO_NOERROR.
 */
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

/**
 * Get scan and readout mode.
 *
 * @param pco A #pco_handle.
 * @param mode PCO_SCANMODE_SLOW or PCO_SCANMODE_FAST
 * @return Error code or PCO_NOERROR.
 * @see pco_set_scan_mode()
 */
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

/**
 * Set trigger mode.
 * @param pco A #pco_handle.
 * @param mode Trigger mode:
 *   - TRIGGER_MODE_AUTO_TRIGGER
 *   - TRIGGER_MODE_SOFTWARE_TRIGGER
 *   - TRIGGER_MODE_EXTERNALTRIGGER
 *   - TRIGGER_MODE_EXTERNALEXPOSURECONTROL
 *   - TRIGGER_MODE_SOURCE_HDSDI
 *   - TRIGGER_MODE_EXTERNAL_SYNCHRONIZED
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_set_trigger_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Trigger_Mode req = { .wCode = SET_TRIGGER_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Trigger_Mode_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Get trigger mode
 *
 * @param pco A #pco_handle.
 * @param mode Trigger mode.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @see pco_set_trigger_mode()
 */
unsigned int pco_get_trigger_mode(pco_handle pco, uint16_t *mode)
{
    SC2_Trigger_Mode_Response resp;
    unsigned int err = pco_read_property(pco, GET_TRIGGER_MODE, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *mode = resp.wMode;
    return err;
}

/**
 * Set automatic data transfer.
 *
 * @param pco A #pco_handle.
 * @param transfer 0 if no automatic transfer, else 1.
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_set_auto_transfer(pco_handle pco, int transfer)
{
    pco->transfer.Transmit = transfer ? 1 : 0;
    return pco_set_cl_config(pco);
}

/**
 * Return if automatic data transfer is currently used.
 *
 * @param pco A #pco_handle.
 * @param transfer 0 if no automatic transfer, else 1.
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_get_auto_transfer(pco_handle pco, int *transfer)
{
    *transfer = pco->transfer.Transmit ? 1 : 0;
    return PCO_NOERROR;
}

/**
 * Initiate a software-triggered exposure.
 *
 * @param pco A #pco_handle.
 * @param success Flag indicating successful exposure.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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
 * Get storage mode.
 *
 * @param pco A #pco_handle.
 * @param mode storage mode.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @see pco_set_storage_mode()
 */
unsigned int pco_get_storage_mode(pco_handle pco, uint16_t *mode)
{
    SC2_Storage_Mode_Response resp;
    unsigned int err = pco_read_property(pco, GET_STORAGE_MODE, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *mode = resp.wMode;
    return err;
}

/**
 * Set storage mode.
 *
 * @param pco A #pco_handle.
 * @param mode STORAGE_MODE_RECORDER or STORAGE_MODE_FIFO_BUFFER.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_set_storage_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Storage_Mode req = { .wCode = SET_STORAGE_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Storage_Mode_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Get recording state.
 *
 * @param pco A #pco_handle.
 * @param state 1 if recording, 0 if stopped.
 * @return Error code or PCO_NOERROR.
 */
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

/**
 * Start or stop recording.
 *
 * @param pco A #pco_handle.
 * @param state 1 to record, 0 to stop.
 * @return Error code or PCO_NOERROR.
 * @note Before starting any recording you have to call pco_arm_camera()!
 */
unsigned int pco_set_rec_state(pco_handle pco, uint16_t state)
{
    const uint32_t REC_WAIT_TIME = 500;
    uint16_t g_state, x = 0;
    unsigned int err = PCO_NOERROR;

    pco_get_rec_state(pco, &g_state);

    if (g_state != state) {
        uint32_t s, ns;
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
 * Get current acquisition mode.
 *
 * @param pco A #pco_handle.
 * @param mode Acquisition mode 
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @see pco_set_acquire_mode()
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
 * Set image acquisition mode.
 *
 * @param pco A #pco_handle.
 * @param mode ACQUIRE_MODE_AUTO, ACQUIRE_MODE_EXTERNAL (acq enable signal) or
 * ACQUIRE_MODE_EXTERNAL_FRAME_TRIGGER.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_set_acquire_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Acquire_Mode req = { .wCode = SET_ACQUIRE_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Acquire_Mode_Response resp; 
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Set timestamp mode. If this is not TIMESTAMP_MODE_OFF, a timestamp will be
 * integrated into the frame either in binary BCD form or as visible characters.
 *
 * @param pco A #pco_handle.
 * @param mode The timestamp mode:
 *    - TIMESTAMP_MODE_OFF
 *    - TIMESTAMP_MODE_BINARY
 *    - TIMESTAMP_MODE_ASCII
 *    - TIMESTAMP_MODE_BINARYANDASCII
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_set_timestamp_mode(pco_handle pco, uint16_t mode)
{
    SC2_Timestamp_Mode_Response resp;
    SC2_Set_Timestamp_Mode com;
    com.wMode = mode;
    com.wCode = SET_TIMESTAMP_MODE;
    com.wSize = sizeof(com);
    return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

/**
 * Set time scale of delay and exposure. Parameter values for delay and exposure can be:
 *    - TIMEBASE_NS
 *    - TIMEBASE_US
 *    - TIMEBASE_MS
 * @param pco A #pco_handle.
 * @param delay Scale of delay.
 * @param exposure Scale of exposure.
 * @return Error code or PCO_NOERROR.
 *
 * @warning Using this function is strongly disadvised. Using the pco.dimax we
 * have to reset the serial connection otherwise subsequent commands will block.
 * However, resetting the serial connection breaks subsequent commands on the
 * pco.4000 ...
 */
unsigned int pco_set_timebase(pco_handle pco, uint16_t delay, uint16_t exposure)
{
    unsigned int err;
    SC2_Set_Timebase com;
    SC2_Timebase_Response resp;

    com.wCode = SET_TIMEBASE;
    com.wSize = sizeof(SC2_Set_Timebase);
    com.wTimebaseDelay = delay;
    com.wTimebaseExposure = exposure;
    err = pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
    /* pco_reset_serial(pco); */
    return err;
}

/**
 * Set current delay and exposure settings.
 *
 * @param pco A #pco_handle.
 * @param delay Delay before exposure starts.
 * @param exposure Exposure time.
 * @return Error code or PCO_NOERROR.
 * @see pco_set_timebase()
 */
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

/**
 * Read current delay and exposure settings.
 *
 * @param pco A #pco_handle.
 * @param delay Delay before exposure starts.
 * @param exposure Exposure time.
 * @return Error code or PCO_NOERROR.
 * @see pco_set_timebase()
 */
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

/**
 * Set the region of interest window.
 *
 * @param pco A #pco_handle.
 * @param window Four-element array with the first two elements denoting
 *   upper-left corner and last two elements the lower-right corner of the ROI
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_set_roi(pco_handle pco, uint16_t *window)
{
    SC2_Set_ROI req = {
        .wCode = SET_ROI, .wSize = sizeof(req),
        .wROI_x0 = window[0], .wROI_y0 = window[1],
        .wROI_x1 = window[2], .wROI_y1 = window[3]
    };
    SC2_ROI_Response resp;

    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read current region of interest window.
 *
 * @param pco A #pco_handle.
 * @param window Four-element array 
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_get_roi(pco_handle pco, uint16_t *window)
{
    unsigned int err = PCO_NOERROR;
    SC2_ROI_Response resp;
    err = pco_read_property(pco, GET_ROI, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        window[0] = resp.wROI_x0;
        window[1] = resp.wROI_y0;
        window[2] = resp.wROI_x1;
        window[3] = resp.wROI_y1;
    }
    return err; 
}

/**
 * Set hotpixel correction.
 *
 * @param pco A #pco_handle.
 * @param mode Any of:
 *    - HOT_PIXEL_CORRECTION_OFF
 *    - HOT_PIXEL_CORRECTION_ON
 *    - HOT_PIXEL_CORRECTION_TEST
 * @return Error code or PCO_NOERROR.
 */
unsigned int pco_set_hotpixel_correction(pco_handle pco, uint32_t mode)
{
    SC2_Set_Hot_Pixel_Correction_Mode com;
    SC2_Hot_Pixel_Correction_Mode_Response resp;

    com.wCode = SET_HOT_PIXEL_CORRECTION_MODE;
    com.wSize = sizeof(com);
    com.wMode = mode;
    return pco_control_command(pco, &com, sizeof(com), &resp, sizeof(resp));
}

/**
 * Prepare camera for recording.
 *
 * @param pco A #pco_handle.
 * @return Error code or PCO_NOERROR.
 * @note This method must be called before starting the recording.
 */
unsigned int pco_arm_camera(pco_handle pco)
{
    unsigned int err;
    SC2_Simple_Telegram req;
    SC2_Arm_Camera_Response resp;

    req.wCode = ARM_CAMERA;
    req.wSize = sizeof(SC2_Simple_Telegram);
    pco->extra_timeout = 5000;
    err = pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
    pco->extra_timeout = 0;
    return err;
}

/**
 * Get number of currently recorded frames.
 *
 * @param pco A #pco_handle.
 * @param segment Number of segment that is queried.
 * @param num_images Number of recorded frames in that segment.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_get_num_images(pco_handle pco, uint16_t segment, uint32_t *num_images)
{
    SC2_Number_of_Images req = { .wCode = GET_NUMBER_OF_IMAGES_IN_SEGMENT,
        .wSize = sizeof(req), .wSegment = segment };
    SC2_Number_of_Images_Response resp;
    unsigned int err = pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *num_images = resp.dwValid;
    return err;
}

/**
 * Return size of each of the four segments.
 *
 * @param pco A #pco_handle
 * @param sizes Four-element array that contains the size of each segment in
 * bytes.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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
 * Return number of the currently used memory segment.
 *
 * @param pco A #pco_handle
 * @param segment Current segment.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
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
 * Clear the active segment and set frame counter to 0.
 *
 * @param pco A #pco_handle
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_clear_active_segment(pco_handle pco)
{
    SC2_Simple_Telegram req = { .wCode = CLEAR_RAM_SEGMENT, .wSize = sizeof(req) };    
    SC2_Clear_RAM_Segment_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read bit alignment of data.
 *
 * @param pco A #pco_handle
 * @param msb_aligned True if data is aligned on most significant bit.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_get_bit_alignment(pco_handle pco, bool *msb_aligned)
{
    SC2_Bit_Alignment_Response resp;
    unsigned int err = pco_read_property(pco, GET_BIT_ALIGNMENT, &resp, sizeof(resp));
    if (err == PCO_NOERROR)
        *msb_aligned = resp.wAlignment == 0;
    return err;
}

/**
 * Set bit alignment of image data.
 *
 * @param pco A #pco_handle
 * @param msb_aligned True if data should be aligned to the most significant
 * bit.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_set_bit_alignment(pco_handle pco, bool msb_aligned)
{
    SC2_Set_Bit_Alignment req = { 
        .wCode = SET_BIT_ALIGNMENT, 
        .wSize = sizeof(req), 
        .wAlignment = msb_aligned ? 0 : 1 
    };
    SC2_Bit_Alignment_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}


/**
 * Trigger image request and transfer.
 * @param pco A #pco_handle
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int pco_request_image(pco_handle pco)
{
    SC2_Request_Image req = { .wCode = REQUEST_IMAGE, .wSize = sizeof(req) };
    SC2_Request_Image_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read images from a segment. This method is only useful for cameras with
 * on-board memory that can be read out after recording (e.g. pco.dimax and
 * pco.4000).
 *
 * @param pco A #pco_handle
 * @param segment Number of the segment from where to read. 
 * @param start First frame to read.
 * @param end Last frame to read.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 * @see pco_get_active_segment()
 * @note Although you can specify start and end frame, it is _not_ possible to
 * read a range of images due to the hardware implementation. You always have to
 * specify start == end.
 */
unsigned int pco_read_images(pco_handle pco, uint16_t segment, uint32_t start, uint32_t end)
{
    SC2_Read_Images_from_Segment req = { 
        .wCode = READ_IMAGES_FROM_SEGMENT, 
        .wSize = sizeof(req),
        .wSegment = segment,
        .dwStartImage = start,
        .dwLastImage = end
    };
    SC2_Read_Images_from_Segment_Response resp;
    return pco_control_command(pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Return size of read-out data in pixels.
 *
 * @param pco A #pco_handle
 * @param width Width of the frame.
 * @param height Height of the frame.
 * @return Error code or PCO_NOERROR.
 */
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

/**
 * Return the currently used re-order function.
 * @param pco A #pco_handle
 * @return Pointer to a #pco_reorder_image_t function.
 */
pco_reorder_image_t pco_get_reorder_func(pco_handle pco)
{
    return pco->reorder_image;
}

/**
 * Initialize a PCO camera.
 * @return An initialized #pco_handle or NULL.
 */
pco_handle pco_init(void)
{
    pco_handle pco = (pco_handle) malloc(sizeof(struct pco_t));
    if (pco == NULL)
        return NULL;

    memset(pco, 0, sizeof(pco));
    
    pco->reorder_image = &pco_reorder_image_5x16;
    pco->timeouts.command = PCO_SC2_COMMAND_TIMEOUT;
    pco->timeouts.image = PCO_SC2_IMAGE_TIMEOUT_L;
    pco->timeouts.transfer = PCO_SC2_COMMAND_TIMEOUT;
    pco->extra_timeout = 0;

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

    /* Okay pco. You like to torture me. With insane default settings. */
    pco_set_bit_alignment(pco, false);

    if (pco_read_property(pco, GET_CAMERA_DESCRIPTION, &pco->description, sizeof(pco->description)) != PCO_NOERROR)
        goto no_pco;

    return pco;

no_pco:
    free(pco);
    return NULL;
}

/**
 * Close pco device.
 * @param pco A #pco_handle.
 */
void pco_destroy(pco_handle pco)
{
    pco_set_rec_state(pco, 0);
    for (int i = 0; i < pco->num_ports; i++)
        clSerialClose(pco->serial_refs[i]);
    free(pco);
}

