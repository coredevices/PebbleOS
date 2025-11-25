/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _NPL_PEBBLE_H_
#define _NPL_PEBBLE_H_

struct ble_npl_event *npl_pebble_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo);

void npl_pebble_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev);

void npl_pebble_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev);

ble_npl_error_t npl_pebble_mutex_init(struct ble_npl_mutex *mu);

ble_npl_error_t npl_pebble_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout);

ble_npl_error_t npl_pebble_mutex_release(struct ble_npl_mutex *mu);

ble_npl_error_t npl_pebble_sem_init(struct ble_npl_sem *sem, uint16_t tokens);

ble_npl_error_t npl_pebble_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout);

ble_npl_error_t npl_pebble_sem_release(struct ble_npl_sem *sem);

void npl_pebble_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                             ble_npl_event_fn *ev_cb, void *ev_arg);

ble_npl_error_t npl_pebble_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks);

void npl_pebble_callout_stop(struct ble_npl_callout *co);

bool npl_pebble_callout_is_active(struct ble_npl_callout *co);

ble_npl_error_t npl_pebble_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks);

ble_npl_time_t npl_pebble_callout_get_ticks(struct ble_npl_callout *co);

ble_npl_time_t npl_pebble_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t now);

ble_npl_error_t npl_pebble_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks);

ble_npl_error_t npl_pebble_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms);

void npl_pebble_hw_set_isr(int irqn, void (*addr)(void));

uint32_t npl_pebble_hw_enter_critical(void);

void npl_pebble_hw_exit_critical(uint32_t ctx);

#endif /* _NPL_PEBBLE_H_ */
