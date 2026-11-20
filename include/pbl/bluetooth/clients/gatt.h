/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/section.h>
#include <pbl/util/size.h>

#include <host/ble_uuid.h>

struct os_mbuf;

/**
 * @defgroup bluetooth_gatt_client GATT client helpers
 * @ingroup bluetooth
 * @brief What clients of a peer's GATT service share: finding the service, and being handed its
 * notifications.
 *
 * A client finds the characteristics of the one service it is for, keeps their handles and
 * uses them with the stack directly:
 *
 * @code{.c}
 * static const ble_uuid_t *const s_chrs[] = {&s_foo_uuid.u, &s_bar_uuid.u};
 *
 * static void prv_found(uint16_t conn_handle, int status, const struct pbl_bt_gattc_chr *chrs,
 *                       void *arg) {
 *   if (status == 0) {
 *     uint16_t value = htole16(BLE_GATT_CCCD_NOTIFY);
 *     ble_gattc_write_flat(conn_handle, chrs[0].cccd_handle, &value, sizeof(value), prv_cb, arg);
 *   }
 * }
 *
 * pbl_bt_gattc_discover(conn_handle, &s_svc_uuid.u, s_chrs, 2, prv_found, nullptr);
 * @endcode
 * @{
 */

/** @brief A characteristic found by pbl_bt_gattc_discover(). */
struct pbl_bt_gattc_chr {
  /** Handle of its value, 0 if the service does not have the characteristic. */
  uint16_t val_handle;
  /** Handle of its Client Characteristic Configuration descriptor, 0 if it has none. */
  uint16_t cccd_handle;
  /** Its properties (@c BLE_GATT_CHR_PROP_*). */
  uint8_t properties;
};

/**
 * @brief Called when pbl_bt_gattc_discover() is done, on the host task.
 *
 * @param conn_handle Connection.
 * @param status 0 if the service was found, @c BLE_HS_ENOENT if the peer does not have it, or
 * what failed.
 * @param chrs The characteristics, in the order they were asked for; valid during the call.
 * @param arg As passed to pbl_bt_gattc_discover().
 */
typedef void (*pbl_bt_gattc_discover_cb_t)(uint16_t conn_handle, int status,
                                           const struct pbl_bt_gattc_chr *chrs, void *arg);

/**
 * @brief Find a service of the peer and characteristics of it.
 *
 * Discoveries run one at a time, in the order asked for. May be called from any thread.
 *
 * @param conn_handle Connection.
 * @param svc_uuid The service.
 * @param chr_uuids The characteristics; the array and the UUIDs must stay valid until @p cb is
 * called.
 * @param num_chrs Number of characteristics.
 * @param cb Called with the outcome, exactly once if this returns 0.
 * @param arg Passed to @p cb.
 * @return 0 if queued, a @c BLE_HS_* code otherwise.
 */
int pbl_bt_gattc_discover(uint16_t conn_handle, const ble_uuid_t *svc_uuid,
                          const ble_uuid_t *const *chr_uuids, uint8_t num_chrs,
                          pbl_bt_gattc_discover_cb_t cb, void *arg);

struct pbl_bt_gattc_service;

/**
 * @brief What a service client reports; any may be null.
 *
 * Characteristics are told by their index among those of the service client. A status is 0, the
 * ATT error code the peer answered with (positive), or a negative errno. Called on the host
 * task, and not after pbl_bt_gattc_service_stop() returns, but for a call already running.
 */
struct pbl_bt_gattc_service_cb {
  /**
   * @brief The search for the service ended.
   *
   * @param status 0 if the peer has it with the characteristics required, -ENOENT if not, or
   * what failed.
   */
  void (*discovered)(struct pbl_bt_gattc_service *svc, int status);

  /** @brief A pbl_bt_gattc_service_subscribe() was answered. */
  void (*subscribed)(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable, int status);

  /** @brief The peer notified or indicated a value, valid during the call. */
  void (*notified)(struct pbl_bt_gattc_service *svc, uint8_t chr, const uint8_t *data,
                   uint16_t len);

  /** @brief A pbl_bt_gattc_service_write() was answered. */
  void (*written)(struct pbl_bt_gattc_service *svc, uint8_t chr, int status);

  /** @brief A pbl_bt_gattc_service_read() was answered, with the value if @p status is 0. */
  void (*read)(struct pbl_bt_gattc_service *svc, uint8_t chr, int status, const uint8_t *data,
               uint16_t len);
};

/**
 * @brief A client of one service of one connection at a time.
 *
 * Define it with PBL_BT_GATTC_SERVICE_DEFINE(), and hand the notifications of the connection to
 * pbl_bt_gattc_service_notify_rx() from a listener.
 */
struct pbl_bt_gattc_service {
  /** The service. */
  const ble_uuid_t *svc_uuid;
  /** Its characteristics. */
  const ble_uuid_t *const *chr_uuids;
  /** Which of them the service may lack, as a bit mask of indexes. */
  uint32_t optional_chrs;
  /** Which of them must have a CCCD, as a bit mask of indexes. */
  uint32_t notifying_chrs;
  /** Number of characteristics. */
  uint8_t num_chrs;
  /** What was found of them. */
  struct pbl_bt_gattc_chr *chrs;
  /** @cond INTERNAL_HIDDEN */
  const struct pbl_bt_gattc_service_cb *cb;
  uint16_t conn_handle;
  bool started;
  uint32_t generation;
  uint32_t session;
  /** @endcond */
};

/**
 * @brief Define a service client.
 *
 * @param _name Name of the struct pbl_bt_gattc_service.
 * @param _svc_uuid The service, a @c ble_uuid_t pointer.
 * @param _chr_uuids Its characteristics, an array of @c ble_uuid_t pointers.
 * @param _optional Bit mask of the characteristics the service may lack.
 * @param _notifying Bit mask of the characteristics that must have a CCCD.
 */
#define PBL_BT_GATTC_SERVICE_DEFINE(_name, _svc_uuid, _chr_uuids, _optional, _notifying) \
  static struct pbl_bt_gattc_chr _name##_chrs[PBL_ARRAY_SIZE(_chr_uuids)];               \
  static struct pbl_bt_gattc_service _name = {                                           \
    .svc_uuid = (_svc_uuid),                                                             \
    .chr_uuids = (_chr_uuids),                                                           \
    .optional_chrs = (_optional),                                                        \
    .notifying_chrs = (_notifying),                                                      \
    .num_chrs = PBL_ARRAY_SIZE(_chr_uuids),                                              \
    .chrs = _name##_chrs,                                                                \
  }

/**
 * @brief Look for the service on a connection.
 *
 * @param svc Service client.
 * @param conn_handle Connection.
 * @param cb Callbacks, which must stay valid.
 * @retval 0 Looking for it, @c discovered follows.
 * @retval -EALREADY Started already.
 * @retval -ENOMEM Out of memory.
 */
int pbl_bt_gattc_service_start(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                               const struct pbl_bt_gattc_service_cb *cb);

/**
 * @brief Look for the service again, as when the peer says its services changed.
 *
 * What was found stays in use meanwhile. @c discovered is only called if the outcome differs:
 * with 0 if the service is there now or has other handles, and its subscriptions are to be
 * made again, or with why not if it is gone. What was asked of the service where it was is
 * then not answered.
 *
 * @param svc Service client.
 * @retval 0 Looking for it.
 * @retval -ENOTCONN Not started.
 * @retval -ENOMEM Out of memory.
 */
int pbl_bt_gattc_service_rediscover(struct pbl_bt_gattc_service *svc);

/** @brief Forget the service and the connection. */
void pbl_bt_gattc_service_stop(struct pbl_bt_gattc_service *svc);

/**
 * @brief Subscribe to the notifications of a characteristic, or unsubscribe.
 *
 * @retval 0 Asked, @c subscribed follows.
 * @retval -ENOTCONN The characteristic, or its CCCD, was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_gattc_service_subscribe(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable);

/**
 * @brief Write the value of a characteristic.
 *
 * @retval 0 Written, @c written follows.
 * @retval -ENOTCONN The characteristic was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_gattc_service_write(struct pbl_bt_gattc_service *svc, uint8_t chr, const void *data,
                               uint16_t len);

/**
 * @brief Read the value of a characteristic, all of it however long.
 *
 * @retval 0 Asked, @c read follows.
 * @retval -ENOTCONN The characteristic was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_gattc_service_read(struct pbl_bt_gattc_service *svc, uint8_t chr);

/**
 * @brief Take a notification or indication if it is of a characteristic of the service.
 *
 * @return true if it was, and @c notified was called.
 */
bool pbl_bt_gattc_service_notify_rx(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                                    uint16_t attr_handle, struct os_mbuf *om);

/** @brief A listener of the notifications and indications of peers. */
struct pbl_bt_gattc_notify_listener {
  /**
   * @brief Take a notification or indication, on the host task.
   *
   * @param conn_handle Connection.
   * @param attr_handle Handle of the characteristic value.
   * @param om The value, which stays the caller's.
   * @return true if it was for this listener.
   */
  bool (*fn)(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om);
};

/**
 * @brief Define a listener of the notifications and indications of peers.
 *
 * Listeners are offered each one in turn by pbl_bt_gattc_notify_rx(), until one takes it.
 *
 * @param _name Name of the listener.
 * @param _fn Its function, see struct pbl_bt_gattc_notify_listener.
 */
#define PBL_BT_GATTC_NOTIFY_LISTENER_DEFINE(_name, _fn)                         \
  static const struct pbl_bt_gattc_notify_listener pbl_bt_gattc_notify_##_name  \
  PBL_ITERABLE_SECTION(const struct pbl_bt_gattc_notify_listener, bt_gattc_ntf, \
                       #_name) = {.fn = (_fn)}

/**
 * @brief Offer a notification or indication of a peer to the listeners.
 *
 * Call it for each @c BLE_GAP_EVENT_NOTIFY_RX of the connections whose GAP events the caller
 * handles.
 *
 * @param conn_handle Connection.
 * @param attr_handle Handle of the characteristic value.
 * @param om The value.
 * @return true if a listener took it.
 */
bool pbl_bt_gattc_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om);

/** @} */
