/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include "error.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"
#include "pvr2_reg.h"
#include "hw/sys/holly_intc.h"
#include "dc_sched.h"
#include "pvr2.h"
#include "framebuffer.h"
#include "pvr2_tex_cache.h"

#include "pvr2_yuv.h"

static void pvr2_yuv_input_byte(unsigned byte);

enum pvr2_yuv_fmt {
    PVR2_YUV_FMT_420,
    PVR2_YUV_FMT_422
};

static struct pvr2_yuv {
    uint32_t dst_addr;
    enum pvr2_yuv_fmt fmt;
    unsigned macroblock_offset;

    unsigned cur_macroblock_x, cur_macroblock_y;

    // width and height, in terms of 16x16 macroblocks
    unsigned macroblock_count_x, macroblock_count_y;

    uint8_t u_buf[64];
    uint8_t v_buf[64];
    uint8_t y_buf[256];

    bool yuv_complete_event_scheduled;
} pvr2_yuv;

#define PVR2_YUV_COMPLETE_INT_DELAY (SCHED_FREQUENCY / 1024)

static void
pvr2_yuv_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_yuv.yuv_complete_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE);
}

static struct SchedEvent pvr2_render_complete_int_event = {
    .handler = pvr2_yuv_complete_int_event_handler
};

static void pvr2_yuv_schedule_int(void) {
    if (!pvr2_yuv.yuv_complete_event_scheduled) {
        dc_cycle_stamp_t int_when =
            clock_cycle_stamp(pvr2_clk) + PVR2_YUV_COMPLETE_INT_DELAY;
        pvr2_yuv.yuv_complete_event_scheduled = true;
        pvr2_render_complete_int_event.when = int_when;
        sched_event(pvr2_clk, &pvr2_render_complete_int_event);
    }
}

void pvr2_yuv_set_base(struct pvr2 *pvr2, uint32_t new_base) {
    uint32_t tex_ctrl = get_ta_yuv_tex_ctrl(pvr2);

    if (tex_ctrl & (1 << 16))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    if (tex_ctrl & (1 << 24))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    /*
     * TODO: what happens if any of these settings change without updating the
     * base address?
     */
    pvr2_yuv.dst_addr = new_base;
    pvr2_yuv.fmt = PVR2_YUV_FMT_420;
    pvr2_yuv.macroblock_offset = 0;
    pvr2_yuv.cur_macroblock_x = 0;
    pvr2_yuv.cur_macroblock_y = 0;
    pvr2_yuv.macroblock_count_x = (tex_ctrl & 0x3f) + 1;
    pvr2_yuv.macroblock_count_y = ((tex_ctrl >> 8) & 0x3f) + 1;
}

void pvr2_yuv_input_data(struct pvr2 *pvr2, void const *dat, unsigned n_bytes) {
    uint32_t tex_ctrl = get_ta_yuv_tex_ctrl(pvr2);

    if (tex_ctrl & (1 << 16))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    if (tex_ctrl & (1 << 24))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if ((pvr2_yuv.dst_addr + 3) >= (ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1))
        RAISE_ERROR(ERROR_INTEGRITY);

    uint8_t const *dat8 = (uint8_t const*)dat;

    while (n_bytes) {
        uint8_t dat_byte;
        memcpy(&dat_byte, dat8, sizeof(dat_byte));
        pvr2_yuv_input_byte(dat_byte);
        dat8++;
        n_bytes--;
    }
}

static void pvr2_yuv_macroblock(void) {
    uint32_t block[16][8];

    if (pvr2_yuv.cur_macroblock_x >= pvr2_yuv.macroblock_count_x) {
        LOG_ERROR("pvr2_yuv.cur_macroblock_x is %u\n", pvr2_yuv.cur_macroblock_x);
        LOG_ERROR("pvr2_yuv.macroblock_count_x is %u\n", pvr2_yuv.macroblock_count_x);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    if (pvr2_yuv.cur_macroblock_y >= pvr2_yuv.macroblock_count_y) {
        // TODO: should reset to zero here
        LOG_ERROR("pvr2_yuv.cur_macroblock_y is %u\n", pvr2_yuv.cur_macroblock_y);
        LOG_ERROR("pvr2_yuv.macroblock_count_y is %u\n", pvr2_yuv.macroblock_count_y);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned row, col;
    for (row = 0; row < 16; row++) {
        for (col = 0; col < 8; col++) {
            /*
             * For the luminance component, each macro block is stored as four
             * 8x8 sub-macroblocks, each of which is contiguous.
             */
            unsigned col_lum, row_lum;
            unsigned lum_start;
            if (row < 8) {
                if (col < 4) {
                    lum_start = 0;
                    col_lum = col;
                    row_lum = row;
                } else {
                    lum_start = 0x40;
                    col_lum = col - 4;
                    row_lum = row;
                }
            } else {
                if (col < 4) {
                    lum_start = 0x80;
                    col_lum = col;
                    row_lum = row - 8;
                } else {
                    lum_start = 0xc0;
                    col_lum = col - 4;
                    row_lum = row - 8;
                }
            }

            unsigned lum[2] = {
                pvr2_yuv.y_buf[row_lum * 8 + col_lum * 2 + lum_start],
                pvr2_yuv.y_buf[row_lum * 8 + col_lum * 2 + lum_start + 1],
            };

            unsigned u_val = pvr2_yuv.u_buf[(row / 2) * 8 + col];
            unsigned v_val = pvr2_yuv.v_buf[(row / 2) * 8 + col];

            block[row][col] = (lum[0] << 8) | (lum[1] << 24) | u_val | (v_val << 16);
        }
    }

    /*
     * TODO: how to know the output linestride?  For now it is hardocoded to
     * 1024 bytes (512 pixels) because that is what NBA2K expects.  Maybe it's
     * 16 * macroblock_count_x * 2 bytes rounded up to next power of two?
     *
     * logically you'd expect the answer to be
     * (2 * 16 * pvr2_yuv.cur_macroblock_x) bytes, but NBA2K clearly needs it to
     * be 1024 bytes...
     */

    unsigned linestride = 512 * 2;
    unsigned macroblock_offs = linestride * 16 * pvr2_yuv.cur_macroblock_y +
        pvr2_yuv.cur_macroblock_x * 8 * sizeof(uint32_t);

    uint32_t row_offs32 = pvr2_yuv.dst_addr / 4 + macroblock_offs / 4;
    uint32_t *row_ptr = ((uint32_t*)pvr2_tex64_mem) + row_offs32;
    uint32_t addr_base = 4 * row_offs32;

    for (row = 0; row < 16; row++) {
        memcpy(row_ptr, block[row], 8 * sizeof(uint32_t));
        row_ptr += linestride / 4;
        addr_base += linestride;

        pvr2_framebuffer_notify_write(ADDR_TEX64_FIRST + addr_base,
                                      8 * sizeof(uint32_t));
        pvr2_tex_cache_notify_write(ADDR_TEX64_FIRST + addr_base,
                                    8 * sizeof(uint32_t));
    }

    pvr2_yuv.cur_macroblock_x++;
    if (pvr2_yuv.cur_macroblock_x >= pvr2_yuv.macroblock_count_x) {
        pvr2_yuv.cur_macroblock_x = 0;
        pvr2_yuv.cur_macroblock_y++;
    }

    if (pvr2_yuv.cur_macroblock_y == pvr2_yuv.macroblock_count_y) {
        pvr2_yuv_schedule_int();
    }
}

static void pvr2_yuv_input_byte(unsigned dat) {
    if (pvr2_yuv.fmt != PVR2_YUV_FMT_420)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (pvr2_yuv.macroblock_offset < 64) {
        pvr2_yuv.u_buf[pvr2_yuv.macroblock_offset++] = dat;
    } else if (pvr2_yuv.macroblock_offset < 128) {
        pvr2_yuv.v_buf[pvr2_yuv.macroblock_offset++ - 64] = dat;
    } else if (pvr2_yuv.macroblock_offset < 384) {
        pvr2_yuv.y_buf[pvr2_yuv.macroblock_offset++ - 128] = dat;
    } else {
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (pvr2_yuv.macroblock_offset == 384) {
        pvr2_yuv.macroblock_offset = 0;

        if (pvr2_yuv.cur_macroblock_x >= pvr2_yuv.macroblock_count_x ||
            pvr2_yuv.cur_macroblock_y >= pvr2_yuv.macroblock_count_y)
            RAISE_ERROR(ERROR_INTEGRITY);

        pvr2_yuv_macroblock();
    }
}
