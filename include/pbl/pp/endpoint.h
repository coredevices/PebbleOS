/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>
#include <pbl/pp/protocol.h>

/**
 * @defgroup pp_endpoint Endpoints
 * @ingroup pp
 * @brief Handlers of inbound messages, one per endpoint ID.
 *
 * An endpoint is defined next to the code that handles it and collected at link time:
 *
 * @code{.c}
 * static void prv_ping_handler(struct pbl_pp_session *session, const uint8_t *data,
 *                              size_t length) {
 *   ...
 * }
 *
 * PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_PING, PBL_PP_ACCESS_ANY, prv_ping_handler);
 * @endcode
 *
 * The handler runs on SysWorkq once the whole message has arrived, with a copy of the payload on
 * the kernel heap. PBL_PP_ENDPOINT_DEFINE_MAIN() runs it on KernelMain instead, for handlers whose
 * events must stay ordered with the session events. Endpoints that need their own buffering
 * provide a @ref pbl_pp_receiver_api with PBL_PP_ENDPOINT_DEFINE_RECEIVER().
 * @{
 */

struct pbl_pp_session;
struct pbl_pp_endpoint;

/** @brief Who may send messages to an endpoint. */
enum pbl_pp_access : uint8_t {
  /** No one. */
  PBL_PP_ACCESS_NONE = 0,
  /** Third party phone apps. */
  PBL_PP_ACCESS_PUBLIC = 1U << 0,
  /** The Pebble mobile app. */
  PBL_PP_ACCESS_PRIVATE = 1U << 1,
  /** Anyone. */
  PBL_PP_ACCESS_ANY = PBL_PP_ACCESS_PUBLIC | PBL_PP_ACCESS_PRIVATE,
};

/** @brief Task the default receiver runs an endpoint handler on. */
enum pbl_pp_handler_task : uint8_t {
  /** SysWorkq. */
  PBL_PP_HANDLER_BG,
  /** KernelMain. */
  PBL_PP_HANDLER_MAIN,
};

/**
 * @brief Endpoint handler.
 *
 * @param session Session the message arrived on.
 * @param data Message payload.
 * @param length Length of @p data in bytes.
 */
typedef void (*pbl_pp_endpoint_handler_t)(struct pbl_pp_session *session, const uint8_t *data,
                                          size_t length);

/**
 * @brief Context of the message a receiver is buffering.
 *
 * Its contents are up to the @ref pbl_pp_receiver_api. Messages are not interleaved within a
 * session, so each session has at most one receiver context at a time.
 */
struct pbl_pp_receiver;

/**
 * @brief Buffers inbound payloads and hands complete messages over.
 *
 * Callbacks run in the context of the transport that received the data. Several sessions may be
 * receiving messages at once. All callbacks are mandatory.
 */
struct pbl_pp_receiver_api {
  /**
   * @brief Prepare a context for a new message.
   *
   * @param session Session receiving the message.
   * @param endpoint Destination endpoint.
   * @param length Payload length in bytes.
   * @return Receiver context, or NULL to drop the message.
   */
  struct pbl_pp_receiver *(*prepare)(struct pbl_pp_session *session,
                                     const struct pbl_pp_endpoint *endpoint, size_t length);

  /**
   * @brief Buffer payload data of the current message.
   *
   * @param receiver Receiver context.
   * @param data Payload data.
   * @param length Length of @p data in bytes.
   */
  void (*write)(struct pbl_pp_receiver *receiver, const uint8_t *data, size_t length);

  /**
   * @brief Hand the complete message over and release the context.
   *
   * @param receiver Receiver context.
   */
  void (*finish)(struct pbl_pp_receiver *receiver);

  /**
   * @brief Drop the current message because the session closed, and release the context.
   *
   * @param receiver Receiver context.
   */
  void (*cleanup)(struct pbl_pp_receiver *receiver);
};

/**
 * @brief The default receiver.
 *
 * Copies the payload to the kernel heap and runs the endpoint's handler on the endpoint's task.
 * Receivers that hand some messages over to a handler can delegate to it.
 */
extern const struct pbl_pp_receiver_api pbl_pp_kernel_receiver;

/** @brief An endpoint. Defined with PBL_PP_ENDPOINT_DEFINE() and its variants. */
struct pbl_pp_endpoint {
  /** Handler, used by the default receiver. */
  pbl_pp_endpoint_handler_t handler;
  /** Receiver, NULL for the default one. */
  const struct pbl_pp_receiver_api *receiver;
  /** Endpoint ID. */
  uint16_t id;
  /** Senders allowed to use the endpoint. */
  enum pbl_pp_access access;
  /** Task the default receiver runs @ref handler on. */
  enum pbl_pp_handler_task task;
};

/** @cond INTERNAL_HIDDEN */
#define PBL_PP__CAT(_a, _b)        PBL_PP__CAT_(_a, _b)
#define PBL_PP__CAT_(_a, _b)       _a##_b
#define PBL_PP__STR(_a)            PBL_PP__STR_(_a)
#define PBL_PP__STR_(_a)           #_a
#define PBL_PP__ENDPOINT_NAME(_id) PBL_PP__CAT(pbl_pp_endpoint_, _id)

#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_PP__ENDPOINT_SECTION(_id) PBL_UNSORTED_SECTION(pbl_pp_ep)
#else
#define PBL_PP__ENDPOINT_SECTION(_id) \
  PBL_ALIGNED(4) PBL_SECTION(".pbl_pp_endpoints." PBL_PP__STR(_id))
#endif

#define PBL_PP__ENDPOINT_DEFINE(_id, _access, _handler, _receiver, _task) \
  extern const struct pbl_pp_endpoint PBL_PP__ENDPOINT_NAME(_id);         \
  const struct pbl_pp_endpoint PBL_PP__ENDPOINT_NAME(_id)                 \
      PBL_USED PBL_PP__ENDPOINT_SECTION(_id) = {                          \
        .handler = (_handler),                                            \
        .receiver = (_receiver),                                          \
        .id = (_id),                                                      \
        .access = (_access),                                              \
        .task = (_task),                                                  \
  }
/** @endcond */

/**
 * @brief Define an endpoint whose handler runs on SysWorkq.
 *
 * @param _id Endpoint ID, one of the PBL_PP_ENDPOINT_ macros.
 * @param _access Senders allowed to use the endpoint, a @ref pbl_pp_access.
 * @param _handler Handler, a @ref pbl_pp_endpoint_handler_t.
 */
#define PBL_PP_ENDPOINT_DEFINE(_id, _access, _handler) \
  PBL_PP__ENDPOINT_DEFINE(_id, _access, _handler, NULL, PBL_PP_HANDLER_BG)

/**
 * @brief Define an endpoint whose handler runs on KernelMain.
 *
 * @param _id Endpoint ID, one of the PBL_PP_ENDPOINT_ macros.
 * @param _access Senders allowed to use the endpoint, a @ref pbl_pp_access.
 * @param _handler Handler, a @ref pbl_pp_endpoint_handler_t.
 */
#define PBL_PP_ENDPOINT_DEFINE_MAIN(_id, _access, _handler) \
  PBL_PP__ENDPOINT_DEFINE(_id, _access, _handler, NULL, PBL_PP_HANDLER_MAIN)

/**
 * @brief Define an endpoint with its own receiver.
 *
 * @param _id Endpoint ID, one of the PBL_PP_ENDPOINT_ macros.
 * @param _access Senders allowed to use the endpoint, a @ref pbl_pp_access.
 * @param _receiver Receiver, a @ref pbl_pp_receiver_api.
 */
#define PBL_PP_ENDPOINT_DEFINE_RECEIVER(_id, _access, _receiver) \
  PBL_PP__ENDPOINT_DEFINE(_id, _access, NULL, &(_receiver), PBL_PP_HANDLER_BG)

/** @} */
