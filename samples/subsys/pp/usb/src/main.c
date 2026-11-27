/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/console/console.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/main.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/pp/transports/usb.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#define MAIN_STACK_SIZE 4096
#define MAIN_CALLS      32

#define ENDPOINT_BENCH 0x0b0b

#define LOG_PERIOD_MS 1000U

enum : uint8_t {
  CMD_PING = 0,
  CMD_PONG = 1,
};

enum : uint8_t {
  VERSION_REQUEST = 0,
  VERSION_RESPONSE = 1,
};

enum : uint8_t {
  BENCH_SINK = 0,
  BENCH_SOURCE = 1,
  BENCH_SYNC = 2,
};

struct PBL_PACKED ping_msg {
  uint8_t cmd;
  uint32_t cookie;
};

struct PBL_PACKED fw_version {
  uint32_t timestamp;
  char version_tag[32];
  char git_hash[8];
  uint8_t is_recovery;
  uint8_t hw_platform;
  uint8_t metadata_version;
};

struct PBL_PACKED version_response {
  uint8_t cmd;
  struct fw_version running;
  struct fw_version recovery;
  uint32_t bootloader_timestamp;
  char board[9];
  char serial[12];
  uint8_t bt_address[6];
  uint32_t resource_crc;
  uint32_t resource_timestamp;
  char language[6];
  uint16_t language_version;
  uint64_t capabilities;
};

struct PBL_PACKED app_log_msg {
  uint8_t uuid[16];
  uint32_t timestamp;
  uint8_t level;
  uint8_t message_length;
  uint16_t line_number;
  char filename[16];
  char message[32];
};

struct PBL_PACKED bench_source {
  uint8_t cmd;
  uint16_t count;
  uint16_t size;
};

/* The main thread of the protocol: a work queue of the sample. */
static PBL_WORKQ_DEFINE(s_main_q);
static PBL_WORK_POOL_DEFINE(s_main_pool, MAIN_CALLS);
static struct pbl_thread s_main_thread;
PBL_THREAD_STACK_DEFINE(s_main_stack, MAIN_STACK_SIZE);

static void prv_log_cb(struct pbl_work *work);

static PBL_DELAYED_WORK_DEFINE(s_log_work, prv_log_cb);
static struct pbl_pp_session *_Atomic s_log_session;
static unsigned int s_log_count;

static uint8_t s_bench_buf[PBL_PP_MAX_OUTBOUND_PAYLOAD];

void pbl_pp_main_call(void (*fn)(void *data), void *data) {
  /* A call from the main thread itself does not wait for a slot, and none may be lost. */
  const int rc = pbl_work_call(&s_main_q, &s_main_pool, fn, data, PBL_FOREVER);

  PBL_ASSERTN(rc == 0);
}

bool pbl_pp_main_is_current(void) {
  return pbl_workq_is_current(&s_main_q);
}

static const char *prv_transport(const struct pbl_pp_session *session) {
  return pbl_pp_session_get_transport(session)->name;
}

static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  switch (event) {
    case PBL_PP_SESSION_OPENED:
      printf("pp_usb: %s session opened\n", prv_transport(session));
      break;
    case PBL_PP_SESSION_CLOSED:
      printf("pp_usb: %s session closed\n", prv_transport(session));
      break;
    default:
      break;
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(sample, prv_session_cb, nullptr);

/* Runs on the system work queue, where sending may wait for room. */
static void prv_ping_handler(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  const struct ping_msg *ping = (const struct ping_msg *)data;
  struct ping_msg pong;

  if (length < sizeof(*ping) || ping->cmd != CMD_PING) {
    return;
  }

  pong = (struct ping_msg){.cmd = CMD_PONG, .cookie = ping->cookie};
  (void)pbl_pp_send(session, PBL_PP_ENDPOINT_PING, (const uint8_t *)&pong, sizeof(pong),
                    PBL_PP_DEFAULT_TIMEOUT_MS);
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_PING, PBL_PP_ACCESS_ANY, prv_ping_handler);

static void prv_version_handler(struct pbl_pp_session *session, const uint8_t *data,
                                size_t length) {
  struct version_response rsp = {
    .cmd = VERSION_RESPONSE,
    .running =
        {
          .version_tag = "v0.0.0-pp-usb",
          .git_hash = "sample",
        },
    .board = "sample",
    .language = "en_US",
  };

  if (length < 1U || data[0] != VERSION_REQUEST) {
    return;
  }

  memcpy(rsp.serial, "SAMPLE000001", sizeof(rsp.serial));
  rsp.running.timestamp = pbl_cpu_to_be32((uint32_t)pbl_realtime());
  (void)pbl_pp_send(session, PBL_PP_ENDPOINT_SYSTEM_VERSION, (const uint8_t *)&rsp, sizeof(rsp),
                    PBL_PP_DEFAULT_TIMEOUT_MS);
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_SYSTEM_VERSION, PBL_PP_ACCESS_ANY, prv_version_handler);

static void prv_log_cb(struct pbl_work *work) {
  struct pbl_pp_session *session = atomic_load(&s_log_session);
  struct app_log_msg msg = {
    .level = 100U,
    .filename = "main.c",
  };
  int len;

  if (session == nullptr) {
    return;
  }

  len = snprintf(msg.message, sizeof(msg.message), "tick %u, uptime %u ms", s_log_count++,
                 (unsigned int)pbl_uptime_ms());
  msg.message_length = (uint8_t)PBL_MIN((size_t)len, sizeof(msg.message));
  msg.timestamp = pbl_cpu_to_be32((uint32_t)pbl_realtime());
  msg.line_number = pbl_cpu_to_be16((uint16_t)__LINE__);

  if (pbl_pp_send(session, PBL_PP_ENDPOINT_APP_LOG, (const uint8_t *)&msg,
                  offsetof(struct app_log_msg, message) + msg.message_length,
                  PBL_PP_DEFAULT_TIMEOUT_MS) != 0) {
    atomic_store(&s_log_session, nullptr);
    return;
  }

  (void)pbl_delayed_work_schedule(&pbl_sys_workq, &s_log_work, PBL_MSEC(LOG_PERIOD_MS));
}

/* The host turns the shipping of app logs on or off. */
static void prv_app_log_handler(struct pbl_pp_session *session, const uint8_t *data,
                                size_t length) {
  if (length < 1U) {
    return;
  }

  printf("pp_usb: app logs %s over %s\n", data[0] != 0U ? "on" : "off", prv_transport(session));
  atomic_store(&s_log_session, data[0] != 0U ? session : nullptr);
  if (data[0] != 0U) {
    (void)pbl_delayed_work_schedule(&pbl_sys_workq, &s_log_work, PBL_MSEC(0));
  }
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_APP_LOG, PBL_PP_ACCESS_ANY, prv_app_log_handler);

/* Sinks what the host sends, sends it what it asks for, and answers when all before arrived. */
static void prv_bench_handler(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  const struct bench_source *src = (const struct bench_source *)data;
  uint16_t count;
  uint16_t size;

  if (length < 1U) {
    return;
  }

  switch (data[0]) {
    case BENCH_SYNC:
      (void)pbl_pp_send(session, ENDPOINT_BENCH, data, 1U, PBL_PP_DEFAULT_TIMEOUT_MS);
      break;
    case BENCH_SOURCE:
      if (length < sizeof(*src)) {
        return;
      }
      count = pbl_be16_to_cpu(src->count);
      size = (uint16_t)PBL_CLAMP(pbl_be16_to_cpu(src->size), 1U, sizeof(s_bench_buf));
      s_bench_buf[0] = BENCH_SOURCE;
      for (uint16_t i = 0U; i < count; i++) {
        if (pbl_pp_send(session, ENDPOINT_BENCH, s_bench_buf, size, PBL_PP_DEFAULT_TIMEOUT_MS) !=
            0) {
          printf("pp_usb: bench stopped after %u messages\n", i);
          return;
        }
      }
      break;
    default:
      break;
  }
}

PBL_PP_ENDPOINT_DEFINE(ENDPOINT_BENCH, PBL_PP_ACCESS_ANY, prv_bench_handler);

int main(void) {
  const struct pbl_thread_attr attr = {
    .name = "pp",
    .entry = pbl_workq_thread,
    .arg = &s_main_q,
    .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO,
    .privileged = true,
    .stack = s_main_stack,
    .stack_size = sizeof(s_main_stack),
  };
  int rc;

  pbl_thread_create(&s_main_thread, &attr);

  /* The console takes frames, for the same endpoints over the serial port. */
  pbl_console_enable_prompt();

  rc = pbl_usbd_enable();
  if (rc != 0) {
    printf("pp_usb: USB device failed (%d)\n", rc);
    return rc;
  }

  printf("pp_usb: USB device up\n");

  return 0;
}
