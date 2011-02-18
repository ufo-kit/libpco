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
    PCO_SC2_CL_TRANSFER_PARAM_I transfer;
    SC2_Camera_Description_Response description;
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

    /* reference the first port for easier access */
    pco.serial_ref = pco.serial_refs[0];
    printf(" scanning for baud rate... ");
    fflush(stdout);
    pco_retrieve_baud_rate(&pco);
    printf("index %i\n", pco.baud_rate);

    SC2_Simple_Telegram com;
    com.wCode = GET_CAMERA_DESCRIPTION;
    com.wSize = sizeof(SC2_Simple_Telegram);
    err = pco_control_command(&pco, &com, sizeof(com), &pco.description, sizeof(SC2_Camera_Type_Response));
    if (err != PCO_NOERROR)
        PCO_ERROR_LOG("GET_CAMERA_DESCRIPTION failed");

    if (pco_retrieve_cl_config(&pco) == PCO_NOERROR)
        printf(" Clock frequency: %i\n", pco.transfer.ClockFrequency);

    for (int i = 0; (i < 4) && (pco.serial_refs[i] != NULL); i++)
        clSerialClose(pco.serial_refs[i]);

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

    
    return 0;
}
