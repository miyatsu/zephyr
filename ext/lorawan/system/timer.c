/*
 * Copyright (c) 2018 Ding Tao
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <kernel.h>
#include <misc/util.h>

#include "timer.h"

void timeout_callback(struct k_timer *timer)
{
	void (*callback)(void) = CONTAINER_OF(timer, struct timer_event, cb);
	callback();
}

void TimerInit(struct timer_event *event, void (*callback)(void))
{
	k_timer_init(&event->timer, timeout_callback, NULL);
	event->cb = callback;
}

void TimerStart(struct timer_event *event)
{
	k_timer_start(&event->timer, event->period, event->period);
}

void TimerStop(struct timer_event *event)
{
	k_timer_stop(&event->timer);
}

void TimerSetValue(struct timer_event *event, uint32_t value)
{
	event->period = value;
}

TimerTime_t TimerGetCurrentTime(void)
{
	return k_uptime_get_32();
}

TimerTime_t TimerGetElapsedTime(TimerTime_t saved_time)
{
	return k_uptime_delta_32(&saved_time);
}
