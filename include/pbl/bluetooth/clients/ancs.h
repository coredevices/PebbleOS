/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/ancs.h>
#include <pbl/bluetooth/clients/gatt.h>

/**
 * @defgroup bluetooth_ancs_client ANCS client
 * @ingroup bluetooth
 * @brief Client of the Apple Notification Center Service of a connected iOS device.
 *
 * A client serves one connection at a time: define one for each connection to serve at once.
 * It finds the service, subscribes to its sources, reports the notifications the device adds,
 * modifies and removes, and on request gets their attributes and those of their apps, and
 * performs their actions. The service requires an encrypted link: start the client once the
 * link is.
 *
 * @code{.c}
 * PBL_BT_ANCS_CLIENT_DEFINE(s_client, 256);
 *
 * static const struct pbl_bt_ancs_attr_req s_attrs[] = {
 *   {.id = PBL_BT_ANCS_NOTIF_ATTR_TITLE, .max_len = 32},
 *   {.id = PBL_BT_ANCS_NOTIF_ATTR_MESSAGE, .max_len = 96},
 * };
 *
 * static void prv_discovered(struct pbl_bt_ancs_client *client, int status) {
 *   if (status == 0) {
 *     pbl_bt_ancs_client_subscribe(client, PBL_BT_ANCS_SOURCE_DATA, true);
 *     pbl_bt_ancs_client_subscribe(client, PBL_BT_ANCS_SOURCE_NOTIFICATION, true);
 *   }
 * }
 *
 * static void prv_notification(struct pbl_bt_ancs_client *client,
 *                              const struct pbl_bt_ancs_notif *notif) {
 *   if (notif->event == PBL_BT_ANCS_EVENT_ADDED) {
 *     pbl_bt_ancs_client_get_notif_attrs(client, notif->uid, s_attrs, PBL_ARRAY_SIZE(s_attrs));
 *   }
 * }
 *
 * static const struct pbl_bt_ancs_client_cb s_cb = {
 *   .discovered = prv_discovered,
 *   .notification = prv_notification,
 *   .notif_attrs = prv_notif_attrs,
 * };
 *
 * pbl_bt_ancs_client_start(&s_client, conn_handle, &s_cb);
 * @endcode
 *
 * A client runs one request at a time: getting attributes or performing an action. The
 * outcome of each is reported once, unless the request is cancelled or the client stopped.
 *
 * A status is 0, the ATT error code the service answered with (positive, e.g.
 * @c PBL_BT_ANCS_ERR_INVALID_PARAMETER), or a negative errno. Callbacks are called on the host
 * task, and none after pbl_bt_ancs_client_stop() returns but one that was already running.
 * @{
 */

/** @brief At most this many attributes in a request. */
#define PBL_BT_ANCS_CLIENT_ATTRS_MAX 8

/** @brief What the service notifies from. */
enum pbl_bt_ancs_source {
  /** Notification Source: notifications added, modified and removed. */
  PBL_BT_ANCS_SOURCE_NOTIFICATION,
  /** Data Source: the attributes asked for. */
  PBL_BT_ANCS_SOURCE_DATA,
};

/** @brief An attribute to ask for. */
struct pbl_bt_ancs_attr_req {
  /** Which one: an enum pbl_bt_ancs_notif_attr or an enum pbl_bt_ancs_app_attr. */
  uint8_t id;
  /**
   * Longest value accepted, 0 for any. The service is told to cut the title, the subtitle
   * and the message of a notification to it, which need one.
   */
  uint16_t max_len;
  /**
   * Whether the response may lack it, as that of a device that does not know the attribute.
   * A response ends once it has the attributes that are not optional.
   */
  bool optional;
};

struct pbl_bt_ancs_client;

/** @brief What a client reports; any may be null. */
struct pbl_bt_ancs_client_cb {
  /**
   * @brief The search for the service ended.
   *
   * @param status 0 if the device has it, -ENOENT if not, or what failed.
   */
  void (*discovered)(struct pbl_bt_ancs_client *client, int status);

  /**
   * @brief A pbl_bt_ancs_client_subscribe() was answered.
   *
   * @param source The source.
   * @param enable As asked for.
   * @param status 0, or why not.
   */
  void (*subscribed)(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source, bool enable,
                     int status);

  /**
   * @brief The device added, modified or removed a notification.
   *
   * @param notif The notification, valid during the call.
   */
  void (*notification)(struct pbl_bt_ancs_client *client, const struct pbl_bt_ancs_notif *notif);

  /**
   * @brief A pbl_bt_ancs_client_get_notif_attrs() ended.
   *
   * @param status 0, -EBADMSG if the response was not to the request or had a value longer
   * than accepted, -ENOBUFS if it did not fit the buffer of the client, or why not otherwise.
   * @param uid The notification.
   * @param attrs If @p status is 0, the attributes in the order they were asked for, null
   * for those the response lacked; valid during the call.
   * @param num_attrs Number of attributes asked for.
   */
  void (*notif_attrs)(struct pbl_bt_ancs_client *client, int status, uint32_t uid,
                      const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs);

  /**
   * @brief A pbl_bt_ancs_client_get_app_attrs() ended.
   *
   * @param status As for @c notif_attrs.
   * @param attrs As for @c notif_attrs.
   * @param num_attrs Number of attributes asked for.
   */
  void (*app_attrs)(struct pbl_bt_ancs_client *client, int status,
                    const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs);

  /**
   * @brief A pbl_bt_ancs_client_perform_action() ended.
   *
   * @param status 0 if the device performed it, or why not.
   * @param uid The notification.
   */
  void (*action_performed)(struct pbl_bt_ancs_client *client, int status, uint32_t uid);
};

/** @brief A client. Define it with PBL_BT_ANCS_CLIENT_DEFINE(). */
struct pbl_bt_ancs_client {
  /** @cond INTERNAL_HIDDEN */
  uint8_t *buf;
  uint16_t buf_size;
  struct pbl_bt_gattc_service svc;
  struct pbl_bt_gattc_chr chrs[3];
  const struct pbl_bt_ancs_client_cb *cb;
  bool started;
  uint8_t request;
  bool acked;
  bool complete;
  uint8_t num_attrs;
  uint16_t len;
  uint32_t uid;
  struct pbl_bt_ancs_attr_req attrs[PBL_BT_ANCS_CLIENT_ATTRS_MAX];
  /** @endcond */
};

/**
 * @brief Define a client.
 *
 * @param _name Name of the struct pbl_bt_ancs_client.
 * @param _buf_size Size of the buffer responses are gathered in: the longest one expected.
 * The response to pbl_bt_ancs_client_get_notif_attrs() takes 5 bytes, and that to
 * pbl_bt_ancs_client_get_app_attrs() the app identifier and 2 bytes; each attribute then takes
 * 3 bytes and its value.
 */
#define PBL_BT_ANCS_CLIENT_DEFINE(_name, _buf_size)                                               \
  static uint8_t _name##_buf[_buf_size];                                                          \
  static struct pbl_bt_ancs_client _name = {                                                      \
    .buf = _name##_buf,                                                                           \
    .buf_size = sizeof(_name##_buf),                                                              \
  };                                                                                              \
  static bool _name##_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) { \
    return pbl_bt_ancs_client_notify_rx(&_name, conn_handle, attr_handle, om);                    \
  }                                                                                               \
  PBL_BT_GATTC_NOTIFY_LISTENER_DEFINE(_name, _name##_notify_rx)

/**
 * @brief Look for the service on a connection.
 *
 * @param client Client.
 * @param conn_handle Connection.
 * @param cb Callbacks, which must stay valid.
 * @retval 0 Looking for it, @c discovered follows.
 * @retval -EALREADY Started already.
 * @retval -ENOMEM Out of memory.
 */
int pbl_bt_ancs_client_start(struct pbl_bt_ancs_client *client, uint16_t conn_handle,
                             const struct pbl_bt_ancs_client_cb *cb);

/**
 * @brief Look for the service again, as when the device says its services changed.
 *
 * Nothing changes if the service is still where it was. Otherwise the request in progress is
 * forgotten and @c discovered is called: with 0 if the service is there now, or elsewhere, and
 * its sources are to be subscribed to again, or with why not if it is gone.
 *
 * @param client Client.
 * @retval 0 Looking for it.
 * @retval -ENOTCONN Not started.
 * @retval -ENOMEM Out of memory.
 */
int pbl_bt_ancs_client_rediscover(struct pbl_bt_ancs_client *client);

/** @brief Forget the service, the connection and the request in progress. */
void pbl_bt_ancs_client_stop(struct pbl_bt_ancs_client *client);

/**
 * @brief Subscribe to a source, or unsubscribe.
 *
 * Subscribe to the Data Source first, so that no response is missed.
 *
 * @param client Client.
 * @param source The source.
 * @param enable true to subscribe.
 * @retval 0 Asked, @c subscribed follows.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ancs_client_subscribe(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source,
                                 bool enable);

/**
 * @brief Get attributes of a notification.
 *
 * With no attributes, the request ends when the service answers it: with
 * @c PBL_BT_ANCS_ERR_INVALID_PARAMETER if the notification does not exist.
 *
 * @param client Client.
 * @param uid The notification.
 * @param attrs The attributes, each an enum pbl_bt_ancs_notif_attr.
 * @param num_attrs Number of attributes, at most @c PBL_BT_ANCS_CLIENT_ATTRS_MAX.
 * @retval 0 Asked, @c notif_attrs follows.
 * @retval -EINVAL Too many attributes.
 * @retval -EBUSY A request is in progress.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ancs_client_get_notif_attrs(struct pbl_bt_ancs_client *client, uint32_t uid,
                                       const struct pbl_bt_ancs_attr_req *attrs, uint8_t num_attrs);

/**
 * @brief Get attributes of an app.
 *
 * @param client Client.
 * @param app_id Identifier of the app, as the app identifier of a notification gives it.
 * @param app_id_len Its length.
 * @param attrs The attributes, each an enum pbl_bt_ancs_app_attr.
 * @param num_attrs Number of attributes, at least one and at most
 * @c PBL_BT_ANCS_CLIENT_ATTRS_MAX.
 * @retval 0 Asked, @c app_attrs follows.
 * @retval -EINVAL No attribute, or too many.
 * @retval -EBUSY A request is in progress.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ancs_client_get_app_attrs(struct pbl_bt_ancs_client *client, const void *app_id,
                                     size_t app_id_len, const struct pbl_bt_ancs_attr_req *attrs,
                                     uint8_t num_attrs);

/**
 * @brief Perform an action on a notification.
 *
 * @param client Client.
 * @param uid The notification.
 * @param action The action.
 * @retval 0 Asked, @c action_performed follows.
 * @retval -EBUSY A request is in progress.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ancs_client_perform_action(struct pbl_bt_ancs_client *client, uint32_t uid,
                                      enum pbl_bt_ancs_action action);

/**
 * @brief Give up on the request in progress, if any: its outcome is not reported.
 *
 * @param client Client.
 */
void pbl_bt_ancs_client_cancel(struct pbl_bt_ancs_client *client);

/**
 * @brief Take a notification of a peer if it is of the service of the client.
 *
 * PBL_BT_ANCS_CLIENT_DEFINE() has it called.
 *
 * @return true if it was.
 */
bool pbl_bt_ancs_client_notify_rx(struct pbl_bt_ancs_client *client, uint16_t conn_handle,
                                  uint16_t attr_handle, struct os_mbuf *om);

/** @} */
