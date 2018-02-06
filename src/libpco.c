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
 * \section intro_installation Installation
 *
 * Check out a current branch from the repository or untar a release tarball
 * into some directory. Create a new empty directory and issue
 * \code cmake <path-to-libpco>
 * \endcode
 * in a terminal. In order to change the installation prefix or library suffix
 * on 64-bit systems, you can call cmake with
 * -DCMAKE_INSTALL_PREFIX=/some/other/path and -DLIB_SUFFIX=64. Build and
 * install the library with
 * \code
 * make && make install
 * \endcode
 * pkg-config meta information is also installed and can be queried with
 * \code
 * pkg-config --cflags --libs pco
 * \endcode
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
 * To release the camera, call
 *
 * \code pco_destroy(pco); \endcode
 *
 * \section frame_grabbing Frame Grabbing
 *
 * libpco is just a small wrapper and it is encouraged to use a higher level
 * abstraction to make use of the different pco camera types. However, if you
 * are using the SiliconSoftware SDK and want to grab frames on your own, you
 * can write something like this:
 *
 * \code
 * // The actual "applet" is specific to the camera type.
 * static const char *applet = "libDualAreaGray16.so";
 * int port = PORT_A;
 * Fg_Struct *fg = Fg_Init(applet, 0);
 *
 * // The actual CameraLink connection is specific to the camera type and frame
 * // grabber.
 * int val = FG_CL_SINGLETAP_16_BIT;
 * Fg_setParameter(fg, FG_CAMERA_LINK_CAMTYP, &val, port);
 *
 * // The frame format is specific to the camera type. Especially on the
 * // pco.edge you have to use FG_GRAY and do the decoding on your own using the
 * // pco_get_reorder_func()
 * val = FG_GRAY16;
 * Fg_setParameter(fg, FG_FORMAT, &val, port);
 *
 * val = FREE_RUN;
 * Fg_setParameter(fg, FG_TRIGGERMODE, &val, port);
 *
 * fg, Fg_setParameter(fg, FG_WIDTH, &width, port);
 * fg, Fg_setParameter(fg, FG_HEIGHT, &height, port);
 *
 * const int n_buffers = 5;
 *
 * dma_mem *mem = Fg_AllocMemEx(fg, n_buffers*width*height*sizeof(uint16_t), n_buffers);
 * if (mem == NULL)
 *     fprintf(stderr, "Couldn't allocate buffer memory!\n");
 *
 * pco_arm_camera(pco);
 * pco_start_recording(pco);
 *
 * const int n_images = 1;
 * fg, Fg_AcquireEx(fg, port, n_images, ACQ_STANDARD, mem);
 *
 * frameindex_t last_frame = 1;
 *
 * // For streaming cameras, this is unnecessary
 * pco_read_images(pco, active_segment, 1, 1);
 * pco_request_image(pco);
 *
 * // Request frame
 * last_frame = Fg_getLastPicNumberBlockingEx(fg, n_images, port, n_images, mem);
 *
 * // Request data
 * uint16_t *frame = (uint16_t *) Fg_getImagePtrEx(fg, last_frame, port, mem);
 * Fg_stopAcquireEx(fg, port, mem, STOP_ASYNC);
 * pco_stop_recording(pco);
 * \endcode
 *
 * \section api_reference API reference
 *
 * All function definitions can be found in libpco.h.
 *
 * \defgroup general General camera properties
 * \defgroup sensor Sensor properties
 * \defgroup recording Recording options
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <clser.h>
#include <time.h>

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

    uint32_t delay;
    uint32_t exposure;

    size_t extra_timeout;
};

#define CHECK_ERR_CL(code) \
    if ((code) != CL_OK)   \
        fprintf (stderr, "cl-error: %i at %s:%i\n", (code), __FILE__, __LINE__);

#define CHECK_PCO(code) \
    if ((code) != PCO_NOERROR) { \
        fprintf (stderr, "pco-error: %x at <%s:%i>\n", (code), __FILE__, __LINE__); \
    }

#define CHECK_PCO_AND_RETURN(code) {\
    CHECK_PCO (code);               \
    if ((code) != PCO_NOERROR)      \
        return (code);              \
    }

/* Courtesy of PCO AG */
static void
decode_line (int width, void *bufout, void* bufin)
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

static void
pco_reorder_image_5x12 (uint16_t *bufout, uint16_t *bufin, int width, int height)
{
    uint16_t *line_top = bufout;
    uint16_t *line_bottom = bufout + (height-1)*width;
    uint16_t *line_in = bufin;
    int off = (width*12) / 16;

    for (int y = 0; y < height/2; y++) {
        decode_line (width, line_top, line_in);
        line_in += off;
        decode_line (width, line_bottom, line_in);
        line_in += off;
        line_top += width;
        line_bottom -= width;
    }
}

static void
pco_reorder_image_5x16 (uint16_t *bufout, uint16_t *bufin, int width, int height)
{
    uint16_t *line_top = bufout;
    uint16_t *line_bottom = bufout + (height-1)*width;
    uint16_t *line_in = bufin;

    for (int y = 0; y < height/2; y++) {
        memcpy (line_top, line_in, width * sizeof(uint16_t));
        line_in += width;
        memcpy (line_bottom, line_in, width * sizeof(uint16_t));
        line_in += width;
        line_top += width;
        line_bottom -= width;
    }
}

static uint32_t
pco_build_checksum (unsigned char *buffer, int *size)
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

static uint32_t
pco_test_checksum (unsigned char *buffer, int *size)
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

    if (buffer[x] != sum)
        return PCO_ERROR_DRIVER_CHECKSUMERROR | PCO_ERROR_DRIVER_CAMERALINK;

    *size = x + 1;
    return PCO_NOERROR;
}

static void
pco_msleep (int time)
{
    fd_set rfds;
    struct timeval tv;

    FD_ZERO (&rfds);
    FD_SET (0, &rfds);

    tv.tv_sec = time / 1000;
    tv.tv_usec = (time % 1000) / 1000;
    select (0, NULL, NULL, NULL, &tv);
}

static uint16_t
pco_msb_pos (uint16_t x)
{
    uint16_t val = 0;

    while (x >>= 1)
        ++val;

    return val;
}

static void
pco_fill_binning_array (uint16_t *a, unsigned int n, int is_linear)
{
    if (is_linear) {
        for (int i = 0; i < n; i++)
            a[i] = i + 1;
    }
    else  {
        for (int i = 0, j = 1; i < n; i++, j *= 2)
            a[i] = j;
    }
}

static unsigned int
pco_get_num_binnings (uint16_t max_binning, int is_linear)
{
    /* In the linear case, we have 1 to max(bin) binnings, otherwise
     * this is log_2(max(bin)) + 1. */
    return is_linear ? max_binning : pco_msb_pos (max_binning) + 1;
}

static unsigned int
pco_set_delay_exposure (pco_handle pco, uint32_t delay, uint32_t exposure)
{
    SC2_Set_Delay_Exposure com;
    SC2_Delay_Exposure_Response resp;

    com.wCode = SET_DELAY_EXPOSURE_TIME;
    com.wSize = sizeof(SC2_Set_Delay_Exposure);
    com.dwDelay = delay;
    com.dwExposure = exposure;
    return pco_control_command (pco, &com, sizeof(com), &resp, sizeof(resp));
}

static unsigned int
pco_retrieve_cl_config (pco_handle pco)
{
    SC2_Simple_Telegram com;
    SC2_Get_CL_Configuration_Response resp;
    SC2_Get_Interface_Output_Format com_iface;
    SC2_Get_Interface_Output_Format_Response resp_iface;
    unsigned int err = PCO_NOERROR;

    com.wCode = GET_CL_CONFIGURATION;
    com.wSize = sizeof(SC2_Simple_Telegram);

    err = pco_control_command (pco, &com, sizeof(com), &resp, sizeof(resp));
    CHECK_PCO_AND_RETURN (err);

    pco->transfer.ClockFrequency = resp.dwClockFrequency;
    pco->transfer.CCline = resp.bCCline;
    pco->transfer.Transmit = resp.bTransmit;
    pco->transfer.DataFormat = resp.bDataFormat;

    com_iface.wCode = GET_INTERFACE_OUTPUT_FORMAT;
    com_iface.wSize = sizeof(com_iface);
    com_iface.wInterface = SET_INTERFACE_CAMERALINK;

    err = pco_control_command (pco, &com_iface, sizeof(com_iface), &resp_iface, sizeof(resp_iface));

    if (err == PCO_NOERROR)
        pco->transfer.DataFormat |= resp_iface.wFormat;

    return err;
}

static void
pco_update_baud_rate (pco_handle pco)
{
    SC2_Get_CL_Baudrate_Response resp;
    SC2_Set_CL_Baudrate req = {
        .wCode = SET_CL_BAUDRATE,
        .dwBaudrate = 115200,
        .wSize = sizeof(req)
    };

    if (pco_control_command (pco, &req, sizeof (req), &resp, sizeof (resp)) != PCO_NOERROR)
        CHECK_ERR_CL (clSetBaudRate (pco->serial_ref, CL_BAUDRATE_115200));
}

static unsigned int
pco_scan_and_set_baud_rate (pco_handle pco)
{
    unsigned baudrates[]  = {
        CL_BAUDRATE_921600, CL_BAUDRATE_460800, CL_BAUDRATE_230400,
        CL_BAUDRATE_115200, CL_BAUDRATE_57600, CL_BAUDRATE_38400,
        CL_BAUDRATE_19200, CL_BAUDRATE_9600, 0,
    };

    unsigned int err = PCO_NOERROR + 1;
    unsigned int supported;
    SC2_Camera_Type_Response resp;
    SC2_Simple_Telegram com = { .wCode = GET_CAMERA_TYPE, .wSize = sizeof (SC2_Simple_Telegram) };

    CHECK_ERR_CL (clGetSupportedBaudRates (pco->serial_ref, &supported));

    for (int i = 0; baudrates[i] != 0 && err != PCO_NOERROR; i++) {
        if (!(baudrates[i] & supported))
            continue;

        CHECK_ERR_CL (clSetBaudRate (pco->serial_ref, baudrates[i]));
        pco_msleep (100);
        err = pco_control_command (pco, &com, sizeof(com), &resp, sizeof(SC2_Camera_Type_Response));
    }

    return err;
}

static unsigned int
pco_read_property (pco_handle pco, uint16_t code, void *dst, uint32_t size)
{
    SC2_Simple_Telegram req = { .wCode = code, .wSize = sizeof(req) };
    return pco_control_command(pco, &req, sizeof(req), dst, size);
}

static unsigned int
pco_get_delay_exposure (pco_handle pco, uint32_t *delay, uint32_t *exposure)
{
    unsigned int err = PCO_NOERROR;
    SC2_Delay_Exposure_Response resp;

    if (pco_read_property(pco, GET_DELAY_EXPOSURE_TIME, &resp, sizeof(resp)) == PCO_NOERROR) {
        *delay = resp.dwDelay;
        *exposure = resp.dwExposure;
    }

    return err;
}

static unsigned int
pco_set_cl_config (pco_handle pco)
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

    err = pco_control_command (pco, &cl_com, sizeof(cl_com), &cl_resp, sizeof(cl_resp));

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
        err = pco_control_command (pco, &req, sizeof(req), &resp_if, sizeof(resp_if));
    }

    return err;
}

static int
pco_reset_serial (pco_handle pco)
{
    for (int i = 0; i < pco->num_ports; i++)
        clSerialClose(pco->serial_refs[i]);

    for (int i = 0; i < pco->num_ports; i++)
        CHECK_ERR_CL (clSerialInit(i, &pco->serial_refs[i]));

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
unsigned int
pco_control_command (pco_handle pco, void *buffer_in, uint32_t size_in, void *buffer_out, uint32_t size_out)
{
    unsigned char buffer[PCO_SC2_DEF_BLOCK_SIZE];
    unsigned int size;
    uint16_t com_in, com_out;
    uint32_t err = PCO_NOERROR;
    int cl_err = CL_OK;

    CHECK_ERR_CL (clFlushPort (pco->serial_ref));

    com_out = 0;
    com_in = *((uint16_t *) buffer_in);
    memset (buffer, 0, PCO_SC2_DEF_BLOCK_SIZE);

    size = size_in;
    err = pco_build_checksum ((unsigned char *) buffer_in, (int *) &size);

    if (err != PCO_NOERROR)
        fprintf (stderr, "Something happened... but is ignored in the original code\n");

    CHECK_ERR_CL (clSerialWrite (pco->serial_ref, (char *) buffer_in, &size, pco->timeouts.command));
    size = sizeof(uint16_t) * 2;

    pco_msleep(100);

    /* XXX: The pco.4000 needs at least 3 times the timeout which makes things
     * slow in the beginning. */
    cl_err = clSerialRead (pco->serial_ref, (char *) buffer, &size, pco->timeouts.command * 3 + pco->extra_timeout);

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

    CHECK_ERR_CL (clSerialRead (pco->serial_ref, (char *) &buffer[sizeof(uint16_t)*2], &size, pco->timeouts.command*2));

    if (cl_err < 0)
        return PCO_ERROR_DRIVER_IOFAILURE | PCO_ERROR_DRIVER_CAMERALINK;

    com_out = *((uint16_t *) buffer);

    if ((com_out & RESPONSE_ERROR_CODE) == RESPONSE_ERROR_CODE) {
        SC2_Failure_Response resp;
        memcpy (&resp, buffer, sizeof(SC2_Failure_Response));
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

    err = pco_test_checksum (buffer, (int *) &size);

    if (err == PCO_NOERROR) {
        size -= 1;

        if (size < size_out)
            size_out = size;

        memcpy (buffer_out, buffer, size_out);
    }

    return err;
}

static unsigned int
pco_get_rec_state (pco_handle pco, uint16_t *state)
{
    SC2_Recording_State_Response resp;
    SC2_Simple_Telegram req = { .wCode = GET_RECORDING_STATE, .wSize = sizeof(req) };
    unsigned int err = PCO_NOERROR;

    err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
    *state = resp.wState;
    return err;
}

static unsigned int
pco_set_rec_state (pco_handle pco, uint16_t state)
{
    const uint32_t REC_WAIT_TIME = 500;
    uint16_t g_state, x = 0;
    unsigned int err = PCO_NOERROR;
    uint32_t s, ns;
    SC2_Set_Recording_State com;
    SC2_Recording_State_Response resp;
    SC2_COC_Runtime_Response coc;

    com.wCode = SET_RECORDING_STATE;
    com.wState = state;
    com.wSize = sizeof(SC2_Set_Recording_State);

    err = pco_control_command (pco, &com, sizeof (SC2_Set_Recording_State), &resp, sizeof(SC2_Recording_State_Response));
    CHECK_PCO_AND_RETURN (err);

    err = pco_read_property (pco, GET_COC_RUNTIME, &coc, sizeof(coc));
    CHECK_PCO_AND_RETURN (err);
    s = coc.dwtime_s;
    ns = coc.dwtime_s;

    ns /= 1000000;
    ns += 1;
    ns += s*1000;

    ns += REC_WAIT_TIME;
    ns /= 50;

    for (x = 0; x < ns; x++) {
        err = pco_get_rec_state (pco, &g_state);
        CHECK_PCO_AND_RETURN (err);

        if (g_state == state)
            break;

        pco_msleep (1000);
    }

    if (x >= ns)
        err = PCO_ERROR_TIMEOUT;

    return err;
}

/**
 * \addtogroup general
 * @{
 */

/**
 * Read the name of the camera.
 *
 * @param pco A #pco_handle.
 * @param name Pointer to zero-terminated string containing the name. Memory is
 * allocated by the library and must be freed by the caller.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int
pco_get_name (pco_handle pco, char **name)
{
    SC2_Camera_Name_Response resp;
    unsigned int err = pco_read_property (pco, GET_CAMERA_NAME, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        char *s = (char *) malloc (40);
        strncpy(s, resp.szName, 40);
        *name = s;
    }
    else
        *name = NULL;

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
unsigned int
pco_get_camera_type (pco_handle pco, uint16_t *type, uint16_t *subtype)
{
    SC2_Camera_Type_Response resp;
    unsigned int err = pco_read_property (pco, GET_CAMERA_TYPE, &resp, sizeof(resp));

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
unsigned int
pco_get_camera_version (pco_handle pco, uint32_t *serial_number, uint16_t *hw_major, uint16_t *hw_minor, uint16_t *fw_major, uint16_t *fw_minor)
{
    SC2_Camera_Type_Response resp;
    unsigned int err = pco_read_property (pco, GET_CAMERA_TYPE, &resp, sizeof(resp));

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
unsigned int
pco_get_health_state (pco_handle pco, uint32_t *warnings, uint32_t *errors, uint32_t *status)
{
    SC2_Camera_Health_Status_Response resp;
    unsigned int err = pco_read_property (pco, GET_CAMERA_HEALTH_STATUS, &resp, sizeof(resp));

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
unsigned int
pco_reset (pco_handle pco)
{
    SC2_Reset_Settings_To_Default_Response resp;
    return pco_read_property(pco, RESET_SETTINGS_TO_DEFAULT, &resp, sizeof(resp));
}

/** @} */

/** \addtogroup sensor
 *  @{
 */

/**
 * Get format of sensor.
 *
 * @param pco A #pco_handle.
 * @param format Location for format of the sensor (SENSORFORMAT_EXTENDED,
 * SENSORFORMAT_STANDARD)
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_sensor_format (pco_handle pco, uint16_t *format)
{
    SC2_Sensor_Format_Response resp;
    unsigned int err = pco_read_property (pco, GET_SENSOR_FORMAT, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *format = resp.wFormat;

    return err;
}

/**
 * Set format of sensor.
 *
 * @param pco A #pco_handle.
 * @param format Format of the sensor:
 *     - SENSORFORMAT_EXTENDED: use all pixels inclusiding effective, dark,
 *       reference and dummy.
 *     - SENSORFORMAT_STANDARD: use only effective pixels.
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_sensor_format (pco_handle pco, uint16_t format)
{
    SC2_Set_Sensor_Format req = { .wCode = SET_SENSOR_FORMAT, .wSize = sizeof(req), .wFormat = format };
    SC2_Sensor_Format_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read temperature of different camera components.
 *
 * @param pco A #pco_handle.
 * @param ccd Temperature of the CCD sensor chip in degree celsius divided by 10.
 * @param camera Temperature of the camera in degree celsius.
 * @param power Temperature of the power supply in degree celsius.
 * @return Error code or PCO_NOERROR.
 * @note If a specific temperature sensor is not available, the values are
 * undefined.
 */
unsigned int
pco_get_temperature (pco_handle pco, int32_t *ccd, int32_t *camera, int32_t *power)
{
    SC2_Temperature_Response resp;
    unsigned int err = pco_read_property (pco, GET_TEMPERATURE, &resp, sizeof(resp));

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
unsigned int
pco_get_cooling_range (pco_handle pco, int16_t *default_temp, int16_t *min_temp, int16_t *max_temp)
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
unsigned int
pco_set_cooling_temperature (pco_handle pco, int16_t temperature)
{
    SC2_Set_Cooling_Setpoint req = { .wCode = SET_COOLING_SETPOINT_TEMPERATURE, .sTemp = temperature, .wSize = sizeof(req) };
    SC2_Cooling_Setpoint_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Get cooling temperature.
 *
 * @param pco A #pco_handle.
 * @param temperature Currently set target temperature of the CCD sensor.
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 * @see pco_get_cooling_range()
 */
unsigned int
pco_get_cooling_temperature (pco_handle pco, int16_t *temperature)
{
    SC2_Cooling_Setpoint_Response resp;
    unsigned int err = pco_read_property (pco, GET_COOLING_SETPOINT_TEMPERATURE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *temperature = resp.sTemp;

    return err;
}

/**
 * Read camera resolution.
 *
 * @param pco A #pco_handle.
 * @param width_std Standard width of sensor matrix.
 * @param height_std Standard height of sensor matrix.
 * @param width_ex Extended width of sensor matrix.
 * @param height_ex Extended height of sensor matrix.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int
pco_get_resolution (pco_handle pco, uint16_t *width_std, uint16_t *height_std, uint16_t *width_ex, uint16_t *height_ex)
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
unsigned int
pco_get_available_pixelrates (pco_handle pco, uint32_t rates[4], int *num_rates)
{
    int j = 0;
    for (int i = 0; i < 4; i++)
        if (pco->description.dwPixelRateDESC[i] > 0)
            rates[j++] = pco->description.dwPixelRateDESC[i];

    *num_rates = j;
    return PCO_NOERROR;
}

/**
 * Set analog-digital-converter (ADC) operation mode for reading the image
 * sensor data. Pixel data can be read out using one ADC (better linearity) or
 * in parallel using more ADCs (faster). Only available for some camera models
 * (defined in the camera description).
 *
 * @param pco A #pco_handle.
 * @param mode Operational mode of the ADCs
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_adc_mode (pco_handle pco, pco_adc_mode mode)
{
    SC2_Set_ADC_Operation req = {
        .wCode = SET_ADC_OPERATION, .wSize = sizeof(req),
        .wMode = (uint16_t) mode
    };
    SC2_ADC_Operation_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Get ADC operation mode.
 *
 * @param pco A #pco_handle.
 * @param mode Location for the operational mode of the ADCs
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_adc_mode (pco_handle pco, pco_adc_mode *mode)
{
    SC2_ADC_Operation_Response resp;
    unsigned int err = pco_read_property (pco, GET_ADC_OPERATION, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *mode = resp.wMode;

    return err;
}

/**
 * Get maximum number of ADCs
 *
 * @param pco A #pco_handle.
 * @return The number of ADCs that can be set with pco_set_adc_mode().
 * @since 0.3
 */
unsigned int
pco_get_maximum_number_of_adcs (pco_handle pco)
{
    return pco->description.wNumADCsDESC;
}

/**
 * Read current pixel rate.
 *
 * @param pco A #pco_handle.
 * @param rate Current pixel rate.
 * @return Error code or PCO_NOERROR.
 * @since 0.2.0
 * @see pco_set_pixelrate().
 */
unsigned int
pco_get_pixelrate (pco_handle pco, uint32_t *rate)
{
    SC2_Pixelrate_Response resp;
    unsigned int err = pco_read_property (pco, GET_PIXELRATE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *rate = resp.dwPixelrate;

    return err;
}

/**
 * Set pixel rate.
 *
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
unsigned int
pco_set_pixelrate (pco_handle pco, uint32_t rate)
{
    SC2_Set_Pixelrate req = { .wCode = SET_PIXELRATE, .wSize = sizeof(req), .dwPixelrate = rate };
    SC2_Pixelrate_Response resp;
    unsigned int err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        pco_reset_serial (pco);

    return err;
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
unsigned int
pco_get_available_conversion_factors (pco_handle pco, uint16_t factors[4], int *num_rates)
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
unsigned int
pco_is_active (pco_handle pco)
{
    SC2_Camera_Type_Response resp;
    return pco_read_property (pco, GET_CAMERA_TYPE, &resp, sizeof(resp)) == PCO_NOERROR;
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
unsigned int
pco_set_scan_mode (pco_handle pco, uint32_t mode)
{
    SC2_Set_Pixelrate com;
    SC2_Pixelrate_Response resp;
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

    com.wCode = SET_PIXELRATE;
    com.wSize = sizeof(SC2_Set_Pixelrate);
    com.dwPixelrate = pixel_clock;

    return pco_control_command (pco, &com, sizeof(SC2_Set_Pixelrate), &resp, sizeof(SC2_Pixelrate_Response));
}

/**
 * Get scan and readout mode.
 *
 * @param pco A #pco_handle.
 * @param mode PCO_SCANMODE_SLOW or PCO_SCANMODE_FAST
 * @return Error code or PCO_NOERROR.
 * @see pco_set_scan_mode()
 */
unsigned int
pco_get_scan_mode (pco_handle pco, uint32_t *mode)
{
    unsigned int err = PCO_NOERROR;
    SC2_Pixelrate_Response pixelrate;

    if ((err = pco_read_property (pco, GET_PIXELRATE, &pixelrate, sizeof(pixelrate))) == PCO_NOERROR) {
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

/** @} */

/**
 * \addtogroup recording
 * @{
 */

/**
 * Set trigger mode.
 *
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
unsigned int
pco_set_trigger_mode (pco_handle pco, uint16_t mode)
{
    SC2_Set_Trigger_Mode req = { .wCode = SET_TRIGGER_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Trigger_Mode_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
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
unsigned int
pco_get_trigger_mode (pco_handle pco, uint16_t *mode)
{
    SC2_Trigger_Mode_Response resp;
    unsigned int err = pco_read_property (pco, GET_TRIGGER_MODE, &resp, sizeof(resp));

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
unsigned int
pco_set_auto_transfer (pco_handle pco, int transfer)
{
    pco->transfer.Transmit = transfer ? 1 : 0;
    return pco_set_cl_config (pco);
}

/**
 * Return if automatic data transfer is currently used.
 *
 * @param pco A #pco_handle.
 * @param transfer 0 if no automatic transfer, else 1.
 * @return Error code or PCO_NOERROR.
 */
unsigned int
pco_get_auto_transfer (pco_handle pco, int *transfer)
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
unsigned int
pco_force_trigger (pco_handle pco, uint32_t *success)
{
    SC2_Force_Trigger_Response resp;
    unsigned int err = pco_read_property (pco, FORCE_TRIGGER, &resp, sizeof(resp));

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
    unsigned int err = pco_read_property (pco, GET_STORAGE_MODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *mode = resp.wMode;

    return err;
}

/**
 * Set storage mode.
 *
 * @param pco A #pco_handle.
 * @param mode Any of:
 *     - STORAGE_MODE_RECORDER:
 *         - images are recorded and stored within the camRam
 *         - “live view” transfers the most recent image to the PC
 *         - indexed or total readout of images after the recording has been
 *            stopped
 *     - STORAGE_MODE_FIFO_BUFFER:
 *         - all images taken are transferred to the PC in chronological order
 *         - camRAM is used as huge FIFO buffer to bypass short bottlenecks in
 *           data transmission. If buffer overflows the oldest images are
 *           overwritten. In FIFO buffer mode images are send directly to the
 *           PC interface like a continuous data stream. Synchronization is
 *           done with the interface.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int
pco_set_storage_mode(pco_handle pco, uint16_t mode)
{
    SC2_Set_Storage_Mode req = { .wCode = SET_STORAGE_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Storage_Mode_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read currently set record mode.
 *
 * @param pco A #pco_handle.
 * @param mode Location for the mode (RECORDER_SUBMODE_RINGBUFFER,
 * RECORDER_SUBMODE_SEQUENCE)
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_record_mode (pco_handle pco, uint16_t *mode)
{
    SC2_Recorder_Submode_Response resp;
    unsigned int err = pco_read_property (pco, GET_RECORDER_SUBMODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *mode = resp.wMode;

    return err;
}

/**
 * Set record mode.
 *
 * @param pco A #pco_handle.
 * @param mode Any of:
 *     - RECORDER_SUBMODE_RINGBUFFER: camera records continuously into ring
 *       buffer. If the allocated buffer is full, the oldest images are
 *       overwritten.
 *     - RECORDER_SUBMODE_SEQUENCE: recording is stopped when the allocated
 *       buffer is full
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_record_mode (pco_handle pco, uint16_t mode)
{
    SC2_Set_Recorder_Submode req = { .wCode = SET_RECORDER_SUBMODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Recorder_Submode_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Check whether the camera is recording.
 *
 * @param pco A #pco_handle.
 * @param is_recording Location for storing the information.
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_is_recording (pco_handle pco, bool *is_recording)
{
    uint16_t state;
    unsigned int err = pco_get_rec_state (pco, &state);

    if (err == PCO_NOERROR)
        *is_recording = state != 0;

    return err;
}

/**
 * Start recording.
 * The recording command controls the status of the camera. If the recording
 * state is [run], images can be released by exposure trigger and acquire
 * enable. If the recording state is [stop] all image readout or exposure
 * sequences are stopped and the sensors are running in a special idle mode to
 * prevent dark charge accumulation.
 *
 * @param pco A #pco_handle.
 * @return Error code or PCO_NOERROR.
 * @note Before starting any recording you have to call pco_arm_camera()!
 */
unsigned int
pco_start_recording (pco_handle pco)
{
    return pco_set_rec_state (pco, 1);
}

/**
 * Stop recording.
 *
 * @param pco A #pco_handle.
 * @return Error code or PCO_NOERROR.
 */
unsigned int
pco_stop_recording (pco_handle pco)
{
    return pco_set_rec_state (pco, 0);
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
unsigned int
pco_get_acquire_mode (pco_handle pco, uint16_t *mode)
{
    SC2_Acquire_Mode_Response resp;
    unsigned int err = pco_read_property (pco, GET_ACQUIRE_MODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *mode = resp.wMode;

    return err;
}

/**
 * Configure the mode of the (acq enbl) camera input.
 *
 * @param pco A #pco_handle.
 * @param mode Any of:
 *     - ACQUIRE_MODE_AUTO: the external control input (acq enbl) is ignored
 *     - ACQUIRE_MODE_EXTERNAL: if the external control input (acq enbl) is
 *       TRUE, then exposure triggers are accepted and images are taken. If
 *       this signal is FALSE, then all exposure triggers are ignored and the
 *       sensor readout is stopped.
 *     - ACQUIRE_MODE_EXTERNAL_FRAME_TRIGGER: TODO: what does this do?
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int
pco_set_acquire_mode (pco_handle pco, uint16_t mode)
{
    SC2_Set_Acquire_Mode req = { .wCode = SET_ACQUIRE_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Acquire_Mode_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/** @} */

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
unsigned int
pco_set_timestamp_mode (pco_handle pco, uint16_t mode)
{
    SC2_Timestamp_Mode_Response resp;
    SC2_Set_Timestamp_Mode com;
    com.wMode = mode;
    com.wCode = SET_TIMESTAMP_MODE;
    com.wSize = sizeof(com);
    return pco_control_command (pco, &com, sizeof(com), &resp, sizeof(resp));
}

/**
 * Get timestamp mode.
 *
 * @param pco A #pco_handle.
 * @param mode Locaton for the timestamp mode
 * @return Error code or PCO_NOERROR.
 */
unsigned int
pco_get_timestamp_mode (pco_handle pco, uint16_t *mode)
{
    SC2_Timestamp_Mode_Response resp;
    unsigned int err = pco_read_property (pco, GET_TIMESTAMP_MODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *mode = resp.wMode;

    return err;
}

/**
 * Set time scale of delay and exposure. Parameter values for delay and exposure can be:
 *    - TIMEBASE_NS
 *    - TIMEBASE_US
 *    - TIMEBASE_MS
 *
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
unsigned int
pco_set_timebase (pco_handle pco, uint16_t delay, uint16_t exposure)
{
    unsigned int err;
    SC2_Set_Timebase com;
    SC2_Timebase_Response resp;

    com.wCode = SET_TIMEBASE;
    com.wSize = sizeof(SC2_Set_Timebase);
    com.wTimebaseDelay = delay;
    com.wTimebaseExposure = exposure;
    err = pco_control_command (pco, &com, sizeof(com), &resp, sizeof(resp));
    pco_reset_serial (pco);
    return err;
}

/**
 * Read time scale of delay and exposure time.
 *
 * @param pco A #pco_handle
 * @param delay Location to store delay scale
 * @param exposure Location to store exposure scale
 * @return Error code or PCO_NOERROR
 */
unsigned int
pco_get_timebase (pco_handle pco, uint16_t *delay, uint16_t *exposure)
{
    SC2_Timebase_Response resp;
    unsigned int err = pco_read_property (pco, GET_TIMEBASE, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        *delay = resp.wTimebaseDelay;
        *exposure = resp.wTimebaseExposure;
    }

    return err;
}


/**
 * Get delay time in current time base.
 *
 * @param pco A #pco_handle.
 * @param delay Location to store delay in current time base
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_delay_time (pco_handle pco, uint32_t *delay)
{
    *delay = pco->delay;
    return PCO_NOERROR;
}

/**
 * Set delay time in current time base.
 *
 * @param pco A #pco_handle.
 * @param delay Delay time in current time base
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_delay_time (pco_handle pco, uint32_t delay)
{
    pco->delay = delay;
    return pco_set_delay_exposure (pco, delay, pco->exposure);
}

/**
 * Get delay time range for valid values that can be set with pco_set_delay().
 *
 * @param pco A #pco_handle.
 * @param min_ns Location to store minimum delay in nano seconds
 * @param max_ms Location to store maximum delay in milli seconds
 * @param step_ns Location to store steps in nano seconds
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_delay_range (pco_handle pco, uint32_t *min_ns, uint32_t *max_ms, uint32_t *step_ns)
{
    *min_ns = pco->description.dwMinDelayDESC;
    *max_ms = pco->description.dwMaxDelayDESC;
    *step_ns = pco->description.dwMinDelayStepDESC;
    return PCO_NOERROR;
}

/**
 * Get exposure time.
 *
 * @param pco A #pco_handle.
 * @param exposure Location to store exposure time in current time base
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_exposure_time (pco_handle pco, uint32_t *exposure)
{
    *exposure = pco->exposure;
    return PCO_NOERROR;
}

/**
 * Set exposure time in current time base.
 *
 * @param pco A #pco_handle.
 * @param exposure Exposure time in current time base
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_exposure_time (pco_handle pco, uint32_t exposure)
{
    pco->exposure = exposure;
    return pco_set_delay_exposure (pco, pco->delay, exposure);
}

/**
 * Get exposure time range.
 *
 * @param pco A #pco_handle.
 * @param min_ns Location to store minimum delay in nano seconds
 * @param max_ms Location to store maximum delay in milli seconds
 * @param step_ns Location to store steps in nano seconds
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_exposure_range (pco_handle pco, uint32_t *min_ns, uint32_t *max_ms, uint32_t *step_ns)
{
    *min_ns = pco->description.dwMinExposureDESC;
    *max_ms = pco->description.dwMaxExposureDESC;
    *step_ns = pco->description.dwMinExposureStepDESC;
    return PCO_NOERROR;
}

unsigned int
pco_set_framerate (pco_handle pco, uint32_t framerate_mhz, uint32_t exposure_ns, bool framerate_priority)
{
    SC2_Set_Framerate_Response resp;
    SC2_Set_Framerate req = {
        .wCode = SET_FRAMERATE, .wSize = sizeof(req),
        .dwFramerate = framerate_mhz,
        .dwExposure = exposure_ns,
        .wMode = framerate_priority ? SET_FRAMERATE_MODE_FRAMERATE_HAS_PRIORITY :
                                      SET_FRAMERATE_MODE_EXPTIME_HAS_PRIORITY
    };

    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

unsigned int
pco_get_framerate (pco_handle pco, uint32_t *framerate_mhz, uint32_t *exposure_ns)
{
    SC2_Get_Framerate_Response resp;
    unsigned int err = PCO_NOERROR;

    err = pco_read_property (pco, GET_FRAMERATE, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        *framerate_mhz = resp.dwFramerate;
        *exposure_ns = resp.dwExposure;
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
unsigned int
pco_set_roi (pco_handle pco, uint16_t *window)
{
    SC2_Set_ROI req = {
        .wCode = SET_ROI, .wSize = sizeof(req),
        .wROI_x0 = window[0], .wROI_y0 = window[1],
        .wROI_x1 = window[2], .wROI_y1 = window[3]
    };
    SC2_ROI_Response resp;

    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Get current region of interest window. The ROI is equal to or smaller than
 * the absolute image area which is defined by the settings of *format* and
 * *binning*.
 *
 * @param pco A #pco_handle.
 * @param window Four-element array
 * @return Error code or PCO_NOERROR.
 */
unsigned int
pco_get_roi (pco_handle pco, uint16_t *window)
{
    unsigned int err = PCO_NOERROR;
    SC2_ROI_Response resp;
    err = pco_read_property (pco, GET_ROI, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        window[0] = resp.wROI_x0;
        window[1] = resp.wROI_y0;
        window[2] = resp.wROI_x1;
        window[3] = resp.wROI_y1;
    }

    return err;
}

/**
 * Get number of steps that you can use to increase the region of interest.
 *
 * @param pco A #pco_handle.
 * @param horizontal Location to store number of horizontal ROI steps
 * @param vertical Location to store number of vertical ROI steps
 * @return Error code or PCO_NOERROR.
 */
unsigned int
pco_get_roi_steps (pco_handle pco, uint16_t *horizontal, uint16_t *vertical)
{
    *horizontal = pco->description.wRoiHorStepsDESC;
    *vertical = pco->description.wRoiVertStepsDESC;
    return PCO_NOERROR;
}

/**
 * Set binning.
 *
 * @param pco A #pco_handle.
 * @param horizontal Horizontal binning
 * @param vertical Vertical binning
 * @return Error code or PCO_NOERROR.
 *
 * Possible values for #horizontal and #vertical can be queried with
 * pco_get_possible_binning().
 *
 * @since 0.3
 */
unsigned int
pco_set_binning (pco_handle pco, uint16_t horizontal, uint16_t vertical)
{
    SC2_Set_Binning req = {
        .wCode = SET_BINNING, .wSize = sizeof(req),
        .wBinningx = horizontal, .wBinningy = vertical
    };

    SC2_Binning_Response resp;
    unsigned int err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));

    /*
     * For no apparent reason, communication stops after setting the binning.
     * Similar to pco_set_timebase() we have to reset the CameraLink connection.
     */
    pco_reset_serial (pco);
    return err;
}

/**
 * Get binning.
 *
 * @param pco A #pco_handle.
 * @param horizontal Location to store horizontal binning information
 * @param vertical Location to store vertical binning information
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_binning (pco_handle pco, uint16_t *horizontal, uint16_t *vertical)
{
    SC2_Binning_Response resp;
    unsigned int err = pco_read_property (pco, GET_BINNING, &resp, sizeof(resp));

    if (err == PCO_NOERROR) {
        *horizontal = resp.wBinningx;
        *vertical = resp.wBinningy;
    }

    return err;
}

/**
 * Get possible binning modes.
 *
 * @param pco A #pco_handle.
 * @param horizontal Location to store horizontal binning information. The
 *      pointer should be initialized with NULL.
 * @param num_horizontal Location for the number of elements in #horizontal
 * @param vertical Location to store vertical binning information. The pointer
 *      should be initialized with NULL.
 * @param num_vertical Location for the number of elements in #vertical
 * @return Error code or PCO_NOERROR.
 *
 * @note Both horizontal and vertical should point to NULL because memory for the
 * data is allocated by this function. This memory must be freed by the caller.
 *
 * @since 0.3
 */
unsigned int
pco_get_possible_binnings(pco_handle pco, uint16_t **horizontal, unsigned int *num_horizontal, uint16_t **vertical, unsigned int *num_vertical)
{
    unsigned int num_h = pco_get_num_binnings (pco->description.wMaxBinHorzDESC, pco->description.wBinHorzSteppingDESC);
    uint16_t *r_horizontal = (uint16_t *) malloc (num_h * sizeof(uint16_t));
    pco_fill_binning_array (r_horizontal, num_h, pco->description.wBinHorzSteppingDESC);

    unsigned int num_v = pco_get_num_binnings (pco->description.wMaxBinVertDESC, pco->description.wBinVertSteppingDESC);
    uint16_t *r_vertical = (uint16_t *) malloc (num_v * sizeof(uint16_t));
    pco_fill_binning_array (r_vertical, num_v, pco->description.wBinVertSteppingDESC);

    *horizontal = r_horizontal;
    *vertical = r_vertical;
    *num_horizontal = num_h;
    *num_vertical = num_v;

    return PCO_NOERROR;
}

/**
 * Check double-image mode availability
 *
 * @param pco A #pco_handle.
 * @return TRUE if double image mode is available
 * @since 0.3
 */
bool
pco_is_double_image_mode_available (pco_handle pco)
{
    return pco->description.wDoubleImageDESC == 1;
}

/**
 * Enable or disable double image mode
 *
 * @param pco A #pco_handle.
 * @param on TRUE to enable double image mode or FALSE
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_double_image_mode (pco_handle pco, bool on)
{
     SC2_Set_Double_Image_Mode req = { .wCode = SET_DOUBLE_IMAGE_MODE, .wMode = on ? 1 : 0, .wSize = sizeof(req) };
     SC2_Double_Image_Mode_Response resp;
     return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read double image mode status. Some cameras (defined in the camera
 * description) allow to make a double image with two exposures separated by a
 * short interleaving time.
 *
 * @param pco A #pco_handle.
 * @param on Location to store status
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_double_image_mode (pco_handle pco, bool *on)
{
    SC2_Double_Image_Mode_Response resp;
    unsigned int err = pco_read_property (pco, GET_DOUBLE_IMAGE_MODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *on = resp.wMode == 1;

    return err;
}

/**
 * Enable or disable offset mode
 *
 * @param pco A #pco_handle.
 * @param on TRUE to enable offset mode or FALSE
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_offset_mode (pco_handle pco, bool on)
{
    /* Yes, indeed. 0 means AUTO and 1 means OFF */
    SC2_Set_Offset_Mode req = { .wCode = SET_OFFSET_MODE, .wMode = on ? 0 : 1, .wSize = sizeof(req) };
    SC2_Offset_Mode_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read offset mode status
 *
 * @param pco A #pco_handle.
 * @param on Location to store status
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_offset_mode (pco_handle pco, bool *on)
{
    SC2_Offset_Mode_Response resp;
    unsigned int err = pco_read_property (pco, GET_OFFSET_MODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *on = resp.wMode == 0;

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
unsigned int
pco_set_hotpixel_correction (pco_handle pco, uint32_t mode)
{
    SC2_Set_Hot_Pixel_Correction_Mode com;
    SC2_Hot_Pixel_Correction_Mode_Response resp;

    com.wCode = SET_HOT_PIXEL_CORRECTION_MODE;
    com.wSize = sizeof(com);
    com.wMode = mode;
    return pco_control_command (pco, &com, sizeof(com), &resp, sizeof(resp));
}

/**
 * Read noise filter mode
 *
 * @param pco A #pco_handle.
 * @param mode Location to store mode (NOISE_FILTER_MODE_OFF,
 * NOISE_FILTER_MODE_ON).
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_get_noise_filter_mode (pco_handle pco, uint16_t *mode)
{
    SC2_Noise_Filter_Mode_Response resp;
    unsigned int err = pco_read_property (pco, GET_NOISE_FILTER_MODE, &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *mode = resp.wMode;

    return err;
}

/**
 * Set noise filter mode
 *
 * @param pco A #pco_handle.
 * @param mode Noise filter mode (NOISE_FILTER_MODE_OFF,
 * NOISE_FILTER_MODE_ON).
 * @return Error code or PCO_NOERROR.
 * @since 0.3
 */
unsigned int
pco_set_noise_filter_mode (pco_handle pco, uint16_t mode)
{
    SC2_Set_Noise_Filter_Mode req = { .wCode = SET_NOISE_FILTER_MODE, .wSize = sizeof(req), .wMode = mode };
    SC2_Noise_Filter_Mode_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Prepare camera for recording.
 *
 * @param pco A #pco_handle.
 * @return Error code or PCO_NOERROR.
 * @note This method must be called before starting the recording.
 */
unsigned int
pco_arm_camera (pco_handle pco)
{
    unsigned int err;
    SC2_Simple_Telegram req;
    SC2_Arm_Camera_Response resp;

    req.wCode = ARM_CAMERA;
    req.wSize = sizeof(SC2_Simple_Telegram);
    pco->extra_timeout = 5000;
    err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
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
unsigned int
pco_get_num_images (pco_handle pco, uint16_t segment, uint32_t *num_images)
{
    SC2_Number_of_Images req = { .wCode = GET_NUMBER_OF_IMAGES_IN_SEGMENT, .wSize = sizeof(req), .wSegment = segment };
    SC2_Number_of_Images_Response resp;
    unsigned int err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *num_images = resp.dwValid;

    return err;
}

/**
 * Trigger image request and transfer. Start an image transfer, while
 * recording state is set to [on]. If storage mode is set to [recorder], the
 * last aquired image is read. If storage mode is set to [FIFO buffer mode]
 * the images are read in the order in which they have been written into the
 * fifo buffer.
 *
 * @param pco A #pco_handle
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int
pco_request_image (pco_handle pco)
{
    SC2_Request_Image req = { .wCode = REQUEST_IMAGE, .wSize = sizeof(req) };
    SC2_Request_Image_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read images from a segment of the camera RAM. The command is only
 * valid if storage mode is set to [recorder] and recording to the camRAM
 * segment is stopped. This method is only useful for cameras with
 * on-board memory that can be read out after recording.
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
unsigned int
pco_read_images (pco_handle pco, uint16_t segment, uint32_t start, uint32_t end)
{
    SC2_Read_Images_from_Segment req = {
        .wCode = READ_IMAGES_FROM_SEGMENT,
        .wSize = sizeof(req),
        .wSegment = segment,
        .dwStartImage = start,
        .dwLastImage = end
    };
    SC2_Read_Images_from_Segment_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
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
unsigned int
pco_get_segment_sizes (pco_handle pco, size_t sizes[4])
{
    SC2_Camera_RAM_Segment_Size_Response resp;
    unsigned int err = pco_read_property (pco, GET_CAMERA_RAM_SEGMENT_SIZE, &resp, sizeof(resp));

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
unsigned int
pco_get_active_segment (pco_handle pco, uint16_t *segment)
{
    SC2_Active_RAM_Segment_Response resp;
    unsigned int err = pco_read_property (pco, GET_ACTIVE_RAM_SEGMENT, &resp, sizeof(resp));

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
unsigned int
pco_clear_active_segment (pco_handle pco)
{
    SC2_Simple_Telegram req = { .wCode = CLEAR_RAM_SEGMENT, .wSize = sizeof(req) };
    SC2_Clear_RAM_Segment_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Read bit alignment of data.
 *
 * @param pco A #pco_handle
 * @param msb_aligned True if data is aligned on most significant bit.
 * @return Error code or PCO_NOERROR.
 * @since 0.2
 */
unsigned int
pco_get_bit_alignment (pco_handle pco, bool *msb_aligned)
{
    SC2_Bit_Alignment_Response resp;
    unsigned int err = pco_read_property (pco, GET_BIT_ALIGNMENT, &resp, sizeof(resp));

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
unsigned int
pco_set_bit_alignment (pco_handle pco, bool msb_aligned)
{
    SC2_Set_Bit_Alignment req = {
        .wCode = SET_BIT_ALIGNMENT,
        .wSize = sizeof(req),
        .wAlignment = msb_aligned ? 0 : 1
    };
    SC2_Bit_Alignment_Response resp;
    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Return size of read-out data in pixels.
 *
 * @param pco A #pco_handle
 * @param width Width of the frame.
 * @param height Height of the frame.
 * @return Error code or PCO_NOERROR.
 */
unsigned int
pco_get_actual_size (pco_handle pco, uint32_t *width, uint32_t *height)
{
   unsigned int err = PCO_NOERROR;

   SC2_Simple_Telegram com;
   SC2_ROI_Response resp;
   com.wCode = GET_ROI;
   com.wSize = sizeof(SC2_Simple_Telegram);
   err = pco_control_command (pco, &com, sizeof(com), &resp, sizeof(resp));

   if (err == PCO_NOERROR) {
       *width = resp.wROI_x1 - resp.wROI_x0 + 1;
       *height = resp.wROI_y1 - resp.wROI_y0 + 1;
   }

   return err;
}

/**
 * Get shutter setting for pco.edge cameras.
 *
 * @param pco A #pco_handle
 * @param shutter Location for shutter mode.
 * @return Error code or PCO_NOERROR
 */
unsigned int
pco_edge_get_shutter (pco_handle pco, pco_edge_shutter *shutter)
{
   unsigned int err = PCO_NOERROR;

    SC2_Simple_Telegram req = {
        .wCode = GET_CAMERA_SETUP,
        .wSize = sizeof(SC2_Simple_Telegram),
    };
    SC2_Get_Camera_Setup_Response resp;

    err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));

    if (err == PCO_NOERROR)
        *shutter = resp.dwSetupFlags[0];

    return err;
}

/**
 * Set shutter setting for pco.edge cameras.
 *
 * @param pco A #pco_handle
 * @param shutter Shutter mode.
 * @return Error code or PCO_NOERROR
 */
unsigned int
pco_edge_set_shutter (pco_handle pco, pco_edge_shutter shutter)
{
    unsigned int err = PCO_NOERROR;

    SC2_Set_Camera_Setup req = {
        .wCode = SET_CAMERA_SETUP,
        .wSize = sizeof(SC2_Set_Camera_Setup),
        .wType = 0,
    };

    SC2_Set_Camera_Setup_Response resp;

    for (int i = 0; i < NUMSETUPFLAGS; i++)
        req.dwSetupFlags[i] = shutter;

    pco->extra_timeout = 5000;
    err = pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
    pco->extra_timeout = 0;
    return err;
}

/**
 * Set current date and time for use in time stamp mode
 *
 * @param pco A #pco_handle
 * @return Error code or PCO_NOERROR
 */
unsigned int
pco_set_date_time (pco_handle pco)
{
    time_t current_time = time(NULL);
    struct tm ct = *localtime (&current_time);

    SC2_Set_Date_Time req = {
        .wCode = SET_DATE_TIME,
        .wSize = sizeof(SC2_Set_Date_Time),
        .bDay = ct.tm_mday,
        .bMonth = ct.tm_mon+1,
        .wYear = ct.tm_year+1900,
        .wHours = ct.tm_hour,
        .bMinutes = ct.tm_min,
        .bSeconds = ct.tm_sec,
    };

    SC2_Date_Time_Response resp;

    return pco_control_command (pco, &req, sizeof(req), &resp, sizeof(resp));
}

/**
 * Return the currently used re-order function.
 *
 * @param pco A #pco_handle
 * @return Pointer to a #pco_reorder_image_t function.
 */
pco_reorder_image_t
pco_get_reorder_func (pco_handle pco)
{
    return pco->reorder_image;
}

/**
 * Initialize a PCO camera.
 *
 * @return An initialized #pco_handle or NULL.
 */
pco_handle
pco_init (void)
{
    pco_handle pco;
    uint16_t type;
    uint16_t subtype;

    pco = (pco_handle) malloc (sizeof(struct pco_t));

    if (pco == NULL)
        return NULL;

    memset (pco, 0, sizeof (struct pco_t));

    pco->reorder_image = &pco_reorder_image_5x16;
    pco->timeouts.command = PCO_SC2_COMMAND_TIMEOUT;
    pco->timeouts.image = PCO_SC2_IMAGE_TIMEOUT_L;
    pco->timeouts.transfer = PCO_SC2_COMMAND_TIMEOUT;
    pco->extra_timeout = 0;

    for (int i = 0; i < 4; i++)
        pco->serial_refs[i] = NULL;

    if (clGetNumSerialPorts (&pco->num_ports) != CL_OK) {
        fprintf (stderr, "Unable to query number of ports\n");
        goto no_pco;
    }

    if (pco->num_ports == 0)
        goto no_pco;

    if (pco->num_ports > 4)
        pco->num_ports = 4;

    for (int i = 0; i < pco->num_ports; i++) {
        if (clSerialInit(i, &pco->serial_refs[i]) != CL_OK) {
            fprintf (stderr, "Unable to initialize serial connection\n");
            goto no_pco;
        }
    }

    /* Reference the first port for easier access */
    pco->serial_ref = pco->serial_refs[0];

    if (pco_scan_and_set_baud_rate (pco) != PCO_NOERROR) {
        fprintf (stderr, "Unable to scan and set baud rate\n");
        goto no_pco;
    }

    if (pco_get_delay_exposure (pco, &pco->delay, &pco->exposure)) {
        fprintf (stderr, "Unable to read default delay and exposure time\n");
        goto no_pco;
    }

    if (pco_set_rec_state (pco, 0))
        goto no_pco;

    if (pco_retrieve_cl_config (pco))
        goto no_pco;

    /* Okay pco. You like to torture me. With insane default settings. */
    pco_set_bit_alignment (pco, false);
    /* Date and time should be set once when the camera in turned on. They are updated as long as the camera is supplied with power. */
    pco_set_date_time (pco);

    if (pco_read_property (pco, GET_CAMERA_DESCRIPTION, &pco->description, sizeof(pco->description)) != PCO_NOERROR)
        goto no_pco;

    /* Update baud rate in case of dimax */
    if (pco_get_camera_type (pco, &type, &subtype) != PCO_NOERROR)
        goto no_pco;

    if (type == CAMERATYPE_PCO_DIMAX_STD) {
        pco_update_baud_rate (pco);
        pco_retrieve_cl_config (pco);
        pco->transfer.DataFormat = PCO_CL_DATAFORMAT_2x12;
        pco_set_cl_config (pco);
    }

    return pco;

no_pco:
    free (pco);
    return NULL;
}

/**
 * Close pco device.
 *
 * @param pco A #pco_handle.
 */
void
pco_destroy (pco_handle pco)
{
    pco_set_rec_state (pco, 0);

    for (int i = 0; i < pco->num_ports; i++)
        clSerialClose (pco->serial_refs[i]);

    free (pco);
}

