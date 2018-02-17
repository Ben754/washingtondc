/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <stdio.h>
#include <stdlib.h>

#include "gfx/geo_buf.h"
#include "gfx/gfx_tex_cache.h"
#include "gfx/opengl/opengl_target.h"
#include "gfx/opengl/opengl_renderer.h"
#include "dreamcast.h"
#include "log.h"

#include "rend_common.h"

static unsigned frame_stamp;

static struct rend_if const *rend_ifp = &opengl_rend_if;

// initialize and clean up the graphics renderer
void rend_init(void) {
    rend_ifp->init();
}

void rend_cleanup(void) {
    rend_ifp->cleanup();
}

// tell the renderer to update the given texture from the cache
void rend_update_tex(unsigned tex_no, void const *tex_dat) {
    rend_ifp->update_tex(tex_no, tex_dat);
}

// tell the renderer to release the given texture from the cache
void rend_release_tex(unsigned tex_no) {
    rend_ifp->release_tex(tex_no);
}

// draw the given geo_buf
void rend_do_draw_geo_buf(struct geo_buf *geo) {
    rend_ifp->do_draw_geo_buf(geo);
}

void rend_draw_geo_buf(struct geo_buf *geo) {
    unsigned tex_no;
    for (tex_no = 0; tex_no < GEO_BUF_TEX_CACHE_SIZE; tex_no++) {
        struct geo_buf_tex *tex = geo->tex_cache + tex_no;
        if (tex->state == GEO_BUF_TEX_DIRTY) {
            struct gfx_tex new_tex_entry = {
                .valid =  true,
                .pix_fmt = tex->pix_fmt,
                .w_shift = tex->w_shift,
                .h_shift = tex->h_shift
            };
            tex->state = GEO_BUF_TEX_READY;
            gfx_tex_cache_add(tex_no, &new_tex_entry, tex->dat);
            free(tex->dat);
            tex->dat = NULL;
        } else if (tex->state == GEO_BUF_TEX_INVALID) {
            gfx_tex_cache_evict(tex_no);
        }
    }

    opengl_target_begin(geo->screen_width, geo->screen_height);
    rend_do_draw_geo_buf(geo);
    opengl_target_end();

    frame_stamp = geo->frame_stamp;

    LOG_DBG("frame_stamp %u rendered\n", frame_stamp);

    enum display_list_type disp_list;
    for (disp_list = DISPLAY_LIST_FIRST; disp_list < DISPLAY_LIST_COUNT;
         disp_list++) {
        struct display_list *list = geo->lists + disp_list;
        if (list->n_groups) {
            /*
             * current protocol is that list->groups is only valid if
             * list->n_groups is non-valid; ergo it's safe to leave a
             * hangning pointer here.
             */
            free(list->groups);
            list->n_groups = 0;
        }
    }
}

void rend_sort_groups(unsigned *order, struct poly_group const *group,
                      unsigned n_groups) {
    unsigned group_no, cmp_no;
    for (group_no = 0; group_no < n_groups; group_no++)
        order[group_no] = group_no;

    if (!n_groups)
        return;

    // use a simple insertion sort algorithm because i'm a plebian
    for (group_no = 0; group_no < n_groups - 1; group_no++) {
        for (cmp_no = group_no + 1; cmp_no < n_groups; cmp_no++) {
            unsigned group_idx = order[group_no];
            unsigned cmp_idx = order[cmp_no];
            if ((1.0 / group[cmp_idx].barycenter_depth) <
                (1.0 / group[group_idx].barycenter_depth)) {
                order[group_no] = cmp_idx;
                order[cmp_no] = group_idx;
            }
        }
    }
}
