/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/usb/dfu.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#ifdef CONFIG_USB_CDC_ACM
#define IFACE 2U
#else
#define IFACE 0U
#endif

#define DFU_CLASS           0xfeU
#define DFU_SUBCLASS        0x01U
#define DFU_PROTOCOL_DFU    0x02U
#define DFU_DESC_FUNCTIONAL 0x21U

#define DFU_ATTR_CAN_DNLOAD             0x01U
#define DFU_ATTR_CAN_UPLOAD             0x02U
#define DFU_ATTR_MANIFESTATION_TOLERANT 0x04U

#define DFU_DNLOAD    0x01U
#define DFU_UPLOAD    0x02U
#define DFU_GETSTATUS 0x03U
#define DFU_CLRSTATUS 0x04U
#define DFU_GETSTATE  0x05U
#define DFU_ABORT     0x06U

#define TRANSFER_SIZE CONFIG_USB_DFU_TRANSFER_SIZE

// Typical time to program a page, which the geometry does not give
#define PAGE_PROGRAM_MS 1U

enum prv_state {
  STATE_DFU_IDLE = 2,
  STATE_DNLOAD_SYNC = 3,
  STATE_DNBUSY = 4,
  STATE_DNLOAD_IDLE = 5,
  STATE_MANIFEST_SYNC = 6,
  STATE_UPLOAD_IDLE = 9,
  STATE_ERROR = 10,
};

enum prv_status {
  STATUS_OK = 0x00,
  STATUS_ERR_WRITE = 0x03,
  STATUS_ERR_ERASE = 0x04,
  STATUS_ERR_ADDRESS = 0x08,
  STATUS_ERR_NOTDONE = 0x09,
  STATUS_ERR_UNKNOWN = 0x0e,
  STATUS_ERR_STALLEDPKT = 0x0f,
};

static const uint8_t s_desc[] = {
  9,
  PBL_USB_DESC_INTERFACE,
  IFACE,
  0,
  0,
  DFU_CLASS,
  DFU_SUBCLASS,
  DFU_PROTOCOL_DFU,
  0,
  9,
  DFU_DESC_FUNCTIONAL,
  DFU_ATTR_CAN_DNLOAD | DFU_ATTR_CAN_UPLOAD | DFU_ATTR_MANIFESTATION_TOLERANT,
  // wDetachTimeOut, which DFU mode does not use
  0,
  0,
  TRANSFER_SIZE & 0xffU,
  TRANSFER_SIZE >> 8,
  0x10,
  0x01,
};

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_usb_dfu_region, usb_dfu_regions);

static void prv_work(struct pbl_work *work);

// The work item runs the flash accesses the interrupt asks for: a block to write, written first,
// then the block to read for the next upload. Upload blocks are read ahead into the buffer the
// host is not reading from.
static struct {
  struct pbl_work work;
  const struct pbl_usb_dfu_region *region;
  enum prv_state state;
  enum prv_status status;
  uint32_t poll_ms;
  uint32_t dn_off;
  uint32_t erased_end;
  bool wr_pending;
  bool wr_busy;
  uint32_t wr_off;
  uint16_t wr_len;
  enum prv_status wr_status;
  uint32_t up_off;
  uint8_t up_last;
  bool rd_pending;
  uint32_t rd_seq;
  uint32_t rd_off;
  uint8_t rd_idx;
  bool up_valid[2];
  uint32_t up_buf_off[2];
  uint8_t ctrl_buf[6];
  uint8_t dn_buf[TRANSFER_SIZE];
  uint8_t up_buf[2][TRANSFER_SIZE];
} s_dfu = {
  .work = PBL_WORK_INITIALIZER(prv_work),
  .state = STATE_DFU_IDLE,
};

// Erase the subsectors up to end that the download has not erased yet
static int prv_erase(uint32_t end) {
  struct pbl_flash_partition part = s_dfu.region->part;
  uint32_t ss = pbl_flash_get_geometry(FLASH)->subsector_size;
  uint32_t to = PBL_MIN(PBL_DIV_ROUND_UP(end, ss) * ss, part.size);
  int ret;

  if (end <= s_dfu.erased_end) {
    return 0;
  }

  ret = pbl_flash_partition_erase(part, s_dfu.erased_end, to - s_dfu.erased_end);
  if (ret != 0) {
    PBL_LOG_ERR("dfu: erase at %" PRIu32 " failed (%d)", s_dfu.erased_end, ret);
    return ret;
  }

  s_dfu.erased_end = to;

  return 0;
}

static enum prv_status prv_write(uint32_t off, uint16_t len) {
  int ret;

  ret = prv_erase(off + len);
  if (ret != 0) {
    return STATUS_ERR_ERASE;
  }

  ret = pbl_flash_partition_write(s_dfu.region->part, off, s_dfu.dn_buf, len);
  if (ret != 0) {
    PBL_LOG_ERR("dfu: write at %" PRIu32 " failed (%d)", off, ret);
    return STATUS_ERR_WRITE;
  }

  return STATUS_OK;
}

static void prv_work(struct pbl_work *work) {
  for (;;) {
    pbl_irq_lock();
    if (s_dfu.wr_pending) {
      uint32_t off = s_dfu.wr_off;
      uint16_t len = s_dfu.wr_len;
      enum prv_status status;

      s_dfu.wr_pending = false;
      pbl_irq_unlock();

      status = prv_write(off, len);

      pbl_irq_lock();
      s_dfu.wr_status = status;
      s_dfu.wr_busy = false;
      pbl_irq_unlock();
      continue;
    }

    if (s_dfu.rd_pending) {
      struct pbl_flash_partition part = s_dfu.region->part;
      uint32_t seq = s_dfu.rd_seq;
      uint32_t off = s_dfu.rd_off;
      uint8_t idx = s_dfu.rd_idx;
      int ret;

      s_dfu.rd_pending = false;
      pbl_irq_unlock();

      ret = pbl_flash_partition_read(part, off, s_dfu.up_buf[idx],
                                     PBL_MIN((uint32_t)TRANSFER_SIZE, part.size - off));

      pbl_irq_lock();
      if (ret == 0 && seq == s_dfu.rd_seq) {
        s_dfu.up_valid[idx] = true;
        s_dfu.up_buf_off[idx] = off;
      }
      pbl_irq_unlock();
      continue;
    }

    pbl_irq_unlock();
    return;
  }
}

// Read the upload block at off ahead, into the buffer the host is not reading from
static void prv_read_ahead(uint32_t off) {
  uint8_t idx = s_dfu.up_last ^ 1U;

  s_dfu.rd_seq++;
  s_dfu.up_valid[idx] = false;
  if (off >= s_dfu.region->part.size) {
    return;
  }

  s_dfu.rd_off = off;
  s_dfu.rd_idx = idx;
  s_dfu.rd_pending = true;
  (void)pbl_work_submit(&pbl_sys_workq, &s_dfu.work);
}

static void prv_idle(void) {
  s_dfu.state = STATE_DFU_IDLE;
  s_dfu.status = STATUS_OK;
  prv_read_ahead(0U);
}

static int prv_fail(enum prv_status status) {
  s_dfu.state = STATE_ERROR;
  s_dfu.status = status;

  return -EINVAL;
}

static int prv_dnload(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  if ((s_dfu.state != STATE_DFU_IDLE && s_dfu.state != STATE_DNLOAD_IDLE) || s_dfu.wr_busy) {
    return prv_fail(STATUS_ERR_STALLEDPKT);
  }

  if (setup->wLength == 0U) {
    if (s_dfu.state != STATE_DNLOAD_IDLE) {
      return prv_fail(STATUS_ERR_NOTDONE);
    }
    s_dfu.state = STATE_MANIFEST_SYNC;
    return 0;
  }

  if (setup->wLength > TRANSFER_SIZE) {
    return prv_fail(STATUS_ERR_STALLEDPKT);
  }

  if (s_dfu.state == STATE_DFU_IDLE) {
    s_dfu.dn_off = 0U;
    s_dfu.erased_end = 0U;
  }

  if (setup->wLength > s_dfu.region->part.size - s_dfu.dn_off) {
    return prv_fail(STATUS_ERR_ADDRESS);
  }

  *data = s_dfu.dn_buf;
  *len = TRANSFER_SIZE;

  return 0;
}

// The block arrived: the work item erases what it reaches and writes it while the host polls
static void prv_dnload_block(uint16_t len) {
  const struct pbl_flash_geometry *geo = pbl_flash_get_geometry(FLASH);
  uint32_t end = s_dfu.dn_off + len;
  uint32_t poll_ms = PAGE_PROGRAM_MS * PBL_DIV_ROUND_UP(len, geo->page_size);

  if (end > s_dfu.erased_end) {
    poll_ms +=
        geo->subsector_erase_ms * PBL_DIV_ROUND_UP(end - s_dfu.erased_end, geo->subsector_size);
  }

  s_dfu.rd_seq++;
  s_dfu.up_valid[0] = false;
  s_dfu.up_valid[1] = false;
  s_dfu.rd_pending = false;

  s_dfu.wr_off = s_dfu.dn_off;
  s_dfu.wr_len = len;
  s_dfu.wr_pending = true;
  s_dfu.wr_busy = true;
  s_dfu.dn_off = end;
  s_dfu.poll_ms = poll_ms;
  s_dfu.state = STATE_DNLOAD_SYNC;
  (void)pbl_work_submit(&pbl_sys_workq, &s_dfu.work);
}

static int prv_upload(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  uint32_t n;

  if ((s_dfu.state != STATE_DFU_IDLE && s_dfu.state != STATE_UPLOAD_IDLE) ||
      setup->wLength > TRANSFER_SIZE) {
    return prv_fail(STATUS_ERR_STALLEDPKT);
  }

  if (s_dfu.state == STATE_DFU_IDLE) {
    s_dfu.up_off = 0U;
  }

  n = PBL_MIN((uint32_t)setup->wLength, s_dfu.region->part.size - s_dfu.up_off);
  *data = s_dfu.ctrl_buf;
  *len = (uint16_t)n;

  if (n != 0U) {
    uint8_t idx = (s_dfu.up_valid[0] && s_dfu.up_buf_off[0] == s_dfu.up_off) ? 0U : 1U;

    if (!s_dfu.up_valid[idx] || s_dfu.up_buf_off[idx] != s_dfu.up_off) {
      return prv_fail(STATUS_ERR_UNKNOWN);
    }

    *data = s_dfu.up_buf[idx];
    s_dfu.up_last = idx;
    s_dfu.up_off += n;
  }

  if (n < setup->wLength) {
    prv_idle();
  } else {
    s_dfu.state = STATE_UPLOAD_IDLE;
    prv_read_ahead(s_dfu.up_off);
  }

  return 0;
}

static void prv_getstatus(void) {
  uint32_t poll_ms = 0U;

  switch (s_dfu.state) {
    case STATE_DNLOAD_SYNC:
    case STATE_DNBUSY:
      if (s_dfu.wr_busy) {
        s_dfu.state = STATE_DNBUSY;
        poll_ms = s_dfu.poll_ms;
      } else if (s_dfu.wr_status != STATUS_OK) {
        (void)prv_fail(s_dfu.wr_status);
      } else {
        s_dfu.state = STATE_DNLOAD_IDLE;
      }
      break;
    case STATE_MANIFEST_SYNC:
      prv_idle();
      break;
    default:
      break;
  }

  s_dfu.ctrl_buf[0] = s_dfu.status;
  pbl_put_le24(poll_ms, &s_dfu.ctrl_buf[1]);
  s_dfu.ctrl_buf[4] = s_dfu.state;
  s_dfu.ctrl_buf[5] = 0U;
}

static int prv_class_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  if (s_dfu.region == nullptr) {
    return -ENODEV;
  }

  switch (setup->bRequest) {
    case DFU_DNLOAD:
      return prv_dnload(setup, data, len);
    case DFU_UPLOAD:
      return prv_upload(setup, data, len);
    case DFU_GETSTATUS:
      prv_getstatus();
      *data = s_dfu.ctrl_buf;
      *len = 6U;
      return 0;
    case DFU_CLRSTATUS:
      if (s_dfu.state != STATE_ERROR) {
        return prv_fail(STATUS_ERR_STALLEDPKT);
      }
      prv_idle();
      return 0;
    case DFU_GETSTATE:
      s_dfu.ctrl_buf[0] = s_dfu.state;
      *data = s_dfu.ctrl_buf;
      *len = 1U;
      return 0;
    case DFU_ABORT:
      if (s_dfu.state == STATE_DNBUSY || s_dfu.state == STATE_ERROR) {
        return prv_fail(STATUS_ERR_STALLEDPKT);
      }
      prv_idle();
      return 0;
    default:
      return prv_fail(STATUS_ERR_STALLEDPKT);
  }
}

static void prv_control_out(const struct pbl_usb_setup *setup, const uint8_t *data, uint16_t len) {
  if (setup->bRequest == DFU_DNLOAD && len != 0U) {
    prv_dnload_block(len);
  }
}

static void prv_configured(bool on) {
  if (!on) {
    return;
  }

  s_dfu.region = nullptr;
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usb_dfu_region, usb_dfu_regions, region) {
    s_dfu.region = region;
    break;
  }

  if (s_dfu.region != nullptr && !s_dfu.wr_busy) {
    prv_idle();
  }
}

PBL_USBD_FUNCTION_DEFINE(dfu) = {
  .desc = s_desc,
  .desc_len = sizeof(s_desc),
  .first_iface = IFACE,
  .num_ifaces = 1U,
  .configured = prv_configured,
  .class_request = prv_class_request,
  .control_out = prv_control_out,
};
