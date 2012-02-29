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

#define CHECK_FG(fg, code) if ((code) != FG_OK) \
    fprintf(stderr, "FG-error at %s:%i: %i\n", __FILE__, __LINE__, Fg_getLastErrorNumber((fg)));

#define CHECK_PCO(err) if ((err) != PCO_NOERROR) fprintf(stderr, "PCO-error at %s:%i\n", __FILE__, __LINE__);

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

struct pco_types {
    uint32_t id;
    const char *name;
};

static struct pco_types pco_cameras[] = {
    { CAMERATYPE_PCO1200HS, "pco.1200 HS" },
    { CAMERATYPE_PCO1300, "pco.1300" },
    { CAMERATYPE_PCO1600, "pco.1600" },
    { CAMERATYPE_PCO2000, "pco.2000" },
    { CAMERATYPE_PCO4000, "pco.4000" },
    { CAMERATYPE_ROCHEHTC, "" },
    { CAMERATYPE_284XS, "" },
    { CAMERATYPE_KODAK1300OEM, "" },
    { CAMERATYPE_PCO1400, "pco.1400" },
    { CAMERATYPE_NEWGEN, "" },
    { CAMERATYPE_PCO_USBPIXELFLY, "pco usb pixelfly" },
    { CAMERATYPE_PCO_DIMAX_STD, "pco.dimax" },
    { CAMERATYPE_PCO_DIMAX_TV, "pco.dimax tv" },
    { CAMERATYPE_PCO_DIMAX_AUTOMOTIVE, "pco.dimax automotive" },
    { CAMERATYPE_SC3_SONYQE, "" },
    { CAMERATYPE_SC3_EMTI, "" },
    { CAMERATYPE_SC3_KODAK4800, "" },
    { CAMERATYPE_PCO_EDGE, "pco.edge" },
    { CAMERATYPE_PCO_EDGE_GL, "pco.edge global shutter" },
    { 0, NULL }
};

static void print_parameters(Fg_Struct *fg, unsigned int dma_index)
{
    int value, ret; 
    for (int i = 0; params[i].desc != NULL; i++) {
        ret = Fg_getParameter(fg, params[i].parameter, &value, dma_index);
        CHECK_FG(fg, ret);
        printf(" %s: %i\n", params[i].desc, value);
        if ((i == 0) && (value == 0))
            break;
    }
}

static const char *find_camera_type(uint32_t id)
{
    int i = 0;
    while ((pco_cameras[i].name != NULL) && (pco_cameras[i].id != id))
        i++;
    return pco_cameras[i].name;
}

static int64_t time_diff(struct timeval *end, struct timeval *start)
{
    return ((end->tv_sec * 1000000) + end->tv_usec) - ((start->tv_sec * 1000000) + start->tv_usec);
}

static void print_camera_name(pco_handle pco)
{
    char *name = NULL;
    if (pco_get_name(pco, &name) == PCO_NOERROR)
        printf("\n Camera name: %s\n", name);

    uint16_t type, subtype;
    CHECK_PCO(pco_get_camera_type(pco, &type, &subtype));
    const char *type_name = find_camera_type(type);
    if (type_name != NULL)
        printf(" Camera type: %s\n", type_name);

    free(name);
}

static void print_version_info(pco_handle pco)
{
    uint32_t serial_number;
    uint16_t hw_major, hw_minor, fw_major, fw_minor;

    if (pco_get_camera_version(pco, &serial_number, &hw_major, &hw_minor, &fw_major, &fw_minor) == PCO_NOERROR) {
        printf(" Serial number: %i\n", serial_number); 
        printf(" Hardware version: %i.%i\n", hw_major, hw_minor);
        printf(" Firmware version: %i.%i\n\n", fw_major, fw_minor);
    }
}

static void print_scan_mode(pco_handle pco)
{
    uint32_t scan_mode = 0xFFFF;
    if (pco_get_scan_mode(pco, &scan_mode) == PCO_NOERROR)
        printf(" Current scan mode: %i\n", scan_mode);
}

static void print_temperature(pco_handle pco)
{
    uint32_t temp_ccd, temp_cam, temp_power;
    if (pco_get_temperature(pco, &temp_ccd, &temp_cam, &temp_power) == PCO_NOERROR) {
        printf(" CCD temperature: %1.2f°C\n", (float) temp_ccd / 10.0f);
        printf(" Camera temperature: %.2f°C\n", (float) temp_cam);
        printf(" Power supply temperature: %.2f°C\n", (float) temp_power);
    }
}

static void print_delay_exposure(pco_handle pco)
{
    uint32_t delay = 0, exposure = 5000;
    CHECK_PCO(pco_set_delay_time(pco, delay));
    CHECK_PCO(pco_set_exposure_time(pco, exposure));
    CHECK_PCO(pco_get_delay_time(pco, &delay));
    CHECK_PCO(pco_get_exposure_time(pco, &exposure));
    printf(" Delay: %u µs\n", delay);
    printf(" Exposure: %u µs\n", exposure);
}

static void print_pixel_rates(pco_handle pco)
{
    uint32_t rates[4] = {0, };
    int num_rates = 0;

    CHECK_PCO(pco_get_available_pixelrates(pco, rates, &num_rates));
    printf(" Pixel rates: ");
    for (int i = 0; i < num_rates; i++)
        printf("%i ", rates[i]);
    printf("\n");
}

static void print_acquire_mode(pco_handle pco)
{
    uint16_t mode;
    if (pco_get_acquire_mode(pco, &mode) == PCO_NOERROR) {
        printf(" Acquire mode: ");
        switch (mode) {
            case ACQUIRE_MODE_AUTO:
                printf("auto\n");
                break;
            case ACQUIRE_MODE_EXTERNAL:
                printf("external\n");
                break;
            case ACQUIRE_MODE_EXTERNAL_FRAME_TRIGGER:
                printf("external frame trigger\n");
                break;
            default:
                printf("[wrong value]\n");
        }
    }
}

static void print_trigger_mode(pco_handle pco)
{
    uint16_t mode;
    if (pco_get_trigger_mode(pco, &mode) == PCO_NOERROR) {
        printf(" Trigger mode: ");
        switch (mode) {
            case TRIGGER_MODE_AUTOTRIGGER:
                printf("auto\n");
                break;
            case TRIGGER_MODE_SOFTWARETRIGGER:
                printf("software\n");
                break;
            case TRIGGER_MODE_EXTERNALTRIGGER:
                printf("external\n");
                break;
            case TRIGGER_MODE_EXTERNALEXPOSURECONTROL:
                printf("external exposure control\n");
                break;
            case TRIGGER_MODE_SOURCE_HDSDI:
                printf("hdsdi\n");
                break;
            case TRIGGER_MODE_EXTERNAL_SYNCHRONIZED:
                printf("external synchronized\n");
                break;
            default:
                printf("[wrong value]\n");
        }
    }
}

static void print_storage_mode(pco_handle pco)
{
    uint16_t mode;
    if (pco_get_storage_mode(pco, &mode) == PCO_NOERROR) {
        printf(" Storage mode: "); 
        switch (mode) {
            case STORAGE_MODE_RECORDER:
                printf("recorder\n");
                break;
            case STORAGE_MODE_FIFO_BUFFER:
                printf("FIFO buffer\n");
                break;
            default:
                printf("[wrong value]\n");
        }
    }
}

static void print_auto_transfer(pco_handle pco)
{
    int transfer = 0;
    if (pco_get_auto_transfer(pco, &transfer) == PCO_NOERROR) {
        printf(" Auto-transfer: %i\n", transfer); 
    }
}

static void print_binning(pco_handle pco)
{
    unsigned int num_horizontal, num_vertical;
    uint16_t *horizontal_binnings = NULL, *vertical_binnings = NULL;
    CHECK_PCO(pco_get_possible_binnings(pco, &horizontal_binnings, &num_horizontal, &vertical_binnings, &num_vertical));
    printf(" Horizontal Binnings: ");
    for (int i = 0; i < num_horizontal; i++)
        printf("%i ", horizontal_binnings[i]);
    printf("\n Vertical Binnings: ");
    for (int i = 0; i < num_vertical; i++)
        printf("%i ", vertical_binnings[i]);
    printf("\n");

    free(horizontal_binnings);
    free(vertical_binnings);
}

static void print_number_of_valid_images(pco_handle pco)
{
    uint16_t active_segment;
    uint32_t num_images = 0;

    CHECK_PCO(pco_get_active_segment(pco, &active_segment));
    CHECK_PCO(pco_get_num_images(pco, active_segment, &num_images));
    printf(" Number of valid images: %i\n", num_images);
}

static void print_segment_info(pco_handle pco)
{
    size_t sizes[4] = { 0, };
    CHECK_PCO(pco_get_segment_sizes(pco, sizes));
    printf(" Segment sizes: %i, %i, %i, %i pages\n", (int) sizes[0], (int) sizes[1], (int) sizes[2], (int) sizes[3]);

    uint16_t active_segment;
    CHECK_PCO(pco_get_active_segment(pco, &active_segment));
    printf(" Active segment: %i\n", active_segment);

    CHECK_PCO(pco_clear_active_segment(pco));
    print_number_of_valid_images(pco);
}

int main(int argc, char const* argv[])
{
    /* CameraLink specific */
    pco_handle pco = pco_init();
    if (pco == NULL) {
        printf("No CameraLink-based PCO camera found\n");
        return 1;
    }
        
    /* Query properties and output them */
    printf("\n--- Camera ----------\n");
    printf(" Active: ");
    if (!pco_is_active(pco)) {
        printf("no\n");
        pco_destroy(pco);
        return 1;
    }
    else
        printf("yes\n");

    CHECK_PCO(pco_set_storage_mode(pco, STORAGE_MODE_RECORDER));
    CHECK_PCO(pco_set_auto_transfer(pco, 1));

    print_camera_name(pco);
    print_version_info(pco);
    print_pixel_rates(pco);
    print_scan_mode(pco);
    print_temperature(pco);
    print_delay_exposure(pco);
    print_acquire_mode(pco);
    print_trigger_mode(pco);
    print_storage_mode(pco);
    print_auto_transfer(pco);
    print_binning(pco);
    print_segment_info(pco);

    /* Setup for recording */
    CHECK_PCO(pco_set_timestamp_mode(pco, TIMESTAMP_MODE_BINARYANDASCII));
    CHECK_PCO(pco_set_timebase(pco, 1, 1));

    uint16_t width_std, height_std, width_ex, height_ex;
    CHECK_PCO(pco_get_resolution(pco, &width_std, &height_std, &width_ex, &height_ex));
    printf(" Maximum resolution: %ix%i (standard), %ix%i (extended)\n", width_std, height_std, width_ex, height_ex);

    uint16_t roi_window[4] = {1, 1, width_std, height_std};
    CHECK_PCO(pco_set_roi(pco, roi_window));
    CHECK_PCO(pco_get_roi(pco, roi_window));
    printf(" ROI: <%i,%i> to <%i,%i>\n", roi_window[0], roi_window[1], roi_window[2], roi_window[3]);

    uint32_t width = width_std, height = height_std;

    /* Frame grabber specific
     *  - edge: libFullAreaGray8
     *  - 4000: libDualAreaGray16
     *  - dimax: libFullAreaGray16
     */
    static const char *applet = "libDualAreaGray16.so";
    int port = PORT_A;
    Fg_Struct *fg = Fg_Init(applet, 0);

    /* This must be set for each camera type:
     *  - edge: FG_CL_8BIT_FULL_10
     *  - 4000: FC_CL_SINGLETAP_16_BIT
     *  - dimax: FG_CL_SINGLETAP_8BIT
     */
    int val = FG_CL_SINGLETAP_16_BIT;
    CHECK_FG(fg, Fg_setParameter(fg, FG_CAMERA_LINK_CAMTYP, &val, port));

    /* This must be set for each camera type:
     *  - edge: FG_GRAY
     *  - 4000: FG_GRAY16
     *  - dimax: FG_GRAY16 or FG_GRAY
     */
    val = FG_GRAY16;
    CHECK_FG(fg, Fg_setParameter(fg, FG_FORMAT, &val, port));

    val = FREE_RUN;
    CHECK_FG(fg, Fg_setParameter(fg, FG_TRIGGERMODE, &val, port));

    /* We need to adjust this for the edge */
    /* width *= 2; */
    CHECK_FG(fg, Fg_setParameter(fg, FG_WIDTH, &width, port));
    CHECK_FG(fg, Fg_setParameter(fg, FG_HEIGHT, &height, port));
    /* width /= 2; */

    printf("\n--- Port A -------------\n");
    print_parameters(fg, port);
    printf("\n--- Port B -------------\n");
    print_parameters(fg, PORT_B);

    /* Start grabbing */
    printf("\n--- Grabbing on Port A\n");
    const int n_buffers = 5;

    dma_mem *mem = Fg_AllocMemEx(fg, n_buffers*width*height*sizeof(uint16_t), n_buffers);
    if (mem == NULL) {
        printf(" Couldn't allocate buffer memory\n");
    }

    const int n_images = 1;
    printf(" Acquire %d image(s)...", n_images);
    fflush(stdout);

    CHECK_PCO(pco_arm_camera(pco));
    CHECK_PCO(pco_set_rec_state(pco, 1));
    CHECK_FG(fg, Fg_AcquireEx(fg, port, n_images, ACQ_STANDARD, mem));

    frameindex_t last_frame = 1;
    struct timeval start, end;

    sleep(3);
    print_number_of_valid_images(pco);

    uint16_t active_segment;
    CHECK_PCO(pco_get_active_segment(pco, &active_segment));
    CHECK_PCO(pco_read_images(pco, active_segment, 1, 1));
    CHECK_PCO(pco_request_image(pco));

    gettimeofday(&start, NULL);
    last_frame = Fg_getLastPicNumberBlockingEx(fg, n_images, port, n_images, mem);
    gettimeofday(&end, NULL);
    CHECK_FG(fg, Fg_stopAcquireEx(fg, port, mem, STOP_ASYNC));
    printf(" done. (last frame = %li)\n", last_frame);

    if (last_frame < 0) {
        printf(" Timed out\n");
    }
    else {
        /* float factor = (pco->transfer.DataFormat & PCO_CL_DATAFORMAT_MASK) == PCO_CL_DATAFORMAT_5x12 ? 16./12 : 2.f; */
        float factor = 2.f;
        float transfered_bytes = n_images * width * height * factor;
        uint64_t elapsed = time_diff(&end, &start);
        printf(" Time: %.2f s\n", (float)(elapsed / 1000000.0f));
        printf(" Bandwidth: %.3f MB/s\n", (transfered_bytes / (1024*1024)) / (elapsed / 1000000.0f));
        printf(" Frame rate: %.2f Frames/s\n", n_images / (elapsed / 1000000.0f));

        uint16_t *frame = (uint16_t *) Fg_getImagePtrEx(fg, last_frame, port, mem);
        
        /* uint16_t *image = (uint16_t *) malloc(width*height*2); */
        /* pco_get_reorder_func(pco)(image, frame, width, height); */

        FILE *fp = fopen("out.raw", "wb");
        fwrite(frame, width*height*2, 1, fp);
        fclose(fp);
    }

    CHECK_PCO(pco_set_rec_state(pco, 0));
    uint32_t num_images = 0;
    if (pco_get_num_images(pco, active_segment, &num_images) == PCO_NOERROR)
        printf(" Number of valid images: %i\n", num_images);

    /* Close CameraLink interfaces and frame grabber */
    CHECK_FG(fg, Fg_FreeMemEx(fg, mem));
    Fg_FreeGrabber(fg);

    pco_destroy(pco);
    
    return 0;
}
