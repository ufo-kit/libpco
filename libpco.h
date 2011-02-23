#ifndef __LIBPCO_H
#define __LIBPCO_H

#include <stdint.h>
#include "sc2_cl.h"
#include "sc2_add.h"
#include "sc2_telegram.h"
#include "sc2_command.h"
#include "PCO_err.h"

typedef struct pco_edge_t {
    unsigned int num_ports;
    unsigned int baud_rate;

    void *serial_refs[4];
    void *serial_ref;

    PCO_SC2_TIMEOUTS timeouts;
    PCO_SC2_CL_TRANSFER_PARAM transfer;
    SC2_Camera_Description_Response description;
    SC2_Firmware_Versions_Response firmware_version;
} pco_edge;

#define PCO_ERROR_LOG(s) { fprintf(stderr, "pco: %s <%s:%i>\n", s, __FILE__, __LINE__); }

void check_error_cl(int code);

struct pco_edge_t *pco_init(void);
void pco_destroy(struct pco_edge_t *pco);

unsigned int pco_control_command(struct pco_edge_t *pco, void *buffer_in, uint32_t size_in, void *buffer_out, uint32_t size_out);

unsigned int pco_active(struct pco_edge_t *pco);
unsigned int pco_scan_and_set_baud_rate(struct pco_edge_t *pco);
unsigned int pco_retrieve_cl_config(struct pco_edge_t *pco);
unsigned int pco_set_cl_config(struct pco_edge_t *pco);
unsigned int pco_read_property(struct pco_edge_t *pco, uint16_t code, void *dst, uint32_t size);
unsigned int pco_get_rec_state(struct pco_edge_t *pco, uint16_t *state);
unsigned int pco_set_rec_state(struct pco_edge_t *pco, uint16_t state);
unsigned int pco_set_timestamp_mode(struct pco_edge_t *pco, uint16_t mode);
unsigned int pco_set_timebase(struct pco_edge_t *pco, uint16_t delay,uint16_t expos);
unsigned int pco_set_delay_exposure(struct pco_edge_t *pco, uint32_t delay, uint32_t expos);
unsigned int pco_get_delay_exposure(struct pco_edge_t *pco, uint32_t *delay, uint32_t *expos);
unsigned int pco_arm_camera(struct pco_edge_t *pco);
unsigned int pco_get_actual_size(struct pco_edge_t *pco, uint32_t *width, uint32_t *height);

#endif
