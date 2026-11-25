/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "responsiveness.h"

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <host/ble_gap.h>
#include <host/ble_hs.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

/*
 * The specification recommends that a peripheral waits 5 s after connecting. iOS connects with a
 * 720 ms supervision timeout and never raises it on its own, so the link is fragile until the
 * first request is granted.
 */
#define INIT_PAUSE_S 1

/* A request not answered within this long is made again. */
#define REQUEST_TIMEOUT_S 5
#define MAX_ATTEMPTS      3

/* A request that collides with a procedure of the central is not an attempt. */
#define COLLISION_RETRY_S 2
#define MAX_COLLISIONS    10

/* How long a request taken back still holds. */
#define RELEASE_DELAY_S 2

#define NEVER UINT64_MAX

struct params {
  uint16_t itvl_min;
  uint16_t itvl_max;
  uint16_t latency;
  uint16_t supervision_timeout;
};

static const struct params s_params[PBL_BT_RESPONSE_TIME_NUM] = {
  [PBL_BT_RESPONSE_TIME_MAX] =
      {
        .itvl_min = BLE_GAP_CONN_ITVL_MS(45),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(60),
        .latency = 4,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(6000),
      },
  [PBL_BT_RESPONSE_TIME_MIDDLE] =
      {
        .itvl_min = BLE_GAP_CONN_ITVL_MS(45),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(60),
        .latency = 4,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(6000),
      },
  [PBL_BT_RESPONSE_TIME_MIN] = {
    .itvl_min = BLE_GAP_CONN_ITVL_MS(15),
    .itvl_max = BLE_GAP_CONN_ITVL_MS(15),
    .latency = 0,
    .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(6000),
  },
};

struct conn {
  uint16_t handle;
  uint64_t connected_at;
  enum pbl_bt_response_time_state state;
  /* When the parameters are next asked for, NEVER once granted or given up. */
  uint64_t negotiate_at;
  /* A request is with the central. */
  bool pending;
  uint8_t attempts;
  uint8_t collisions;
};

static void prv_work_fn(struct pbl_work *work);

static PBL_MUTEX_DEFINE(s_lock);
static PBL_DELAYED_WORK_DEFINE(s_work, prv_work_fn);
static struct ble_gap_event_listener s_listener;
static struct conn s_conns[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];
/* The requests that ask for something. */
static struct pbl_bt_responsiveness_req *s_reqs;

static uint64_t prv_secs_from(uint64_t ticks, uint32_t secs) {
  return ticks + (uint64_t)secs * PBL_TICK_HZ;
}

static struct conn *prv_find(uint16_t handle) {
  for (size_t i = 0; i < PBL_ARRAY_SIZE(s_conns); i++) {
    if (s_conns[i].handle == handle) {
      return &s_conns[i];
    }
  }

  return nullptr;
}

static bool prv_is_granted(const struct conn *conn) {
  const struct params *params = &s_params[conn->state];
  struct ble_gap_conn_desc desc;

  if (ble_gap_conn_find(conn->handle, &desc) != 0) {
    return true;
  }

  /* Anything faster than the fastest will do. */
  return (conn->state == PBL_BT_RESPONSE_TIME_MIN || desc.conn_itvl >= params->itvl_min) &&
         desc.conn_itvl <= params->itvl_max && desc.conn_latency == params->latency;
}

static void prv_handle_failure(struct conn *conn, int status, uint64_t now) {
  if ((status == BLE_HS_HCI_ERR(BLE_ERR_LMP_COLLISION) ||
       status == BLE_HS_HCI_ERR(BLE_ERR_DIFF_TRANS_COLL)) &&
      conn->collisions < MAX_COLLISIONS) {
    conn->collisions++;
    if (conn->attempts > 0) {
      conn->attempts--;
    }
    conn->negotiate_at = prv_secs_from(now, COLLISION_RETRY_S);
    return;
  }

  conn->negotiate_at = prv_secs_from(now, REQUEST_TIMEOUT_S);
}

static void prv_negotiate(struct conn *conn, uint64_t now) {
  const struct params *params = &s_params[conn->state];
  const struct ble_gap_upd_params upd_params = {
    .itvl_min = params->itvl_min,
    .itvl_max = params->itvl_max,
    .latency = params->latency,
    .supervision_timeout = params->supervision_timeout,
  };
  const uint64_t pause_end = prv_secs_from(conn->connected_at, INIT_PAUSE_S);
  int rc;

  if (now < pause_end) {
    conn->negotiate_at = pause_end;
    return;
  }

  if (conn->attempts >= MAX_ATTEMPTS) {
    PBL_LOG_ERR("Response time %d not granted, giving up", conn->state);
    conn->negotiate_at = NEVER;
    return;
  }

  PBL_LOG_DBG("Requesting interval %u to %u ms, latency %u",
              params->itvl_min * BLE_HCI_CONN_ITVL / 1000,
              params->itvl_max * BLE_HCI_CONN_ITVL / 1000, params->latency);

  conn->attempts++;
  rc = ble_gap_update_params(conn->handle, &upd_params);
  if (rc != 0) {
    PBL_LOG_ERR("Could not request connection parameters (0x%04x)", (uint16_t)rc);
    prv_handle_failure(conn, rc, now);
    return;
  }

  conn->pending = true;
  conn->negotiate_at = prv_secs_from(now, REQUEST_TIMEOUT_S);
}

static void prv_drop(struct pbl_bt_responsiveness_req **link) {
  struct pbl_bt_responsiveness_req *req = *link;

  *link = req->next;
  req->next = nullptr;
  req->active = false;
}

static void prv_drop_all(uint16_t handle) {
  for (struct pbl_bt_responsiveness_req **link = &s_reqs; *link != nullptr;) {
    if ((*link)->conn_handle == handle) {
      prv_drop(link);
    } else {
      link = &(*link)->next;
    }
  }
}

/* Brings a connection up to date, and returns when it next needs to be. */
static uint64_t prv_update_conn(struct conn *conn, uint64_t now) {
  enum pbl_bt_response_time_state state = PBL_BT_RESPONSE_TIME_MAX;
  uint64_t next = NEVER;

  for (struct pbl_bt_responsiveness_req **link = &s_reqs; *link != nullptr;) {
    struct pbl_bt_responsiveness_req *req = *link;

    if (req->conn_handle == conn->handle) {
      if (req->until <= now) {
        prv_drop(link);
        continue;
      }

      state = PBL_MAX(state, (enum pbl_bt_response_time_state)req->state);
      next = PBL_MIN(next, req->until);
    }

    link = &req->next;
  }

  if (state != conn->state) {
    PBL_LOG_DBG("Response time %d", state);
    conn->state = state;
    conn->attempts = 0;
    conn->collisions = 0;
    /* A request with the central is waited for first. */
    if (!conn->pending) {
      conn->negotiate_at = now;
    }
  }

  if (conn->negotiate_at <= now) {
    conn->pending = false;
    if (prv_is_granted(conn)) {
      conn->negotiate_at = NEVER;
    } else {
      prv_negotiate(conn, now);
    }
  }

  return PBL_MIN(next, conn->negotiate_at);
}

static void prv_update(void) {
  const uint64_t now = pbl_uptime_ticks();
  uint64_t next = NEVER;

  for (size_t i = 0; i < PBL_ARRAY_SIZE(s_conns); i++) {
    if (s_conns[i].handle != BLE_HS_CONN_HANDLE_NONE) {
      next = PBL_MIN(next, prv_update_conn(&s_conns[i], now));
    }
  }

  if (next == NEVER) {
    pbl_delayed_work_cancel(&s_work);
  } else {
    pbl_delayed_work_reschedule(&pbl_sys_workq, &s_work,
                                PBL_TICKS(PBL_MIN(next - now, PBL_TICK_FOREVER - 1U)));
  }
}

static void prv_work_fn(struct pbl_work *work) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

/* A listener called earlier may ask before the connection is heard of here. */
static struct conn *prv_find_or_add(uint16_t handle) {
  struct ble_gap_conn_desc desc;
  struct conn *conn;

  if (handle == BLE_HS_CONN_HANDLE_NONE) {
    return nullptr;
  }

  conn = prv_find(handle);
  if (conn != nullptr) {
    return conn;
  }

  conn = prv_find(BLE_HS_CONN_HANDLE_NONE);
  if (conn == nullptr || ble_gap_conn_find(handle, &desc) != 0) {
    return nullptr;
  }

  *conn = (struct conn){
    .handle = handle,
    .connected_at = pbl_uptime_ticks(),
    .negotiate_at = NEVER,
  };

  return conn;
}

static void prv_handle_update(struct conn *conn, int status) {
  const uint64_t now = pbl_uptime_ticks();
  const bool pending = conn->pending;

  conn->pending = false;
  if (status == 0) {
    conn->negotiate_at = now;
  } else if (pending) {
    prv_handle_failure(conn, status, now);
  }
}

static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  struct conn *conn;

  if (event->type != BLE_GAP_EVENT_CONNECT && event->type != BLE_GAP_EVENT_DISCONNECT &&
      event->type != BLE_GAP_EVENT_CONN_UPDATE) {
    return 0;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        (void)prv_find_or_add(event->connect.conn_handle);
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      conn = prv_find(event->disconnect.conn.conn_handle);
      if (conn != nullptr) {
        prv_drop_all(conn->handle);
        conn->handle = BLE_HS_CONN_HANDLE_NONE;
      }
      break;
    case BLE_GAP_EVENT_CONN_UPDATE:
      conn = prv_find(event->conn_update.conn_handle);
      if (conn != nullptr) {
        prv_handle_update(conn, event->conn_update.status);
      }
      break;
    default:
      break;
  }

  prv_update();

  pbl_mutex_unlock(&s_lock);

  return 0;
}

void pbl_bt__responsiveness_init(void) {
  for (size_t i = 0; i < PBL_ARRAY_SIZE(s_conns); i++) {
    s_conns[i].handle = BLE_HS_CONN_HANDLE_NONE;
  }

  while (s_reqs != nullptr) {
    prv_drop(&s_reqs);
  }

  (void)ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
}

void pbl_bt_responsiveness_request(uint16_t conn_handle, struct pbl_bt_responsiveness_req *req,
                                   enum pbl_bt_response_time_state state,
                                   uint16_t max_period_secs) {
  const uint64_t now = pbl_uptime_ticks();
  struct conn *conn;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  conn = prv_find_or_add(conn_handle);
  if (conn == nullptr) {
    pbl_mutex_unlock(&s_lock);
    return;
  }

  if (state == PBL_BT_RESPONSE_TIME_MAX) {
    if (req->active && req->conn_handle == conn_handle) {
      req->until = prv_secs_from(now, RELEASE_DELAY_S);
    }
  } else {
    if (!req->active) {
      req->active = true;
      req->next = s_reqs;
      s_reqs = req;
    }

    req->conn_handle = conn_handle;
    req->state = state;
    req->until = (max_period_secs == PBL_BT_RESPONSE_TIME_FOREVER)
                     ? NEVER
                     : prv_secs_from(now, max_period_secs);
  }

  prv_update();

  pbl_mutex_unlock(&s_lock);
}

enum pbl_bt_response_time_state pbl_bt_responsiveness_get(uint16_t conn_handle) {
  enum pbl_bt_response_time_state state = PBL_BT_RESPONSE_TIME_MAX;
  struct conn *conn;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  conn = prv_find(conn_handle);
  if (conn_handle != BLE_HS_CONN_HANDLE_NONE && conn != nullptr) {
    state = conn->state;
  }

  pbl_mutex_unlock(&s_lock);

  return state;
}
