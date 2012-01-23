/* Copyright (C) 2010, 2011 Karlsruhe Institute of Technology

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

#ifndef __LIBPCO_H
#define __LIBPCO_H

#include <stdint.h>
#include <stdbool.h>
#include "sc2_defs.h"
#include "PCO_err.h"

#define PCO_SCANMODE_SLOW   0
#define PCO_SCANMODE_FAST   1

/**
 * Opaque data structure that identifies a PCO camera 
 */
typedef struct pco_t *pco_handle; 

/**
 * Specifies the type of function that is used to re-order images coming from a
 * pco.edge camera.
 */
typedef void (*pco_reorder_image_t)(uint16_t *bufout, uint16_t *bufin, int width, int height);

pco_handle pco_init();
void pco_destroy(pco_handle pco);

unsigned int pco_is_active(pco_handle pco);
unsigned int pco_get_camera_type(pco_handle pco, uint16_t *type, uint16_t *subtype);
unsigned int pco_get_health_state(pco_handle pco, uint32_t *warnings, uint32_t *errors, uint32_t *status);
unsigned int pco_reset(pco_handle pco);
unsigned int pco_get_temperature(pco_handle pco, uint32_t *ccd, uint32_t *camera, uint32_t *power);
unsigned int pco_get_name(pco_handle pco, char **name);
unsigned int pco_get_resolution(pco_handle pco, uint16_t *width_std, uint16_t *height_std, uint16_t *width_ex, uint16_t *height_ex);
unsigned int pco_get_available_pixelrates(pco_handle pco, uint32_t rates[4], int *num_rates);
unsigned int pco_get_pixelrate(pco_handle pco, uint32_t *rate);
unsigned int pco_set_pixelrate(pco_handle pco, uint32_t rate);
unsigned int pco_get_available_conversion_factors(pco_handle pco, uint16_t factors[4], int *num_rates);

unsigned int pco_set_scan_mode(pco_handle pco, uint32_t mode);
unsigned int pco_get_scan_mode(pco_handle pco, uint32_t *mode);

unsigned int pco_set_roi(pco_handle pco, uint16_t *window);
unsigned int pco_get_roi(pco_handle pco, uint16_t *window);

unsigned int pco_get_segment_sizes(pco_handle pco, size_t sizes[4]);
unsigned int pco_get_active_segment(pco_handle pco, uint16_t *segment);
unsigned int pco_clear_active_segment(pco_handle pco);
unsigned int pco_get_bit_alignment(pco_handle pco, bool *msb_aligned);
unsigned int pco_set_bit_alignment(pco_handle pco, bool msb_aligned);

unsigned int pco_get_num_images(pco_handle pco, uint16_t segment, uint32_t *num_images);
unsigned int pco_force_trigger(pco_handle pco, uint32_t *success);
unsigned int pco_set_timestamp_mode(pco_handle pco, uint16_t mode);
unsigned int pco_set_timebase(pco_handle pco, uint16_t delay,uint16_t expos);
unsigned int pco_set_delay_exposure(pco_handle pco, uint32_t delay, uint32_t expos);
unsigned int pco_get_delay_exposure(pco_handle pco, uint32_t *delay, uint32_t *expos);
unsigned int pco_get_trigger_mode(pco_handle pco, uint16_t *mode);
unsigned int pco_set_trigger_mode(pco_handle pco, uint16_t mode);
unsigned int pco_set_auto_transfer(pco_handle pco, int transfer);
unsigned int pco_get_auto_transfer(pco_handle pco, int *transfer);

unsigned int pco_get_storage_mode(pco_handle pco, uint16_t *mode);
unsigned int pco_set_storage_mode(pco_handle pco, uint16_t mode);
unsigned int pco_arm_camera(pco_handle pco);
unsigned int pco_get_rec_state(pco_handle pco, uint16_t *state);
unsigned int pco_set_rec_state(pco_handle pco, uint16_t state);
unsigned int pco_get_acquire_mode(pco_handle pco, uint16_t *mode);
unsigned int pco_set_acquire_mode(pco_handle pco, uint16_t mode);

unsigned int pco_request_image(pco_handle pco);
unsigned int pco_read_images(pco_handle pco, uint16_t segment, uint32_t start, uint32_t end);
unsigned int pco_get_actual_size(pco_handle pco, uint32_t *width, uint32_t *height);
unsigned int pco_set_hotpixel_correction(pco_handle pco, uint32_t mode);

unsigned int pco_control_command(pco_handle pco, void *buffer_in, uint32_t size_in, void *buffer_out, uint32_t size_out);

pco_reorder_image_t pco_get_reorder_func(pco_handle pco);

#endif
