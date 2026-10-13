/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/accel_manager.h"

#include <pbl/drivers/accel.h>
#include <pbl/drivers/rtc.h>
#include <pbl/shell/shell.h>
#include "pbl/kernel/compiler.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ACCEL_RECORDER_MAGIC         0x52434150U
#define ACCEL_RECORDER_VERSION       1U
#define ACCEL_RECORDER_CHUNK_MAGIC   0xA55AU
#define ACCEL_RECORDER_MAX_FILES     10U
#define ACCEL_RECORDER_LABEL_LEN     24U
#define ACCEL_RECORDER_BOARD_LEN     16U
#define ACCEL_RECORDER_CHUNK_SAMPLES 52U
#define ACCEL_RECORDER_RATE_HZ       52UL
#define ACCEL_RECORDER_MAX_KB        1024UL

struct PBL_PACKED accel_recorder_header {
  uint32_t magic;
  uint8_t version;
  uint8_t reserved[3];
  uint32_t data_len;
  uint32_t start_time;
  uint32_t interval_us;
  char board[ACCEL_RECORDER_BOARD_LEN];
  char label[ACCEL_RECORDER_LABEL_LEN];
};

struct PBL_PACKED accel_recorder_chunk {
  uint16_t magic;
  uint16_t num_samples;
  uint32_t timestamp_ms;
  uint32_t interval_us;
};

static struct {
  AccelManagerState *state;
  int fd;
  char name[16];
  uint32_t data_len;
  uint32_t capacity;
  uint32_t num_samples;
  uint32_t num_chunks;
  uint32_t write_errors;
  AccelRawData samples[ACCEL_RECORDER_CHUNK_SAMPLES];
  uint8_t chunk[sizeof(struct accel_recorder_chunk) +
                ACCEL_RECORDER_CHUNK_SAMPLES * sizeof(AccelRawData)];
} s_rec = {.fd = -1};

static void prv_file_name(char *buf, size_t len, unsigned int index) {
  snprintf(buf, len, "accrec%u", index);
}

static void prv_finish(void) {
  sys_accel_manager_data_unsubscribe(s_rec.state);
  s_rec.state = NULL;

  if (pfs_seek(s_rec.fd, offsetof(struct accel_recorder_header, data_len), FSeekSet) >= 0) {
    pfs_write(s_rec.fd, &s_rec.data_len, sizeof(s_rec.data_len));
  }

  pfs_close(s_rec.fd);
  s_rec.fd = -1;
}

static void prv_handle_data(void *data) {
  if (s_rec.state == NULL) {
    return;
  }

  uint64_t timestamp_ms;
  const uint32_t num_samples = sys_accel_manager_get_num_samples(s_rec.state, &timestamp_ms);
  const size_t samples_len = num_samples * sizeof(AccelRawData);
  const size_t chunk_len = sizeof(struct accel_recorder_chunk) + samples_len;

  if ((num_samples > 0U) && ((s_rec.data_len + chunk_len) <= s_rec.capacity)) {
    const struct accel_recorder_chunk chunk = {
      .magic = ACCEL_RECORDER_CHUNK_MAGIC,
      .num_samples = num_samples,
      .timestamp_ms = (uint32_t)timestamp_ms,
      .interval_us = accel_get_sampling_interval(),
    };

    memcpy(s_rec.chunk, &chunk, sizeof(chunk));
    memcpy(&s_rec.chunk[sizeof(chunk)], s_rec.samples, samples_len);

    if (pfs_write(s_rec.fd, s_rec.chunk, chunk_len) == (int)chunk_len) {
      s_rec.data_len += chunk_len;
      s_rec.num_samples += num_samples;
      s_rec.num_chunks++;
    } else {
      s_rec.write_errors++;
    }
  }

  sys_accel_manager_consume_samples(s_rec.state, num_samples);

  if ((s_rec.data_len + sizeof(s_rec.chunk)) > s_rec.capacity) {
    prv_finish();
  }
}

static int prv_cmd_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long rate_hz = ACCEL_RECORDER_RATE_HZ;
  unsigned long max_kb = ACCEL_RECORDER_MAX_KB;
  char name[sizeof(s_rec.name)];
  unsigned int index;
  int fd = -1;

  if (s_rec.state != NULL) {
    pbl_shell_error(sh, "already recording to %s", s_rec.name);
    return -EBUSY;
  }

  if ((argc > 2) &&
      ((pbl_shell_strtoul(argv[2], &rate_hz) != 0) || (rate_hz == 0UL) || (rate_hz > 1000UL))) {
    pbl_shell_error(sh, "invalid rate");
    return -EINVAL;
  }

  if ((argc > 3) && ((pbl_shell_strtoul(argv[3], &max_kb) != 0) || (max_kb == 0UL))) {
    pbl_shell_error(sh, "invalid size");
    return -EINVAL;
  }

  for (index = 0U; index < ACCEL_RECORDER_MAX_FILES; index++) {
    prv_file_name(name, sizeof(name), index);
    fd = pfs_open(name, OP_FLAG_READ, 0, 0);
    if (fd == E_DOES_NOT_EXIST) {
      break;
    }
    if (fd >= 0) {
      pfs_close(fd);
    }
  }

  if (index == ACCEL_RECORDER_MAX_FILES) {
    pbl_shell_error(sh, "no free slot, remove a recording first");
    return -ENOSPC;
  }

  const uint32_t capacity = max_kb * 1024UL;
  fd = pfs_open(name, OP_FLAG_READ | OP_FLAG_WRITE, FILE_TYPE_STATIC,
                sizeof(struct accel_recorder_header) + capacity);
  if (fd < 0) {
    pbl_shell_error(sh, "could not create %s (%d)", name, fd);
    return -ENOSPC;
  }

  AccelManagerState *state = sys_accel_manager_data_subscribe(ACCEL_SAMPLING_50HZ, prv_handle_data,
                                                              NULL, PebbleTask_KernelBackground);
  const uint32_t interval_us =
      accel_manager_set_native_sampling_interval(state, 1000000UL / rate_hz);
  sys_accel_manager_set_sample_buffer(
      state, s_rec.samples, MIN(ARRAY_LENGTH(s_rec.samples), accel_get_max_num_samples()));

  struct accel_recorder_header header = {
    .magic = ACCEL_RECORDER_MAGIC,
    .version = ACCEL_RECORDER_VERSION,
    .start_time = (uint32_t)rtc_get_time(),
    .interval_us = interval_us,
  };
  strncpy(header.board, ACCEL_RECORDER_BOARD, sizeof(header.board));
  strncpy(header.label, argv[1], sizeof(header.label));

  // Leave data_len erased so that stop can program it in place
  const size_t head_len = offsetof(struct accel_recorder_header, data_len);
  const size_t tail_off = head_len + sizeof(header.data_len);
  if ((pfs_write(fd, &header, head_len) != (int)head_len) ||
      (pfs_seek(fd, tail_off, FSeekSet) < 0) ||
      (pfs_write(fd, (uint8_t *)&header + tail_off, sizeof(header) - tail_off) !=
       (int)(sizeof(header) - tail_off))) {
    sys_accel_manager_data_unsubscribe(state);
    pfs_close_and_remove(fd);
    pbl_shell_error(sh, "could not write header");
    return -EIO;
  }

  s_rec.fd = fd;
  strncpy(s_rec.name, name, sizeof(s_rec.name));
  s_rec.data_len = 0U;
  s_rec.capacity = capacity;
  s_rec.num_samples = 0U;
  s_rec.num_chunks = 0U;
  s_rec.write_errors = 0U;
  s_rec.state = state;

  pbl_shell_print(sh, "recording to %s at %" PRIu32 " mHz, up to %lu KiB", name,
                  (uint32_t)(1000000000UL / interval_us), max_kb);

  return 0;
}

static int prv_cmd_stop(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (s_rec.state == NULL) {
    pbl_shell_error(sh, "not recording");
    return -EINVAL;
  }

  prv_finish();

  pbl_shell_print(sh, "stopped %s: %" PRIu32 " samples, %" PRIu32 " bytes", s_rec.name,
                  s_rec.num_samples, s_rec.data_len);

  return 0;
}

static int prv_cmd_status(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (s_rec.state == NULL) {
    pbl_shell_print(sh, "idle");
    return 0;
  }

  pbl_shell_print(sh, "recording to %s: %" PRIu32 " samples, %" PRIu32 "/%" PRIu32 " bytes",
                  s_rec.name, s_rec.num_samples, s_rec.data_len, s_rec.capacity);
  pbl_shell_print(sh, "chunks %" PRIu32 ", write errors %" PRIu32, s_rec.num_chunks,
                  s_rec.write_errors);

  return 0;
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  char name[sizeof(s_rec.name)];

  for (unsigned int index = 0U; index < ACCEL_RECORDER_MAX_FILES; index++) {
    struct accel_recorder_header header;

    prv_file_name(name, sizeof(name), index);
    if ((s_rec.state != NULL) && (strcmp(name, s_rec.name) == 0)) {
      pbl_shell_print(sh, "%s: recording", name);
      continue;
    }

    const int fd = pfs_open(name, OP_FLAG_READ, 0, 0);
    if (fd < 0) {
      continue;
    }

    if ((pfs_read(fd, &header, sizeof(header)) == (int)sizeof(header)) &&
        (header.magic == ACCEL_RECORDER_MAGIC)) {
      header.label[sizeof(header.label) - 1U] = '\0';
      pbl_shell_print(sh, "%s: label %s, bytes %" PRIu32 ", interval %" PRIu32 " us", name,
                      header.label, header.data_len, header.interval_us);
    } else {
      pbl_shell_print(sh, "%s: invalid", name);
    }

    pfs_close(fd);
  }

  return 0;
}

static int prv_cmd_remove(const struct pbl_shell *sh, size_t argc, char **argv) {
  char name[sizeof(s_rec.name)];
  const bool all = (strcmp(argv[1], "all") == 0);

  for (unsigned int index = 0U; index < ACCEL_RECORDER_MAX_FILES; index++) {
    prv_file_name(name, sizeof(name), index);
    if (!all && (strcmp(name, argv[1]) != 0)) {
      continue;
    }

    if ((s_rec.state != NULL) && (strcmp(name, s_rec.name) == 0)) {
      pbl_shell_error(sh, "%s is being recorded", name);
      continue;
    }

    if (pfs_remove(name) == S_SUCCESS) {
      pbl_shell_print(sh, "removed %s", name);
    }
  }

  return 0;
}

static const struct pbl_shell_cmd sub_accelrec[] = {
  PBL_SHELL_CMD_ARG(start, NULL, "Start recording <label> [rate_hz] [max_kib]", prv_cmd_start, 2,
                    2),
  PBL_SHELL_CMD(stop, NULL, "Stop recording", prv_cmd_stop),
  PBL_SHELL_CMD(status, NULL, "Show the recording state", prv_cmd_status),
  PBL_SHELL_CMD(list, NULL, "List the recordings", prv_cmd_list),
  PBL_SHELL_CMD_ARG(remove, NULL, "Remove a recording <name|all>", prv_cmd_remove, 2, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(accelrec, sub_accelrec, "Raw accelerometer recorder", NULL);
