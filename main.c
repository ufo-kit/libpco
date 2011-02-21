#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libpco.h"

#include "fgrab_struct.h"
#include "fgrab_prototyp.h"

#include "clser.h"

void check_error_fg(Fg_Struct *fg, int code)
{
    if (code != FG_OK)
        printf("Error [%i]: %s\n", Fg_getLastErrorNumber(fg), Fg_getLastErrorDescription(fg));
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

void pcofg_set_size(struct pco_edge_t *pco, Fg_Struct *fg, int width, int height)
{
    int w = width;
    uint32_t format = pco->transfer.DataFormat & PCO_CL_DATAFORMAT_MASK; 

    /* FIXME: The following code is in the original code also. However, w is
     * never used again after assignment.*/
    switch (format) {
        case PCO_CL_DATAFORMAT_4x16:
        case PCO_CL_DATAFORMAT_5x16:
            w *= 2;
            break;
        case PCO_CL_DATAFORMAT_5x12:
            w *= 12;
            w /= 16;
            break;
    }
}

int main(int argc, char const* argv[])
{
    static const char *applet = "libFullAreaGray8.so";

    /* CameraLink specific */
    printf("--- CameraLink ---------\n");
    struct pco_edge_t *pco = pco_init();

    unsigned int buffer_size, version, err;
    char str[256];

    printf(" Ports: %i\n", pco->num_ports);
    for (int i = 0; i < pco->num_ports; i++) {
        check_error_cl(clGetSerialPortIdentifier(i, str, &buffer_size));
        printf("  Port Identifier (Port %i): %s\n", i, str);
    }

    clGetManufacturerInfo(str, &buffer_size, &version);
    printf(" Manufacturer: %s\n", str);
    printf(" Version: %x\n", version);
    
    /* Query properties and output them */
    printf("\n--- Camera ----------\n");
    printf(" Active: ");
    if (!pco_active(pco)) {
        printf("no\n");
        pco_destroy(pco);
        return 1;
    }
    else
        printf("yes\n");

    printf(" Scanning for baud rate... ");
    fflush(stdout);
    pco_scan_and_set_baud_rate(pco);
    printf("using %i Bd/s\n", pco->baud_rate);

    SC2_Camera_Name_Response name;
    if (pco_read_property(pco, GET_CAMERA_NAME, &name, sizeof(name)) == PCO_NOERROR)
        printf("\n Camera name: %s\n", name.szName);

    err = pco_read_property(pco, GET_CAMERA_DESCRIPTION, &pco->description, sizeof(pco->description));
    if (err != PCO_NOERROR)
        PCO_ERROR_LOG("GET_CAMERA_DESCRIPTION failed");

    if (pco_retrieve_cl_config(pco) == PCO_NOERROR)
        printf(" Clock frequency: %i MHz\n", pco->transfer.ClockFrequency/1000000);

    SC2_Temperature_Response temperature;
    if (pco_read_property(pco, GET_TEMPERATURE, &temperature, sizeof(temperature)) == PCO_NOERROR) {
        printf(" CCD temperature: %i°C\n", temperature.sCCDtemp);
        printf(" Camera temperature: %i°C\n", temperature.sCamtemp);
        printf(" Power supply temperature: %i°C\n", temperature.sPStemp);
    }

    pco_set_delay_exposure(pco, 1000, 5000);
    SC2_Delay_Exposure_Response de;
    if (pco_read_property(pco, GET_DELAY_EXPOSURE_TIME, &de, sizeof(de)) == PCO_NOERROR) {
        printf(" Delay: %u µs\n", (uint32_t) de.dwDelay);
        printf(" Exposure: %u µs\n", (uint32_t) de.dwExposure);
    }
   
    SC2_Pixelrate_Response pixelrate;
    if (pco_read_property(pco, GET_PIXELRATE, &pixelrate, sizeof(pixelrate)) == PCO_NOERROR)
        printf(" Pixel rate: %i\n", pixelrate.dwPixelrate);

    /* Setup for recording */
    if (pco_set_rec_state(pco, 0) != PCO_NOERROR)
        PCO_ERROR_LOG("SET RECORDING STATE failed");

    if (pco_set_timestamp_mode(pco, 2) != PCO_NOERROR)
        PCO_ERROR_LOG("SET TIMESTAMP failed");

    if (pco_set_timebase(pco, 1, 1) != PCO_NOERROR)
        PCO_ERROR_LOG("SET TIMEBASE failed");

    /* XXX: here followed pco_set_delay_exposure() with unknown values */

    if (pco->transfer.DataFormat != (SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER|PCO_CL_DATAFORMAT_5x12)) {
        pco->transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x12;

        if (pco_set_cl_config(pco) != PCO_NOERROR)
            PCO_ERROR_LOG("Setting CameraLink config failed");
    }

    if (pco_arm_camera(pco) != PCO_NOERROR)
        PCO_ERROR_LOG("Couldn't ARM camera\n");

    uint32_t width, height;
    if (pco_get_actual_size(pco, &width, &height) == PCO_NOERROR) {
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

    check_error_fg(fg, Fg_setParameter(fg, FG_WIDTH, &width, PORT_A));
    check_error_fg(fg, Fg_setParameter(fg, FG_HEIGHT, &height, PORT_A));
    printf(" Actual dimensions: %ix%i\n", width, height);

    dma_mem *mem = Fg_AllocMemEx(fg, width*height*sizeof(uint16_t), 4);
    if (mem == NULL) {
        printf(" Couldn't allocate buffer memory\n");
    }

    pco_set_rec_state(pco, 1);
    sleep(1);
    printf(" Acquire image...");
    fflush(stdout);
    check_error_fg(fg, Fg_AcquireEx(fg, 0, 4, ACQ_STANDARD, mem));
    frameindex_t last_frame = Fg_getLastPicNumberBlockingEx(fg, 4, PORT_A, 5, mem);
    printf(" done.\n");
    if (last_frame < 0) {
        printf(" Couldn't retrieve last frame\n");
    }
    else {
        uint16_t *frame = (uint16_t *) Fg_getImagePtrEx(fg, 1, PORT_A, mem);
        FILE *fp = fopen("out.raw", "wb");
        fwrite(frame, 2*width*height, 1, fp);
        fclose(fp);
    }

    /* Close CameraLink interfaces and frame grabber */
    check_error_fg(fg, Fg_FreeMemEx(fg, mem));
    Fg_FreeGrabber(fg);

    pco_destroy(pco);
    
    return 0;
}
