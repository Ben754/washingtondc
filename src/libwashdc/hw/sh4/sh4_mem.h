/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef SH4_MEM_H_
#define SH4_MEM_H_

#include <assert.h>

#include "sh4_excp.h"
#include "mem_code.h"
#include "washdc/error.h"
#include "washdc/MemoryMap.h"

struct Sh4;

enum VirtMemArea {
    SH4_AREA_P0 = 0,
    SH4_AREA_P1,
    SH4_AREA_P2,
    SH4_AREA_P3,
    SH4_AREA_P4
};

// Physical memory aread boundaries
#define SH4_AREA_P0_FIRST  0x00000000
#define SH4_AREA_P0_LAST   0x7fffffff
#define SH4_AREA_P1_FIRST  0x80000000
#define SH4_AREA_P1_LAST   0x9fffffff
#define SH4_AREA_P2_FIRST  0xa0000000
#define SH4_AREA_P2_LAST   0xbfffffff
#define SH4_AREA_P3_FIRST  0xc0000000
#define SH4_AREA_P3_LAST   0xdfffffff
#define SH4_AREA_P4_FIRST  0xe0000000
#define SH4_AREA_P4_LAST   0xffffffff

/*
 * SH4_P4_REGSTART is the addr of the first memory-mapped
 *     register in area 7
 * SH4_P4_REGEND is the first addr *after* the last memory-mapped
 *     register in the p4 area.
 * SH4_AREA7_REGSTART is the addr of the first memory-mapped
 *     register in area 7
 * SH4_AREA7_REGEND is the first addr *after* the last memory-mapped
 *     register in area 7
 */
#define SH4_P4_REGSTART    0xff000000
#define SH4_P4_REGEND      0xfff00008
#define SH4_AREA7_REGSTART 0x1f000000
#define SH4_AREA7_REGEND   0x1ff00008
static_assert((SH4_P4_REGEND - SH4_P4_REGSTART) ==
              (SH4_AREA7_REGEND - SH4_AREA7_REGSTART),
              "AREA7 is not the same size as the P4 area");

/* constants needed for opcache as ram */
#define SH4_LONGS_PER_OP_CACHE_LINE 8
#define SH4_OP_CACHE_LINE_SIZE (SH4_LONGS_PER_OP_CACHE_LINE * 4)
#define SH4_OC_RAM_AREA_SIZE (8 * 1024)

void sh4_mem_init(Sh4 *sh4);
void sh4_mem_cleanup(Sh4 *sh4);

struct sh4_mem {
    struct memory_map *map;
};

void sh4_set_mem_map(struct Sh4 *sh4, struct memory_map *map);

extern struct memory_interface sh4_p4_intf;

#endif
