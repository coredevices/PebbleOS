/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>
#include <process_management/app_manager.h>

/**
 * @defgroup services_event_service Event service
 * @ingroup services
 * @brief Fans out kernel events to the tasks subscribed to them.
 *
 * Tasks subscribe to event types through the applib event service client, which posts
 * subscription events to KernelMain. KernelMain then passes every event it handles to
 * event_service_handle_event(), which copies it to the queue of each subscribed task and handles
 * it inline when the current task is subscribed.
 *
 * Kernel services that produce an event type can register callbacks to start and stop the
 * underlying hardware or work as subscribers come and go:
 *
 * @code{.c}
 * static void prv_start(PebbleTask task) {
 *   // first or additional subscriber: enable the sensor
 * }
 *
 * static void prv_stop(PebbleTask task) {
 *   // subscriber gone: disable the sensor if it was the last one
 * }
 *
 * void my_service_init(void) {
 *   event_service_init(PEBBLE_COMPASS_DATA_EVENT, prv_start, prv_stop);
 * }
 * @endcode
 *
 * Events carrying a heap buffer keep it alive until every recipient task has processed the
 * event. A recipient that needs the buffer for longer claims it with
 * event_service_claim_buffer() and later releases it with event_service_free_claimed_buffer().
 * @{
 */

/**
 * @brief Called on KernelMain when a task subscribes to an event type.
 *
 * @param task Subscribing task.
 */
typedef void (*EventServiceAddSubscriberCallback)(PebbleTask task);

/**
 * @brief Called on KernelMain when a task unsubscribes from an event type.
 *
 * @param task Unsubscribing task.
 */
typedef void (*EventServiceRemoveSubscriberCallback)(PebbleTask task);

/** @brief Initialize the event service, once during system startup. */
void event_service_system_init(void);

/**
 * @brief Register an event type and its subscriber callbacks.
 *
 * Called by kernel services producing @p type, typically at their initialization. Registering
 * again replaces the previous registration and drops its subscribers. Event types without a
 * registration get one without callbacks on first subscription.
 *
 * @param type Event type.
 * @param start_cb Called when a task subscribes, or NULL.
 * @param stop_cb Called when a task unsubscribes, or NULL.
 */
void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback start_cb,
                        EventServiceRemoveSubscriberCallback stop_cb);

/**
 * @brief Check whether an event type has subscribers.
 *
 * @param event_type Event type.
 * @return true if at least one task is subscribed.
 */
bool event_service_is_running(PebbleEventType event_type);

/**
 * @brief Deliver an event to its subscribers.
 *
 * Called from the KernelMain event loop. Tasks masked out by the event's task mask are skipped.
 * If a subscriber's queue is full, non-release builds close an unprivileged app or worker and
 * assert otherwise.
 *
 * @param e Event to deliver.
 */
void event_service_handle_event(PebbleEvent *e);

/**
 * @brief Subscribe to an event type on behalf of a task.
 *
 * Must be called from KernelMain.
 *
 * @param subscription Subscription to add.
 */
void event_service_subscribe_from_kernel_main(PebbleSubscriptionEvent *subscription);

/**
 * @brief Handle a subscription or unsubscription event.
 *
 * Called from the KernelMain event loop.
 *
 * @param subscription Subscription to add or remove.
 */
void event_service_handle_subscription(PebbleSubscriptionEvent *subscription);

/**
 * @brief Remove all subscriptions of a task.
 *
 * @param task Task whose subscriptions are removed, e.g. an exiting process.
 */
void event_service_clear_process_subscriptions(PebbleTask task);

/**
 * @brief Claim the heap buffer of an event so it is not freed with the event.
 *
 * Only one claim per buffer is supported.
 *
 * @param e Event whose buffer is claimed.
 * @return Reference to pass to event_service_free_claimed_buffer(), or NULL if the event has no
 *         tracked buffer or it is already claimed.
 */
void *event_service_claim_buffer(PebbleEvent *e);

/**
 * @brief Release a buffer claimed with event_service_claim_buffer().
 *
 * The buffer is freed once no recipient task still needs it.
 *
 * @param ref Reference returned by event_service_claim_buffer(), or NULL.
 */
void event_service_free_claimed_buffer(void *ref);

/**
 * @brief Check whether a buffer is tracked by the event service.
 *
 * True for kernel-allocated buffers attached to an in-flight event. Syscalls that dereference an
 * event's embedded data pointer use it to reject pointers fabricated by the caller.
 *
 * @param buf Buffer to check.
 * @return true if @p buf is a tracked event buffer.
 */
bool event_service_is_known_buffer(const void *buf);

/** @} */
