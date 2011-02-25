#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "libpco.h"
#include "sc2_defs.h"

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

/* Courtesy of PCO AG */
void decode_line(int width, void *bufout, void* bufin)
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

void reorder_image_5x12(uint16_t *bufout, uint16_t *bufin, int width, int height)
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

void reorder_image_5x16(uint16_t *bufout, uint16_t *bufin, int width, int height)
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

int64_t time_diff(struct timeval *end, struct timeval *start)
{
    return ((end->tv_sec * 1000000) + end->tv_usec) - ((start->tv_sec * 1000000) + start->tv_usec);
}

int main(int argc, char const* argv[])
{
    static const char *applet = "libFullAreaGray8.so";

    /* CameraLink specific */
    printf("--- CameraLink ---------\n");
    struct pco_edge_t *pco = pco_init();

    unsigned int buffer_size = 256, version;
    char str[buffer_size];

    printf(" Ports: %i\n", pco->num_ports);
    for (int i = 0; i < pco->num_ports; i++) {
        check_error_cl(clGetSerialPortIdentifier(i, str, &buffer_size));
        printf("  Port Identifier (Port %i): %s\n", i, str);
        buffer_size = 256;
    }

    check_error_cl(clGetManufacturerInfo(str, &buffer_size, &version));
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

    SC2_Camera_Name_Response name;
    if (pco_read_property(pco, GET_CAMERA_NAME, &name, sizeof(name)) == PCO_NOERROR)
        printf("\n Camera name: %s\n", name.szName);

    printf(" Available pixel clocks\n");
    for (int i = 0; i < 4; i++) {
        if (pco->description.dwPixelRateDESC[i] > 0)
            printf("  Pixelclock %i: %.2f MHz\n", i+1, pco->description.dwPixelRateDESC[i] / 1000000.0f);
    }

    printf(" Clock frequency: %i MHz\n", pco->transfer.ClockFrequency/1000000);
    printf(" Data format: 0x0%x\n", (pco->transfer.DataFormat & PCO_CL_DATAFORMAT_MASK));
    printf(" Transmit continuously: %s\n", (pco->transfer.Transmit ? "yes" : "no"));

    SC2_Temperature_Response temperature;
    if (pco_read_property(pco, GET_TEMPERATURE, &temperature, sizeof(temperature)) == PCO_NOERROR) {
        printf(" CCD temperature: %1.2f°C\n", temperature.sCCDtemp / 10.0f);
        printf(" Camera temperature: %i°C\n", temperature.sCamtemp);
        printf(" Power supply temperature: %i°C\n", temperature.sPStemp);
    }

    pco_set_delay_exposure(pco, 0, 5);
    SC2_Delay_Exposure_Response de;
    if (pco_read_property(pco, GET_DELAY_EXPOSURE_TIME, &de, sizeof(de)) == PCO_NOERROR) {
        printf(" Delay: %u µs\n", (uint32_t) de.dwDelay);
        printf(" Exposure: %u µs\n", (uint32_t) de.dwExposure);
    }
   
    SC2_Pixelrate_Response pixelrate;
    if (pco_read_property(pco, GET_PIXELRATE, &pixelrate, sizeof(pixelrate)) == PCO_NOERROR)
        printf(" Pixel rate: %i\n", pixelrate.dwPixelrate);

    /* Setup for recording */
    if (pco_set_timestamp_mode(pco, 0) != PCO_NOERROR)
        PCO_ERROR_LOG("SET TIMESTAMP failed");

    if (pco_set_timebase(pco, 1, 1) != PCO_NOERROR)
        PCO_ERROR_LOG("SET TIMEBASE failed");

    pco->transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x16;
    if (pco_set_cl_config(pco) != PCO_NOERROR)
        PCO_ERROR_LOG("Setting CameraLink config failed");

    if (pco_arm_camera(pco) != PCO_NOERROR)
        PCO_ERROR_LOG("Couldn't ARM camera\n");

    uint32_t width, height;
    if (pco_get_actual_size(pco, &width, &height) == PCO_NOERROR) {
        printf(" Dimensions: %ix%i\n", width, height);
    }

    /* Frame grabber specific */
    Fg_Struct * fg;

    fg = Fg_Init(applet, 0);

    int val = FG_CL_8BIT_FULL_10;
    check_error_fg(fg, Fg_setParameter(fg, FG_CAMERA_LINK_CAMTYP, &val, PORT_A));

    val = FG_GRAY;
    check_error_fg(fg, Fg_setParameter(fg, FG_FORMAT, &val, PORT_A));

    val = FREE_RUN;
    check_error_fg(fg, Fg_setParameter(fg, FG_TRIGGERMODE, &val, PORT_A));

    width *= 2;

    check_error_fg(fg, Fg_setParameter(fg, FG_WIDTH, &width, PORT_A));
    check_error_fg(fg, Fg_setParameter(fg, FG_HEIGHT, &height, PORT_A));
    printf(" Actual dimensions: %ix%i\n", width, height);

    width /= 2;

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
    printf("\n--- Grabbing on Port A\n");
    const int n_buffers = 20;

    dma_mem *mem = Fg_AllocMemEx(fg, n_buffers*width*height*sizeof(uint16_t), n_buffers);
    if (mem == NULL) {
        printf(" Couldn't allocate buffer memory\n");
    }

    const int n_images = 20;

    pco_set_rec_state(pco, 1);
    printf(" Acquire %d image(s)...", n_images);
    fflush(stdout);
    check_error_fg(fg, Fg_AcquireEx(fg, 0, n_images, ACQ_STANDARD, mem));

    frameindex_t last_frame = 1;
    struct timeval start, end;

    gettimeofday(&start, NULL);
    last_frame = Fg_getLastPicNumberBlockingEx(fg, n_images, PORT_A, n_images, mem);
    gettimeofday(&end, NULL);
    check_error_fg(fg, Fg_stopAcquireEx(fg, 0, mem, STOP_ASYNC));
    printf(" done.\n");

    if (last_frame < 0) {
        printf(" Timed out\n");
    }
    else {
        long transfered_bytes = n_images * width * height * 2;
        uint64_t elapsed = time_diff(&end, &start);
        printf(" Time: %.2f s\n", (float)(elapsed / 1000000.0f));
        printf(" Bandwidth: %.3f MB/s\n", (transfered_bytes / (1024*1024)) / (elapsed / 1000000.0f));
        printf(" Frame rate: %.2f Frames/s\n", n_images / (elapsed / 1000000.0f));

        uint16_t *frame = (uint16_t *) Fg_getImagePtrEx(fg, last_frame, PORT_A, mem);
        uint16_t *image = (uint16_t *) malloc(width*height*2);
        if ((pco->transfer.DataFormat & PCO_CL_DATAFORMAT_MASK) == PCO_CL_DATAFORMAT_5x12)
            reorder_image_5x12(image, frame, width, height);
        else
            reorder_image_5x16(image, frame, width, height);

        FILE *fp = fopen("out.raw", "wb");
        fwrite(image, width*height*2, 1, fp);
        fclose(fp);
        free(image);
    }

    /* Close CameraLink interfaces and frame grabber */
    check_error_fg(fg, Fg_FreeMemEx(fg, mem));
    Fg_FreeGrabber(fg);

    pco_destroy(pco);
    
    return 0;
}
