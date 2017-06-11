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

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "spg.h"
#include "types.h"
#include "mem_code.h"
#include "MemoryMap.h"
#include "error.h"
#include "video/opengl/framebuffer.h"

#include "pvr2_core_reg.h"

static uint32_t fb_r_sof1, fb_r_sof2, fb_r_ctrl, fb_r_size,
    fb_w_sof1, fb_w_sof2, fb_w_ctrl, fb_w_linestride;

#define N_PVR2_CORE_REGS (ADDR_PVR2_CORE_LAST - ADDR_PVR2_CORE_FIRST + 1)
static reg32_t pvr2_core_regs[N_PVR2_CORE_REGS];

struct pvr2_core_mem_mapped_reg;

typedef int(*pvr2_core_reg_read_handler_t)(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
typedef int(*pvr2_core_reg_write_handler_t)(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);

static int
default_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
default_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
pvr2_core_read_only_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
pvr2_core_id_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len);
static int
pvr2_core_revision_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len);

static int
fb_r_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);

static int
vo_control_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
vo_control_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);

static int
fb_r_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_r_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_r_size_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_size_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_r_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_w_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_w_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_w_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_linestride_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void *buf, addr32_t addr, unsigned len);
static int
fb_w_linestride_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void const *buf, addr32_t addr, unsigned len);

static struct pvr2_core_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;
    unsigned n_elem;

    pvr2_core_reg_read_handler_t on_read;
    pvr2_core_reg_write_handler_t on_write;
} pvr2_core_reg_info[] = {
    { "ID", 0x5f8000, 4, 1,
      pvr2_core_id_read_handler, pvr2_core_read_only_reg_write_handler },
    { "REVISION", 0x5f8004, 4, 1,
      pvr2_core_revision_read_handler, pvr2_core_read_only_reg_write_handler },
    { "SOFTRESET", 0x5f8008, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },

    { "SPAN_SORT_CFG", 0x5f8030, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_BORDER_COL", 0x5f8040, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_R_CTRL", 0x5f8044, 4, 1,
      fb_r_ctrl_reg_read_handler, fb_r_ctrl_reg_write_handler },
    { "FB_W_CTRL", 0x5f8048, 4, 1,
      fb_w_ctrl_reg_read_handler, fb_w_ctrl_reg_write_handler },
    { "FB_W_LINESTRIDE", 0x5f804c, 4, 1,
      fb_w_linestride_reg_read_handler, fb_w_linestride_reg_write_handler },
    { "FB_R_SOF1", 0x5f8050, 4, 1,
      fb_r_sof1_reg_read_handler, fb_r_sof1_reg_write_handler },
    { "FB_R_SOF2", 0x5f8054, 4, 1,
      fb_r_sof2_reg_read_handler, fb_r_sof2_reg_write_handler },
    { "FB_R_SIZE", 0x5f805c, 4, 1,
      fb_r_size_reg_read_handler, fb_r_size_reg_write_handler },
    { "FB_W_SOF1", 0x5f8060, 4, 1,
      fb_w_sof1_reg_read_handler, fb_w_sof1_reg_write_handler },
    { "FB_W_SOF2", 0x5f8064, 4, 1,
      fb_w_sof2_reg_read_handler, fb_w_sof2_reg_write_handler },
    { "FB_X_CLIP", 0x5f8068, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_Y_CLIP", 0x5f806c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_SHAD_SCALE", 0x5f8074, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_CULL_VAL", 0x5f8078, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_PARAM_CFG", 0x5f807c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "HALF_OFFSET", 0x5f8080, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_PERP_VAL", 0x5f8084, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "ISP_BACKGND_D", 0x5f8088, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "ISP_BACKGND_T", 0x5f808c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "ISP_FEED_CFG", 0x5f8098, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_CLAMP_MAX", 0x5f80bc, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_CLAMP_MIN", 0x5f80c0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TEXT_CONTROL", 0x5f80e4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SCALER_CTL", 0x5f80f4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_BURSTXTRL", 0x5f8110, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "Y_COEFF", 0x5f8118, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SDRAM_REFRESH", 0x5f80a0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SDRAM_CFG", 0x5f80a8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_COL_RAM", 0x5f80b0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_COL_VERT", 0x5f80b4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_DENSITY", 0x5f80b8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_HBLANK_INT", 0x5f80c8, 4, 1,
      read_spg_hblank_int, write_spg_hblank_int },
    { "SPG_VBLANK_INT", 0x5f80cc, 4, 1,
      read_spg_vblank_int, write_spg_vblank_int },
    { "SPG_CONTROL", 0x5f80d0, 4, 1,
      read_spg_control, write_spg_control },
    { "SPG_HBLANK", 0x5f80d4, 4, 1,
      read_spg_hblank, write_spg_hblank },
    { "SPG_LOAD", 0x5f80d8, 4, 1,
      read_spg_load, write_spg_load },
    { "SPG_VBLANK", 0x5f80dc, 4, 1,
      read_spg_vblank, write_spg_vblank },
    { "SPG_WIDTH", 0x5f80e0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_CONTROL", 0x5f80e8, 4, 1,
      vo_control_reg_read_handler, vo_control_reg_write_handler },
    { "VO_STARTX", 0x5f80ec, 4, 1,
      default_pvr2_core_reg_read_handler, default_pvr2_core_reg_write_handler },
    { "VO_STARTY", 0x5f80f0, 4, 1,
      default_pvr2_core_reg_read_handler, default_pvr2_core_reg_write_handler },
    { "SPG_STATUS", 0x5f810c, 4, 1,
      read_spg_status, pvr2_core_read_only_reg_write_handler },
    { "TA_OL_BASE", 0x5f8124, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_ISP_BASE", 0x5f8128, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_ISP_LIMIT", 0x5f8130, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_OL_LIMIT", 0x5f812c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_GLOB_TILE_CLIP", 0x5f813c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_ALLOC_CTRL", 0x5f8140, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_NEXT_OPB_INIT", 0x5f8164, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_LIST_INIT", 0x5f8144, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },

    { "FOG_TABLE", 0x5f8200, 4, 0x80,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },

    { NULL }
};

int pvr2_core_reg_read(void *buf, size_t addr, size_t len) {
    struct pvr2_core_mem_mapped_reg *curs = pvr2_core_reg_info;

    while (curs->reg_name) {
        if ((addr >= curs->addr) &&
            (addr < (curs->addr + curs->len * curs->n_elem))) {
            if ((curs->len == len) || (curs->len >= len && curs->n_elem > 1)) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while reading "
                                  "from a pvr2 core register");
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_feature("reading from one of the pvr2 core registers");
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int pvr2_core_reg_write(void const *buf, size_t addr, size_t len) {
    struct pvr2_core_mem_mapped_reg *curs = pvr2_core_reg_info;

    while (curs->reg_name) {
        if ((addr >= curs->addr) &&
            (addr < (curs->addr + curs->len * curs->n_elem))) {
            if ((curs->len == len) || (curs->len >= len && curs->n_elem > 1)) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while writing to a pvr2 "
                                  "core register");
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_feature("writing to one of the pvr2 core registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
default_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_CORE_FIRST) >> 2;
    memcpy(buf, idx + pvr2_core_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_CORE_FIRST) >> 2;
    memcpy(idx + pvr2_core_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
warn_pvr2_core_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_pvr2_core_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        fprintf(stderr, "WARNING: read from pvr2 core register %s\n",
                reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            fprintf(stderr, "WARNING: read 0x%02x from pvr2 core register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            fprintf(stderr, "WARNING: read 0x%04x from pvr2 core register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            fprintf(stderr, "WARNING: read 0x%08x from pvr2 core register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            fprintf(stderr, "WARNING: read from pvr2 core register %s\n",
                    reg_info->reg_name);
        }
    }

    return MEM_ACCESS_SUCCESS;
}

static int
warn_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        fprintf(stderr, "WARNING: writing 0x%02x from pvr2 core register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        fprintf(stderr, "WARNING: writing 0x%04x to pvr2 core register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        fprintf(stderr, "WARNING: writing 0x%08x to pvr2 core register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        fprintf(stderr, "WARNING: reading from pvr2 core register %s\n",
                reg_info->reg_name);
    }

    return default_pvr2_core_reg_write_handler(reg_info, buf, addr, len);
}

static int
pvr2_core_id_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    /* hardcoded hardware ID */
    uint32_t tmp = 0x17fd11db;

    memcpy(buf, &tmp, len);

    return MEM_ACCESS_SUCCESS;
}

static int
pvr2_core_revision_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    uint32_t tmp = 17;

    memcpy(buf, &tmp, len);

    return MEM_ACCESS_SUCCESS;
}

static int
pvr2_core_read_only_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    error_set_feature("whatever happens when you write to "
                      "a read-only register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
fb_r_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_ctrl, sizeof(fb_r_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    reg32_t new_val;
    memcpy(&new_val, buf, sizeof(new_val));

    if (new_val & PVR2_VCLK_DIV_MASK)
        spg_set_pclk_div(1);
    else
        spg_set_pclk_div(2);

    spg_set_pix_double_y((bool)(new_val & PVR2_LINE_DOUBLE_MASK));

    memcpy(&fb_r_ctrl, buf, sizeof(fb_r_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
vo_control_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    return warn_pvr2_core_reg_read_handler(reg_info, buf, addr, len);
}

static int
vo_control_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    reg32_t new_val;
    memcpy(&new_val, buf, sizeof(new_val));

    spg_set_pix_double_x((bool)(new_val & PVR2_PIXEL_DOUBLE_MASK));

    return warn_pvr2_core_reg_write_handler(reg_info, buf, addr, len);
}

static int
fb_r_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_sof1, sizeof(fb_r_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_r_sof1, buf, sizeof(fb_r_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_sof2, sizeof(fb_r_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_r_sof2, buf, sizeof(fb_r_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_size_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_size, sizeof(fb_r_size));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_size_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_r_size, buf, sizeof(fb_r_size));
    return MEM_ACCESS_SUCCESS;
}

uint32_t get_fb_r_sof1() {
    return fb_r_sof1;
}

uint32_t get_fb_r_sof2() {
    return fb_r_sof2;
}

uint32_t get_fb_r_ctrl() {
    return fb_r_ctrl;
}

uint32_t get_fb_r_size() {
    return fb_r_size;
}

uint32_t get_fb_w_sof1() {
    return fb_w_sof1;
}

uint32_t get_fb_w_sof2() {
    return fb_w_sof2;
}

uint32_t get_fb_w_ctrl() {
    return fb_w_ctrl;
}

uint32_t get_fb_w_linestride() {
    return fb_w_linestride & 0x1ff;
}

static int
fb_w_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_sof1, sizeof(fb_w_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_sof1, buf, sizeof(fb_w_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_sof2, sizeof(fb_w_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_sof2, buf, sizeof(fb_w_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_ctrl, sizeof(fb_w_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_ctrl, buf, sizeof(fb_w_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_linestride_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_linestride, sizeof(fb_w_linestride));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_linestride_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_linestride, buf, sizeof(fb_w_linestride));
    return MEM_ACCESS_SUCCESS;
}
