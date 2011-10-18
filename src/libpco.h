#ifndef __LIBPCO_H
#define __LIBPCO_H

#include <stdint.h>
/* #include "sc2_cl.h" */
#include "sc2_add.h"
#include "sc2_telegram.h"
#include "sc2_command.h"
#include "PCO_err.h"

typedef struct pco_t *pco_handle;

#define PCO_ERROR_LOG(s) { fprintf(stderr, "pco: %s <%s:%i>\n", s, __FILE__, __LINE__); }

#define PCO_SCANMODE_SLOW   0
#define PCO_SCANMODE_FAST   1

void check_error_cl(int code);

/**
 * Initialize pco camera.
 * \return Pointer to newly created pco_edge structure
 */
pco_handle pco_init(void);

/**
 * Close pco device
 * \param[in] pco Device handle to pco.edge
 */
void pco_destroy(pco_handle pco);

/**
 * Check if the pco is active and can be used.
 * \return non-zero number if active, zero if not
 */
unsigned int pco_is_active(pco_handle pco);

unsigned int pco_get_health_state(pco_handle pco, uint32_t *warnings, uint32_t *errors, uint32_t *status);
unsigned int pco_reset(pco_handle pco);
unsigned int pco_get_temperature(pco_handle pco, float *ccd, float *camera, float *power);
unsigned int pco_get_name(pco_handle pco, char **name);

/**
 * Set scan and readout mode.
 *
 * There are two modes to read out the sensor chip and that affect the actual
 * pixel clock. Both modes will essentially return 16 bit data with
 * approximately 10 and 12 bit dynamic range.
 *
 * \param[in] pco Device handle to pco.edge
 * \param[in] mode PCO_SCANMODE_SLOW or PCO_SCANMODE_FAST
 */
unsigned int pco_set_scan_mode(pco_handle pco, uint32_t mode);
unsigned int pco_get_scan_mode(pco_handle pco, uint32_t *mode);

/**
 * Set the readout window/region of interest.
 *
 * \param[in] pco Device handle to pco.edge
 * \param[in] window 4-element array with the first two elements denoting
 *   upper-left corner and last two elements the lower-right corner of the ROI
 */
unsigned int pco_set_roi(pco_handle pco, uint16_t *window);
unsigned int pco_get_roi(pco_handle pco, uint16_t *window);

unsigned int pco_clear_active_segment(pco_handle pco);

unsigned int pco_get_num_images(pco_handle pco, uint32_t segment, uint32_t *num_images);
unsigned int pco_read_property(pco_handle pco, uint16_t code, void *dst, uint32_t size);
unsigned int pco_force_trigger(pco_handle pco, uint32_t *success);
unsigned int pco_set_timestamp_mode(pco_handle pco, uint16_t mode);
unsigned int pco_set_storage_mode(pco_handle pco, uint32_t mode);
unsigned int pco_set_timebase(pco_handle pco, uint16_t delay,uint16_t expos);
unsigned int pco_set_delay_exposure(pco_handle pco, uint32_t delay, uint32_t expos);
unsigned int pco_get_delay_exposure(pco_handle pco, uint32_t *delay, uint32_t *expos);

unsigned int pco_arm_camera(pco_handle pco);
unsigned int pco_get_rec_state(pco_handle pco, uint16_t *state);
unsigned int pco_set_rec_state(pco_handle pco, uint16_t state);

unsigned int pco_request_image(pco_handle pco);
unsigned int pco_read_images(pco_handle pco, uint32_t segment, uint32_t start, uint32_t end);
unsigned int pco_get_actual_size(pco_handle pco, uint32_t *width, uint32_t *height);
unsigned int pco_set_hotpixel_correction(pco_handle pco, uint32_t mode);

/**
 * Send control data via CameraLink to camera.
 *
 * This function sends messages as defined in sc2_telegram.h to the camera
 * device via the serial CameraLink connection. However, it is strongly
 * disadvised to use this function directly but rather use one of the
 * pco_set/get functions.
 *
 * \param[in] pco Device handle
 * \param[in] buffer_in Appropriately filled message structure
 * \param[in] size_in Size of buffer_in (Be aware to send real size using packed structures!)
 * \param[out] buffer_out Buffer where result message is stored
 * \param[out] size_out Size of buffer_out
 */
unsigned int pco_control_command(pco_handle pco, void *buffer_in, uint32_t size_in, void *buffer_out, uint32_t size_out);

#endif
