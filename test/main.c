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

#define CHECK_ERROR_FG(fg, code) if ((code) != FG_OK) \
    fprintf(stderr, "Error at line %i: %i\n", __LINE__, Fg_getLastErrorNumber((fg)));

#define CHECK_PCO(err) if ((err) != PCO_NOERROR) fprintf(stderr, "error at %s:%i\n", __FILE__, __LINE__);

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

void print_parameters(Fg_Struct *fg, unsigned int dma_index)
{
    int value, ret; 
    for (int i = 0; params[i].desc != NULL; i++) {
        ret = Fg_getParameter(fg, params[i].parameter, &value, dma_index);
        CHECK_ERROR_FG(fg, ret);
        printf(" %s: %i\n", params[i].desc, value);
    }
}

const char *find_camera_type(uint32_t id)
{
    int i = 0;
    while ((pco_cameras[i].name != NULL) && (pco_cameras[i].id != id))
        i++;
    return pco_cameras[i].name;
}

int64_t time_diff(struct timeval *end, struct timeval *start)
{
    return ((end->tv_sec * 1000000) + end->tv_usec) - ((start->tv_sec * 1000000) + start->tv_usec);
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

    char *name = NULL;
    if (pco_get_name(pco, &name) == PCO_NOERROR) {
        printf("\n Camera name: %s\n", name);
        free(name);
    }

    uint32_t scan_mode = 0xFFFF;
    if (pco_get_scan_mode(pco, &scan_mode) == PCO_NOERROR)
        printf(" Current scan mode: %i\n", scan_mode);

    CHECK_PCO(pco_set_storage_mode(pco, STORAGE_MODE_RECORDER));

    uint32_t temp_ccd, temp_cam, temp_power;
    if (pco_get_temperature(pco, &temp_ccd, &temp_cam, &temp_power) == PCO_NOERROR) {
        printf(" CCD temperature: %1.2f°C\n", (float) temp_ccd / 10.0f);
        printf(" Camera temperature: %.2f°C\n", (float) temp_cam);
        printf(" Power supply temperature: %.2f°C\n", (float) temp_power);
    }

    uint32_t delay = 0, exposure = 5000;
    CHECK_PCO(pco_set_delay_exposure(pco, delay, exposure));
    CHECK_PCO(pco_get_delay_exposure(pco, &delay, &exposure));
    printf(" Delay: %u µs\n", delay);
    printf(" Exposure: %u µs\n", exposure);

    CHECK_PCO(pco_set_auto_transfer(pco, 1));
    int transfer = 0;
    if (pco_get_auto_transfer(pco, &transfer) == PCO_NOERROR) {
        printf(" Auto-transfer: %i\n", transfer); 
    }

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

    size_t sizes[4];
    if (pco_get_segment_sizes(pco, sizes) == PCO_NOERROR)
        printf(" Segment sizes: %i, %i, %i, %i pages\n", (int) sizes[0], (int) sizes[1], (int) sizes[2], (int) sizes[3]);

    uint16_t active_segment;
    if (pco_get_active_segment(pco, &active_segment) == PCO_NOERROR)
        printf(" Active segment: %i\n", active_segment);

    CHECK_PCO(pco_clear_active_segment(pco));
    uint32_t num_images = 0;
    if (pco_get_num_images(pco, active_segment, &num_images) == PCO_NOERROR)
        printf(" Number of valid images: %i\n", num_images);

    /* Setup for recording */
    CHECK_PCO(pco_set_timestamp_mode(pco, TIMESTAMP_MODE_BINARYANDASCII));
    CHECK_PCO(pco_set_timebase(pco, 1, 1));

    uint16_t roi_window[4] = {1, 1, 1920, 1080};
    CHECK_PCO(pco_set_roi(pco, roi_window));
    CHECK_PCO(pco_get_roi(pco, roi_window));
    printf(" ROI: <%i,%i> to <%i,%i>\n", roi_window[0], roi_window[1], roi_window[2], roi_window[3]);

    uint32_t width = roi_window[2]-roi_window[0] + 1;
    uint32_t height = roi_window[3]-roi_window[1] + 1;

    CHECK_PCO(pco_arm_camera(pco));

    if (pco_get_actual_size(pco, &width, &height) == PCO_NOERROR) {
        printf(" Dimensions: %ix%i\n", width, height);
    }

    /* Frame grabber specific */
    static const char *applet = "libDualAreaGray16.so";
    int port = PORT_A;
    Fg_Struct *fg = Fg_Init(applet, 0);

    int val = FG_CL_SINGLETAP_16_BIT;
    CHECK_ERROR_FG(fg, Fg_setParameter(fg, FG_CAMERA_LINK_CAMTYP, &val, port));

    val = FG_GRAY16;
    CHECK_ERROR_FG(fg, Fg_setParameter(fg, FG_FORMAT, &val, port));

    val = FREE_RUN;
    CHECK_ERROR_FG(fg, Fg_setParameter(fg, FG_TRIGGERMODE, &val, port));

    /* width *= 2; */
    CHECK_ERROR_FG(fg, Fg_setParameter(fg, FG_WIDTH, &width, port));
    CHECK_ERROR_FG(fg, Fg_setParameter(fg, FG_HEIGHT, &height, port));
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

    CHECK_PCO(pco_set_rec_state(pco, 1));
    CHECK_ERROR_FG(fg, Fg_AcquireEx(fg, port, n_images, ACQ_STANDARD, mem));

    frameindex_t last_frame = 1;
    struct timeval start, end;

    sleep(3);
    if (pco_get_num_images(pco, active_segment, &num_images) == PCO_NOERROR)
        printf(" Number of valid images: %i\n", num_images);

    /* CHECK_PCO(pco_read_images(pco, active_segment, 1, 1)); */
    CHECK_PCO(pco_request_image(pco));
    gettimeofday(&start, NULL);
    last_frame = Fg_getLastPicNumberBlockingEx(fg, n_images, port, n_images, mem);
    gettimeofday(&end, NULL);
    CHECK_ERROR_FG(fg, Fg_stopAcquireEx(fg, port, mem, STOP_ASYNC));
    printf(" done. (last frame = %i)\n", (unsigned int) last_frame);

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
        /* pco->reorder_image(image, frame, width, height); */

        FILE *fp = fopen("out.raw", "wb");
        fwrite(frame, width*height*2, 1, fp);
        fclose(fp);
        /* free(image); */
    }

    CHECK_PCO(pco_set_rec_state(pco, 0));
    if (pco_get_num_images(pco, active_segment, &num_images) == PCO_NOERROR)
        printf(" Number of valid images: %i\n", num_images);

    /* Close CameraLink interfaces and frame grabber */
    CHECK_ERROR_FG(fg, Fg_FreeMemEx(fg, mem));
    Fg_FreeGrabber(fg);

    pco_destroy(pco);
    
    return 0;
}
