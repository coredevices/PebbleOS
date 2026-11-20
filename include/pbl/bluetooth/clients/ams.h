/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/ams.h>
#include <pbl/bluetooth/clients/gatt.h>

/**
 * @defgroup bluetooth_ams_client AMS client
 * @ingroup bluetooth
 * @brief Client of the Apple Media Service of a connected iOS device.
 *
 * A client serves one connection at a time: define one for each connection to serve at once.
 * It finds the service, sends commands to the media player, registers for attributes of the
 * player, the queue and the track and reports their values as the device updates them, and
 * on request reads the whole of a value that was cut. The service requires an encrypted link:
 * start the client once the link is.
 *
 * @code{.c}
 * PBL_BT_AMS_CLIENT_DEFINE(s_client);
 *
 * static const uint8_t s_track_attrs[] = {
 *   PBL_BT_AMS_TRACK_ATTR_ARTIST,
 *   PBL_BT_AMS_TRACK_ATTR_TITLE,
 * };
 *
 * static void prv_discovered(struct pbl_bt_ams_client *client, int status) {
 *   if (status == 0) {
 *     pbl_bt_ams_client_subscribe(client, PBL_BT_AMS_SOURCE_ENTITY_UPDATE, true);
 *   }
 * }
 *
 * static void prv_subscribed(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source,
 *                            bool enable, int status) {
 *   if (status == 0) {
 *     pbl_bt_ams_client_register(client, PBL_BT_AMS_ENTITY_TRACK, s_track_attrs,
 *                                PBL_ARRAY_SIZE(s_track_attrs));
 *   }
 * }
 *
 * static void prv_update(struct pbl_bt_ams_client *client,
 *                        const struct pbl_bt_ams_update *update) {
 *   if (update->entity == PBL_BT_AMS_ENTITY_TRACK &&
 *       update->attr == PBL_BT_AMS_TRACK_ATTR_TITLE) {
 *     pbl_bt_ams_client_send_command(client, PBL_BT_AMS_COMMAND_VOLUME_UP);
 *   }
 * }
 *
 * static const struct pbl_bt_ams_client_cb s_cb = {
 *   .discovered = prv_discovered,
 *   .subscribed = prv_subscribed,
 *   .update = prv_update,
 * };
 *
 * pbl_bt_ams_client_start(&s_client, conn_handle, &s_cb);
 * @endcode
 *
 * Commands and registrations are answered in the order they were made. A client reads one
 * value at a time.
 *
 * A status is 0, the ATT error code the service answered with (positive, e.g.
 * @c PBL_BT_AMS_ERR_INVALID_STATE), or a negative errno. Callbacks are called on the host
 * task, and none after pbl_bt_ams_client_stop() returns but one that was already running.
 * @{
 */

/** @brief At most this many attributes in a registration. */
#define PBL_BT_AMS_CLIENT_ATTRS_MAX 8

/** @brief What the service notifies from. */
enum pbl_bt_ams_source {
  /** Remote Command: the commands the media player takes. */
  PBL_BT_AMS_SOURCE_REMOTE_COMMAND,
  /** Entity Update: the values of the attributes registered for. */
  PBL_BT_AMS_SOURCE_ENTITY_UPDATE,
};

struct pbl_bt_ams_client;

/** @brief What a client reports; any may be null. */
struct pbl_bt_ams_client_cb {
  /**
   * @brief The search for the service ended.
   *
   * @param status 0 if the device has it, -ENOENT if not, or what failed.
   */
  void (*discovered)(struct pbl_bt_ams_client *client, int status);

  /**
   * @brief A pbl_bt_ams_client_subscribe() was answered.
   *
   * @param source The source.
   * @param enable As asked for.
   * @param status 0, or why not.
   */
  void (*subscribed)(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source, bool enable,
                     int status);

  /**
   * @brief The commands the media player takes changed.
   *
   * @param commands The commands it takes now, each an enum pbl_bt_ams_command; valid during
   * the call.
   * @param num_commands Their number.
   */
  void (*commands)(struct pbl_bt_ams_client *client, const uint8_t *commands, uint8_t num_commands);

  /**
   * @brief The device told the value of an attribute that was registered for.
   *
   * @param update The attribute and its value, valid during the call.
   */
  void (*update)(struct pbl_bt_ams_client *client, const struct pbl_bt_ams_update *update);

  /**
   * @brief A pbl_bt_ams_client_send_command() was answered.
   *
   * @param status 0 if the device took the command, or why not.
   */
  void (*command_sent)(struct pbl_bt_ams_client *client, int status);

  /**
   * @brief A pbl_bt_ams_client_register() was answered.
   *
   * @param status 0 if the device took the registration, or why not.
   */
  void (*registered)(struct pbl_bt_ams_client *client, int status);

  /**
   * @brief A pbl_bt_ams_client_read_attr() ended.
   *
   * @param status 0, @c PBL_BT_AMS_ERR_ABSENT_ATTRIBUTE if the attribute is empty, or why
   * not otherwise.
   * @param entity The entity.
   * @param attr Its attribute.
   * @param value If @p status is 0, the whole value: UTF-8, not terminated, valid during the
   * call.
   * @param len Its length.
   */
  void (*attr_read)(struct pbl_bt_ams_client *client, int status, enum pbl_bt_ams_entity entity,
                    uint8_t attr, const uint8_t *value, uint16_t len);
};

/** @brief A client. Define it with PBL_BT_AMS_CLIENT_DEFINE(). */
struct pbl_bt_ams_client {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_bt_gattc_service svc;
  struct pbl_bt_gattc_chr chrs[3];
  const struct pbl_bt_ams_client_cb *cb;
  bool started;
  bool reading;
  uint8_t entity;
  uint8_t attr;
  /** @endcond */
};

/**
 * @brief Define a client.
 *
 * @param _name Name of the struct pbl_bt_ams_client.
 */
#define PBL_BT_AMS_CLIENT_DEFINE(_name)                                                           \
  static struct pbl_bt_ams_client _name;                                                          \
  static bool _name##_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) { \
    return pbl_bt_ams_client_notify_rx(&_name, conn_handle, attr_handle, om);                     \
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
int pbl_bt_ams_client_start(struct pbl_bt_ams_client *client, uint16_t conn_handle,
                            const struct pbl_bt_ams_client_cb *cb);

/**
 * @brief Look for the service again, as when the device says its services changed.
 *
 * Nothing changes if the service is still where it was. Otherwise the read in progress is
 * forgotten and @c discovered is called: with 0 if the service is there now, or elsewhere, and
 * it is to be subscribed to and registered with again, or with why not if it is gone.
 *
 * @param client Client.
 * @retval 0 Looking for it.
 * @retval -ENOTCONN Not started.
 * @retval -ENOMEM Out of memory.
 */
int pbl_bt_ams_client_rediscover(struct pbl_bt_ams_client *client);

/** @brief Forget the service, the connection and the read in progress. */
void pbl_bt_ams_client_stop(struct pbl_bt_ams_client *client);

/**
 * @brief Subscribe to a source, or unsubscribe.
 *
 * @param client Client.
 * @param source The source.
 * @param enable true to subscribe.
 * @retval 0 Asked, @c subscribed follows.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ams_client_subscribe(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source,
                                bool enable);

/**
 * @brief Send a command to the media player.
 *
 * @param client Client.
 * @param command The command.
 * @retval 0 Sent, @c command_sent follows.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ams_client_send_command(struct pbl_bt_ams_client *client,
                                   enum pbl_bt_ams_command command);

/**
 * @brief Register for attributes of an entity: their values are told now, and as they change.
 *
 * Subscribe to the Entity Update first. A registration replaces the one made before for the
 * entity.
 *
 * @param client Client.
 * @param entity The entity.
 * @param attrs Its attributes.
 * @param num_attrs Number of attributes, at most @c PBL_BT_AMS_CLIENT_ATTRS_MAX.
 * @retval 0 Asked, @c registered follows, and then @c update for each.
 * @retval -EINVAL Too many attributes.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ams_client_register(struct pbl_bt_ams_client *client, enum pbl_bt_ams_entity entity,
                               const uint8_t *attrs, uint8_t num_attrs);

/**
 * @brief Read the whole value of an attribute, as when an update had it cut.
 *
 * @param client Client.
 * @param entity The entity.
 * @param attr Its attribute.
 * @retval 0 Asked, @c attr_read follows.
 * @retval -EBUSY A read is in progress.
 * @retval -ENOTCONN The service was not found (yet).
 * @retval -ENOMEM Out of memory.
 * @retval -EIO The stack refused.
 */
int pbl_bt_ams_client_read_attr(struct pbl_bt_ams_client *client, enum pbl_bt_ams_entity entity,
                                uint8_t attr);

/**
 * @brief Take a notification of a peer if it is of the service of the client.
 *
 * PBL_BT_AMS_CLIENT_DEFINE() has it called.
 *
 * @return true if it was.
 */
bool pbl_bt_ams_client_notify_rx(struct pbl_bt_ams_client *client, uint16_t conn_handle,
                                 uint16_t attr_handle, struct os_mbuf *om);

/** @} */
