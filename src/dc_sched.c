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

#include <stddef.h>
#include <string.h>

#include "error.h"
#include "hw/sh4/sh4.h" // for SH4_CLOCK_SCALE
#include "dreamcast.h"

#include "dc_sched.h"

static DEF_ERROR_U64_ATTR(current_dc_cycle_stamp)
static DEF_ERROR_U64_ATTR(event_sched_dc_cycle_stamp)

void dc_clock_init(struct dc_clock *clk) {
    memset(clk, 0, sizeof(*clk));
}

void dc_clock_cleanup(struct dc_clock *clk) {
}

static void update_target_stamp(struct dc_clock *clock) {
    if (clock->ev_next_priv) {
        clock->target_stamp_priv = clock->ev_next_priv->when;
    } else {
        /*
         * Somehow there are no events scheduled.
         *
         * Hard to say what to do here.  Constantly checking to see if
         * a new event got pushed would be costly.  Instead I just run
         * the cpu a little, but not so much that I drastically overrun
         * anything that might get scheduled.  The number of cycles to
         * run here is arbitrary, but if it's too low then performance
         * will be negatively impacted and if it's too high then
         * accuracy will be negatively impacted.
         *
         * TBH, I'm not even 100% sure this problem can even happen since
         * there's no way to turn off SPG, TMU, etc.
         */
        clock->target_stamp_priv = clock_cycle_stamp(clock) + 16 * SH4_CLOCK_SCALE;
    }
}

void sched_event(struct dc_clock *clock, struct SchedEvent *event) {
#ifdef INVARIANTS
    /*
     * check to make sure the event isn't being scheduled after it should have
     * already executed.
     */
    dc_cycle_stamp_t cur_stamp = clock_cycle_stamp(clock);
    if (event->when < cur_stamp) {
        error_set_current_dc_cycle_stamp(cur_stamp);
        error_set_event_sched_dc_cycle_stamp(event->when);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    struct SchedEvent *next_ptr = clock->ev_next_priv;
    struct SchedEvent **pprev_ptr = &clock->ev_next_priv;
    while (next_ptr && next_ptr->when < event->when) {
        pprev_ptr = &next_ptr->next_event;
        next_ptr = next_ptr->next_event;
    }
    *pprev_ptr = event;
    if (next_ptr)
        next_ptr->pprev_event = &event->next_event;
    event->next_event = next_ptr;
    event->pprev_event = pprev_ptr;

    update_target_stamp(clock);
}

void cancel_event(struct dc_clock *clock, struct SchedEvent *event) {
#ifdef INVARIANTS
    /*
     * check to make sure the event isn't being canceled after it should have
     * already executed.
     */
    dc_cycle_stamp_t cur_stamp = clock_cycle_stamp(clock);
    if (event->when < cur_stamp) {
        error_set_current_dc_cycle_stamp(cur_stamp);
        error_set_event_sched_dc_cycle_stamp(event->when);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    if (event->next_event)
        event->next_event->pprev_event = event->pprev_event;
    *event->pprev_event = event->next_event;

    // XXX this is unnecessary, but I'm trying to be extra-safe here
    event->next_event = NULL;
    event->pprev_event = NULL;

    update_target_stamp(clock);
}

struct SchedEvent *pop_event(struct dc_clock *clock) {
    struct SchedEvent *ev_ret = clock->ev_next_priv;

#ifdef INVARIANTS
    /*
     * check to make sure the event isn't being canceled after it should have
     * already executed.
     */
    dc_cycle_stamp_t cur_stamp = clock_cycle_stamp(clock);
    if (ev_ret && (ev_ret->when < cur_stamp)) {
        error_set_current_dc_cycle_stamp(cur_stamp);
        error_set_event_sched_dc_cycle_stamp(ev_ret->when);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    if (clock->ev_next_priv) {
        clock->ev_next_priv = clock->ev_next_priv->next_event;
        if (clock->ev_next_priv)
            clock->ev_next_priv->pprev_event = &clock->ev_next_priv;
    }

    // XXX this is unnecessary, but I'm trying to be extra-safe here
    if (ev_ret) {
        ev_ret->next_event = NULL;
        ev_ret->pprev_event = NULL;
    }

    update_target_stamp(clock);

    return ev_ret;
}

struct SchedEvent *peek_event(struct dc_clock *clock) {
    return clock->ev_next_priv;
}

dc_cycle_stamp_t *clock_get_target_pointer(struct dc_clock *clock) {
    return &clock->target_stamp_priv;
}

dc_cycle_stamp_t *clock_get_cycle_stamp_pointer(struct dc_clock *clock) {
    return &clock->cycle_stamp_priv;
}

dc_cycle_stamp_t clock_target_stamp(struct dc_clock *clock) {
    return clock->target_stamp_priv;
}
