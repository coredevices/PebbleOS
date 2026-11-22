/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>

#include <pbl/drivers/disk.h>
#include <pbl/drivers/sdhc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/sd/sd.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DEFINE(sd, CONFIG_SD_LOG_LEVEL);

#define BLOCK_SIZE 512U

#define IDENT_HZ   400000U
#define DEFAULT_HZ 25000000U
#define HIGH_HZ    50000000U

#define CMD_GO_IDLE_STATE        0U
#define CMD_ALL_SEND_CID         2U
#define CMD_SEND_RELATIVE_ADDR   3U
#define CMD_SWITCH_FUNC          6U
#define CMD_SELECT_CARD          7U
#define CMD_SEND_IF_COND         8U
#define CMD_SEND_CSD             9U
#define CMD_STOP_TRANSMISSION    12U
#define CMD_SEND_STATUS          13U
#define CMD_SET_BLOCKLEN         16U
#define CMD_READ_SINGLE_BLOCK    17U
#define CMD_READ_MULTIPLE_BLOCK  18U
#define CMD_WRITE_BLOCK          24U
#define CMD_WRITE_MULTIPLE_BLOCK 25U
#define CMD_APP_CMD              55U
#define ACMD_SET_BUS_WIDTH       6U
#define ACMD_SD_SEND_OP_COND     41U

// 2.7 to 3.6 V, with the check pattern
#define IF_COND_ARG     0x1aaU
#define IF_COND_MASK    0xfffU
#define OCR_VDD         0x00ff8000U
#define OCR_HCS         PBL_BIT(30)
#define OCR_BUSY        PBL_BIT(31)
#define BUS_WIDTH_4_ARG 2U

// The CSD's command classes, class 10 being the switch function
#define CSD_CCC_SWITCH PBL_BIT(30)

// Access mode, function group 1, checked or switched to high speed, the other groups unchanged
#define SWITCH_CHECK_HIGH_SPEED 0x00fffff1U
#define SWITCH_SET_HIGH_SPEED   0x80fffff1U
#define SWITCH_STATUS_SIZE      64U
// The switch status: the functions group 1 supports, and the one it switched to
#define SWITCH_GROUP1_SUPPORT_BYTE 13U
#define SWITCH_GROUP1_RESULT_BYTE  16U
#define SWITCH_FUNC_HIGH_SPEED     1U

#define STATUS_READY_FOR_DATA PBL_BIT(8)
#define STATUS_STATE_MASK     PBL_GENMASK(12, 9)
#define STATUS_STATE_TRAN     4U
#define STATUS_ERRORS         0xfdf90008U

// How long a card takes to leave its power-up, and to program what it was sent
#define OP_COND_TIMEOUT_MS 1000U
#define PROGRAM_TIMEOUT_MS 1000U

struct prv_card {
  struct pbl_mutex lock;
  bool open;
  bool high_capacity;
  bool switch_func;
  bool high_speed;
  uint16_t rca;
  uint32_t sectors;
};

static struct prv_card s_card = {.lock = PBL_MUTEX_INITIALIZER};

static int prv_cmd(uint8_t opcode, uint32_t arg, uint32_t flags, uint32_t *resp) {
  struct pbl_sdhc_cmd cmd = {.opcode = opcode, .arg = arg, .flags = flags};
  int ret = pbl_sdhc_request(SDHC, &cmd, nullptr);

  if (ret == 0 && resp != nullptr) {
    for (unsigned int i = 0U; i < 4U; i++) {
      resp[i] = cmd.resp[i];
    }
  }

  return ret;
}

static int prv_acmd(uint8_t opcode, uint32_t arg, uint32_t flags, uint32_t *resp) {
  int ret = prv_cmd(CMD_APP_CMD, (uint32_t)s_card.rca << 16, PBL_SDHC_RSP_R1, nullptr);

  return ret != 0 ? ret : prv_cmd(opcode, arg, flags, resp);
}

// From the CSD, version 2.0 for the high capacity cards and 1.0 for the others; the response
// starts with its bits 127:96
static uint32_t prv_csd_sectors(const uint32_t *csd) {
  if ((csd[0] >> 30) == 1U) {
    uint32_t c_size = ((csd[1] & 0x3fU) << 16) | (csd[2] >> 16);

    return (c_size + 1U) * 1024U;
  }

  uint32_t read_bl_len = (csd[1] >> 16) & 0xfU;
  uint32_t c_size = ((csd[1] & 0x3ffU) << 2) | (csd[2] >> 30);
  uint32_t c_size_mult = (csd[2] >> 15) & 0x7U;

  return (uint32_t)(((uint64_t)(c_size + 1U) << (c_size_mult + 2U + read_bl_len)) / BLOCK_SIZE);
}

// The SD physical layer's initialization: idle, the interface condition, the operating condition
// until the card leaves its power-up, then its identification, address and size
static int prv_identify(void) {
  uint32_t resp[4];
  uint64_t deadline;
  bool v2;
  int ret;

  ret = pbl_sdhc_set_ios(SDHC, &(struct pbl_sdhc_ios){.clock_hz = IDENT_HZ, .bus_width = 1U});
  if (ret != 0) {
    return ret;
  }

  s_card.rca = 0U;
  ret = prv_cmd(CMD_GO_IDLE_STATE, 0U, PBL_SDHC_RSP_NONE, nullptr);
  if (ret != 0) {
    return ret;
  }

  // Version 1 cards do not answer
  v2 = prv_cmd(CMD_SEND_IF_COND, IF_COND_ARG, PBL_SDHC_RSP_R1, resp) == 0;
  if (v2 && (resp[0] & IF_COND_MASK) != IF_COND_ARG) {
    return -EIO;
  }

  deadline = pbl_uptime_ms() + OP_COND_TIMEOUT_MS;
  do {
    ret = prv_acmd(ACMD_SD_SEND_OP_COND, OCR_VDD | (v2 ? OCR_HCS : 0U), PBL_SDHC_RSP_R3, resp);
    if (ret != 0) {
      return ret;
    }
    if ((resp[0] & OCR_BUSY) != 0U) {
      break;
    }
    pbl_sleep_ms(10U);
  } while (pbl_uptime_ms() < deadline);

  if ((resp[0] & OCR_BUSY) == 0U) {
    return -ETIMEDOUT;
  }
  s_card.high_capacity = (resp[0] & OCR_HCS) != 0U;

  ret = prv_cmd(CMD_ALL_SEND_CID, 0U, PBL_SDHC_RSP_R2, nullptr);
  if (ret != 0) {
    return ret;
  }

  ret = prv_cmd(CMD_SEND_RELATIVE_ADDR, 0U, PBL_SDHC_RSP_R1, resp);
  if (ret != 0) {
    return ret;
  }
  s_card.rca = (uint16_t)(resp[0] >> 16);

  ret = prv_cmd(CMD_SEND_CSD, (uint32_t)s_card.rca << 16, PBL_SDHC_RSP_R2, resp);
  if (ret != 0) {
    return ret;
  }
  s_card.sectors = prv_csd_sectors(resp);
  s_card.switch_func = (resp[1] & CSD_CCC_SWITCH) != 0U;

  return prv_cmd(CMD_SELECT_CARD, (uint32_t)s_card.rca << 16, PBL_SDHC_RSP_R1B, nullptr);
}

static int prv_switch(uint32_t arg, uint8_t *status) {
  struct pbl_sdhc_cmd cmd = {.opcode = CMD_SWITCH_FUNC, .arg = arg, .flags = PBL_SDHC_RSP_R1};
  struct pbl_sdhc_data data = {.buf = status, .block_size = SWITCH_STATUS_SIZE, .blocks = 1U};

  return pbl_sdhc_request(SDHC, &cmd, &data);
}

// The card checks it supports high speed, then switches to it
static int prv_switch_high_speed(bool *high_speed) {
  static uint8_t s_status[PBL_DCACHE_ROUND_UP(SWITCH_STATUS_SIZE)] PBL_ALIGNED(
      PBL_DCACHE_LINE_SIZE);
  int ret;

  *high_speed = false;

  ret = prv_switch(SWITCH_CHECK_HIGH_SPEED, s_status);
  if (ret != 0) {
    return ret;
  }
  if ((s_status[SWITCH_GROUP1_SUPPORT_BYTE] & PBL_BIT(SWITCH_FUNC_HIGH_SPEED)) == 0U) {
    return 0;
  }

  ret = prv_switch(SWITCH_SET_HIGH_SPEED, s_status);
  if (ret != 0) {
    return ret;
  }
  *high_speed = (s_status[SWITCH_GROUP1_RESULT_BYTE] & 0xfU) == SWITCH_FUNC_HIGH_SPEED;

  return 0;
}

// The widest bus, and the fastest clock the slot and the card take
static int prv_configure(void) {
  uint8_t width = 1U;
  int ret;

  if (SDHC->bus_width >= 4U) {
    ret = prv_acmd(ACMD_SET_BUS_WIDTH, BUS_WIDTH_4_ARG, PBL_SDHC_RSP_R1, nullptr);
    if (ret != 0) {
      return ret;
    }
    width = 4U;
  }

  if (!s_card.high_capacity) {
    ret = prv_cmd(CMD_SET_BLOCKLEN, BLOCK_SIZE, PBL_SDHC_RSP_R1, nullptr);
    if (ret != 0) {
      return ret;
    }
  }

  s_card.high_speed = false;
  if (SDHC->max_clock_hz > DEFAULT_HZ && s_card.switch_func) {
    ret = prv_switch_high_speed(&s_card.high_speed);
    if (ret != 0) {
      return ret;
    }
  }

  return pbl_sdhc_set_ios(
      SDHC, &(struct pbl_sdhc_ios){
              .clock_hz = PBL_MIN(s_card.high_speed ? HIGH_HZ : DEFAULT_HZ, SDHC->max_clock_hz),
              .bus_width = width,
            });
}

static int prv_open(const struct pbl_disk *disk) {
  int cd = pbl_sdhc_get_cd(SDHC);
  int ret = 0;

  if (cd < 0) {
    return cd;
  }

  (void)pbl_mutex_lock(&s_card.lock, PBL_FOREVER);
  if (cd == 0) {
    s_card.open = false;
    ret = -ENOMEDIUM;
  } else if (!s_card.open) {
    ret = prv_identify();
    if (ret == 0) {
      ret = prv_configure();
    }
    s_card.open = ret == 0;
    if (ret == 0) {
      PBL_LOG_DBG("%s card, %" PRIu32 " sectors, %s speed", s_card.high_capacity ? "SDHC" : "SDSC",
                  s_card.sectors, s_card.high_speed ? "high" : "default");
    }
  }
  pbl_mutex_unlock(&s_card.lock);

  return ret;
}

static uint32_t prv_addr(uint32_t sector) {
  return s_card.high_capacity ? sector : sector * BLOCK_SIZE;
}

// Until the card has programmed what it was sent and is back in the transfer state
static int prv_wait_ready(void) {
  uint64_t deadline = pbl_uptime_ms() + PROGRAM_TIMEOUT_MS;
  uint32_t resp[4];
  int ret;

  do {
    ret = prv_cmd(CMD_SEND_STATUS, (uint32_t)s_card.rca << 16, PBL_SDHC_RSP_R1, resp);
    if (ret != 0) {
      return ret;
    }
    if ((resp[0] & STATUS_ERRORS) != 0U) {
      return -EIO;
    }
    if ((resp[0] & STATUS_READY_FOR_DATA) != 0U &&
        PBL_FIELD_GET(STATUS_STATE_MASK, resp[0]) == STATUS_STATE_TRAN) {
      return 0;
    }
    pbl_sleep_ms(1U);
  } while (pbl_uptime_ms() < deadline);

  return -ETIMEDOUT;
}

static int prv_transfer_run(uint32_t sector, uint32_t count, void *buf, bool write) {
  struct pbl_sdhc_data data =
      {.buf = buf, .block_size = BLOCK_SIZE, .blocks = count, .write = write};
  struct pbl_sdhc_cmd cmd = {
    .opcode = write ? (count > 1U ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_BLOCK)
                    : (count > 1U ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK),
    .arg = prv_addr(sector),
    .flags = PBL_SDHC_RSP_R1,
  };
  int ret;

  ret = pbl_sdhc_request(SDHC, &cmd, &data);
  if (count > 1U) {
    int stop = prv_cmd(CMD_STOP_TRANSMISSION, 0U, PBL_SDHC_RSP_R1B, nullptr);

    ret = ret != 0 ? ret : stop;
  }
  if (ret == 0 && write) {
    ret = prv_wait_ready();
  }

  return ret;
}

// In runs of as many blocks as the controller moves in one command, one block at a time from a
// buffer it does not take runs of
static int prv_transfer(uint32_t sector, uint32_t count, void *buf, bool write) {
  uint32_t run = SDHC->max_blocks != 0U ? SDHC->max_blocks : count;
  uint8_t *p = buf;
  int ret = 0;

  if (SDHC->multi_block_align > 1U && (uintptr_t)buf % SDHC->multi_block_align != 0U) {
    run = 1U;
  }

  if (!s_card.open) {
    return -ENOMEDIUM;
  }
  if (count == 0U || sector >= s_card.sectors || count > s_card.sectors - sector) {
    return -EINVAL;
  }

  (void)pbl_mutex_lock(&s_card.lock, PBL_FOREVER);
  while (ret == 0 && count > 0U) {
    uint32_t n = PBL_MIN(count, run);

    ret = prv_transfer_run(sector, n, p, write);
    sector += n;
    count -= n;
    p += n * BLOCK_SIZE;
  }
  pbl_mutex_unlock(&s_card.lock);

  return ret;
}

static int prv_read(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf) {
  return prv_transfer(sector, count, buf, false);
}

static int prv_write(const struct pbl_disk *disk, uint32_t sector, uint32_t count,
                     const void *buf) {
  return prv_transfer(sector, count, (void *)buf, true);
}

static int prv_get_capacity(const struct pbl_disk *disk, uint32_t *count) {
  if (!s_card.open) {
    return -ENOMEDIUM;
  }

  *count = s_card.sectors;

  return 0;
}

static const struct pbl_disk_ops s_ops = {
  .open = prv_open,
  .read = prv_read,
  .write = prv_write,
  .get_capacity = prv_get_capacity,
};

PBL_DEVICE_STATE_DEFINE(s_sd_disk);
static const struct pbl_disk s_sd_disk =
    PBL_DISK_INIT(s_sd_disk, "sd", nullptr, nullptr, &s_ops, BLOCK_SIZE);
PBL_DEVICE_REGISTER(s_sd_disk, &s_sd_disk.dev);

const struct pbl_disk *const SD_DISK = &s_sd_disk;
