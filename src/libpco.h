#ifndef __LIBPCO_H
#define __LIBPCO_H

#include <stdint.h>
#include "sc2_cl.h"
#include "sc2_add.h"
#include "sc2_telegram.h"
#include "sc2_command.h"
#include "PCO_err.h"

typedef struct pco_edge {
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
} pco_edge_t;

#define PCO_ERROR_LOG(s) { fprintf(stderr, "pco: %s <%s:%i>\n", s, __FILE__, __LINE__); }

#define PCO_SCANMODE_SLOW   0
#define PCO_SCANMODE_FAST   1

void check_error_cl(int code);

/**
 * Initialize pco.edge camera.
 * \return Pointer to newly created pco_edge structure
 */
struct pco_edge *pco_init(void);

/**
 * Close pco.edge device
 * \param[in] pco Device handle to pco.edge
 */
void pco_destroy(struct pco_edge *pco);

/**
 * Check if the pco.edge is active and can be used.
 * \return non-zero number if active, zero if not
 */
unsigned int pco_is_active(struct pco_edge *pco);

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
unsigned int pco_set_scan_mode(struct pco_edge *pco, uint32_t mode);
unsigned int pco_get_scan_mode(struct pco_edge *pco, uint32_t *mode);

/**
 * Set the readout window/region of interest.
 *
 * \param[in] pco Device handle to pco.edge
 * \param[in] window 4-element array with the first two elements denoting
 *   upper-left corner and last two elements the lower-right corner of the ROI
 */
unsigned int pco_set_roi(struct pco_edge *pco, uint16_t *window);
unsigned int pco_get_roi(struct pco_edge *pco, uint16_t *window);

unsigned int pco_read_property(struct pco_edge *pco, uint16_t code, void *dst, uint32_t size);
unsigned int pco_get_rec_state(struct pco_edge *pco, uint16_t *state);
unsigned int pco_set_rec_state(struct pco_edge *pco, uint16_t state);
unsigned int pco_set_timestamp_mode(struct pco_edge *pco, uint16_t mode);
unsigned int pco_set_timebase(struct pco_edge *pco, uint16_t delay,uint16_t expos);
unsigned int pco_set_delay_exposure(struct pco_edge *pco, uint32_t delay, uint32_t expos);
unsigned int pco_get_delay_exposure(struct pco_edge *pco, uint32_t *delay, uint32_t *expos);
unsigned int pco_arm_camera(struct pco_edge *pco);
unsigned int pco_get_actual_size(struct pco_edge *pco, uint32_t *width, uint32_t *height);


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
unsigned int pco_control_command(struct pco_edge *pco, void *buffer_in, uint32_t size_in, void *buffer_out, uint32_t size_out);

#endif
