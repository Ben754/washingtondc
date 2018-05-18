/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#ifndef SPG_H_
#define SPG_H_

#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "dc_sched.h"
#include "pvr2_core_reg.h"

struct pvr2_core_mem_mapped_reg;

void spg_init();
void spg_cleanup();

// val should be either 1 or 2
void spg_set_pclk_div(unsigned val);

void spg_set_pix_double_x(bool val);
void spg_set_pix_double_y(bool val);

uint32_t get_spg_control();

uint32_t
spg_hblank_int_mmio_read(struct mmio_region_pvr2_core_reg *region,
                         unsigned idx, void *ctxt);
void
spg_hblank_int_mmio_write(struct mmio_region_pvr2_core_reg *region,
                          unsigned idx, uint32_t val, void *ctxt);

uint32_t
spg_vblank_int_mmio_read(struct mmio_region_pvr2_core_reg *region,
                         unsigned idx, void *ctxt);
void
spg_vblank_int_mmio_write(struct mmio_region_pvr2_core_reg *region,
                          unsigned idx, uint32_t val, void *ctxt);

uint32_t
spg_control_mmio_read(struct mmio_region_pvr2_core_reg *region,
                      unsigned idx, void *ctxt);
void
spg_control_mmio_write(struct mmio_region_pvr2_core_reg *region,
                       unsigned idx, uint32_t val, void *ctxt);

uint32_t
spg_hblank_mmio_read(struct mmio_region_pvr2_core_reg *region,
                     unsigned idx, void *ctxt);
void
spg_hblank_mmio_write(struct mmio_region_pvr2_core_reg *region,
                      unsigned idx, uint32_t val, void *ctxt);

uint32_t
spg_load_mmio_read(struct mmio_region_pvr2_core_reg *region,
                   unsigned idx, void *ctxt);
void
spg_load_mmio_write(struct mmio_region_pvr2_core_reg *region,
                    unsigned idx, uint32_t val, void *ctxt);

uint32_t
spg_vblank_mmio_read(struct mmio_region_pvr2_core_reg *region,
                     unsigned idx, void *ctxt);
void
spg_vblank_mmio_write(struct mmio_region_pvr2_core_reg *region,
                      unsigned idx, uint32_t val, void *ctxt);

uint32_t
spg_status_mmio_read(struct mmio_region_pvr2_core_reg *region,
                     unsigned idx, void *ctxt);

#endif
