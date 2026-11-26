/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/disk.h>
#include <pbl/drivers/udc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/usb/msc.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#define IFACE  0U
#define EP_OUT 0x01U
#define EP_IN  (PBL_UDC_EP_IN | 0x01U)

#define MSC_CLASS         0x08U
#define MSC_SUBCLASS_SCSI 0x06U
#define MSC_PROTOCOL_BOT  0x50U

#define REQ_GET_MAX_LUN 0xfeU
#define REQ_RESET       0xffU

#define MAX_LUNS 16U

#define CBW_SIGNATURE 0x43425355U
#define CSW_SIGNATURE 0x53425355U
#define CBW_LEN       31U
#define CSW_LEN       13U
#define CBW_FLAG_IN   0x80U
#define CB_MAX_LEN    16U

// Endpoints the function halted
#define HALT_IN  0x01U
#define HALT_OUT 0x02U

// OUT transfers take whole packets: a CBW takes one at high speed
#define CBW_BUF_LEN 512U
#define BUF_SIZE    CONFIG_USB_MSC_BUF_SIZE

#define CSW_PASSED      0x00U
#define CSW_FAILED      0x01U
#define CSW_PHASE_ERROR 0x02U

#define SCSI_TEST_UNIT_READY       0x00U
#define SCSI_REQUEST_SENSE         0x03U
#define SCSI_INQUIRY               0x12U
#define SCSI_MODE_SENSE_6          0x1aU
#define SCSI_START_STOP_UNIT       0x1bU
#define SCSI_PREVENT_ALLOW_REMOVAL 0x1eU
#define SCSI_READ_FORMAT_CAPACITY  0x23U
#define SCSI_READ_CAPACITY_10      0x25U
#define SCSI_READ_10               0x28U
#define SCSI_WRITE_10              0x2aU
#define SCSI_SYNCHRONIZE_CACHE_10  0x35U
#define SCSI_MODE_SENSE_10         0x5aU

#define SENSE_NO_SENSE        0x00U
#define SENSE_NOT_READY       0x02U
#define SENSE_MEDIUM_ERROR    0x03U
#define SENSE_ILLEGAL_REQUEST 0x05U
#define SENSE_DATA_PROTECT    0x07U

#define ASC_WRITE_ERROR          0x0cU
#define ASC_UNRECOVERED_READ     0x11U
#define ASC_INVALID_OPCODE       0x20U
#define ASC_LBA_OUT_OF_RANGE     0x21U
#define ASC_INVALID_FIELD_IN_CDB 0x24U
#define ASC_WRITE_PROTECTED      0x27U
#define ASC_MEDIUM_NOT_PRESENT   0x3aU
#define ASC_MEDIUM_REMOVAL       0x53U
#define ASCQ_REMOVAL_PREVENTED   0x02U

#define INQUIRY_LEN       36U
#define REQUEST_SENSE_LEN 18U
#define MODE_PAGE_CACHING 0x08U
#define MODE_PAGE_ALL     0x3fU
#define CACHING_PAGE_LEN  20U
#define MODE_WP           0x80U

static_assert(BUF_SIZE % CBW_BUF_LEN == 0U, "the buffers take whole high-speed packets");

static const struct PBL_PACKED {
  struct pbl_usb_desc_iface iface;
  struct pbl_usb_desc_ep out;
  struct pbl_usb_desc_ep in;
} s_desc = {
  .iface =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bNumEndpoints = 2U,
        .bInterfaceClass = MSC_CLASS,
        .bInterfaceSubClass = MSC_SUBCLASS_SCSI,
        .bInterfaceProtocol = MSC_PROTOCOL_BOT,
      },
  .out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_OUT,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .in = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes = PBL_UDC_EP_BULK,
    .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
  },
};

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_usb_msc_lun, usb_msc_luns);

PBL_USBD_FUNCTION_DECLARE(msc);

enum prv_state {
  // Not configured
  ST_OFF,
  // Waiting for a CBW
  ST_CBW,
  // A CBW arrived, for the work item
  ST_CMD,
  // The work item runs the command
  ST_EXEC,
  ST_DATA,
  ST_CSW,
  // An invalid CBW: both endpoints halted until a reset
  ST_WEDGED,
};

// A buffer of the data phase, filled and emptied in turn by the disk and the bus
enum prv_buf {
  BUF_FREE,
  BUF_DISK,
  BUF_READY,
  BUF_USB,
};

// What a command moves, as the work item found it
struct prv_cmd {
  uint8_t status;
  bool in;
  uint32_t len;
  bool disk;
  uint32_t lba;
  uint32_t sector_size;
};

static void prv_work(struct pbl_work *work);

// The interrupt receives CBWs and moves the data and CSWs, the work item runs the commands and
// the disk accesses. A reset or a configuration change bumps gen, so that the work item drops
// what it finishes for a command that is gone.
static struct {
  struct pbl_work work;
  enum prv_state state;
  uint32_t gen;
  uint8_t halted;
  uint8_t num_luns;
  uint32_t cbw_len;
  uint32_t tag;
  uint32_t host_len;
  bool host_in;
  uint32_t residue;
  uint8_t status;
  const struct pbl_usb_msc_lun *lun;
  bool disk;
  bool failed;
  uint32_t lba;
  uint32_t sector_size;
  uint32_t disk_left;
  uint32_t usb_left;
  uint8_t usb_idx;
  uint8_t disk_idx;
  enum prv_buf buf_state[2];
  uint32_t buf_len[2];
  uint8_t ctrl;
  uint8_t cbw[CBW_BUF_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
  uint8_t csw[CSW_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
  uint8_t buf[2][BUF_SIZE] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
} s_msc = {
  .work = PBL_WORK_INITIALIZER(prv_work),
};

static uint8_t prv_ep(uint8_t ep) {
  return pbl_usbd_ep(PBL_USBD_FUNCTION(msc), ep);
}

static void prv_halt(uint8_t bit) {
  s_msc.halted |= bit;
  (void)pbl_usbd_ep_set_halt(prv_ep(bit == HALT_IN ? EP_IN : EP_OUT), true);
}

static void prv_cbw_rx(void) {
  s_msc.state = ST_CBW;
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_OUT), s_msc.cbw, sizeof(s_msc.cbw));
}

// What the host still expects to move is halted, then the CSW goes, once the host clears it
static void prv_finish(void) {
  if (s_msc.residue != 0U) {
    prv_halt(s_msc.host_in ? HALT_IN : HALT_OUT);
  }

  pbl_put_le32(CSW_SIGNATURE, &s_msc.csw[0]);
  pbl_put_le32(s_msc.tag, &s_msc.csw[4]);
  pbl_put_le32(s_msc.residue, &s_msc.csw[8]);
  s_msc.csw[12] = s_msc.failed ? CSW_FAILED : s_msc.status;
  s_msc.state = ST_CSW;
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_IN), s_msc.csw, CSW_LEN);
}

// Data IN: sends the next buffer the disk filled, or ends once the disk is done
static void prv_in_next(void) {
  uint8_t i = s_msc.usb_idx;

  switch (s_msc.buf_state[i]) {
    case BUF_READY:
      s_msc.buf_state[i] = BUF_USB;
      (void)pbl_udc_ep_queue(UDC, prv_ep(EP_IN), s_msc.buf[i], s_msc.buf_len[i]);
      break;
    case BUF_FREE:
      if (s_msc.disk_left == 0U || s_msc.failed) {
        s_msc.residue += s_msc.usb_left;
        prv_finish();
      }
      break;
    default:
      break;
  }
}

// Data OUT: receives into the next free buffer, or ends once everything is on the disk
static void prv_out_next(void) {
  uint8_t i = s_msc.usb_idx;

  if (s_msc.usb_left != 0U) {
    if (s_msc.buf_state[i] == BUF_FREE) {
      s_msc.buf_len[i] = PBL_MIN(s_msc.usb_left, (uint32_t)BUF_SIZE);
      s_msc.buf_state[i] = BUF_USB;
      (void)pbl_udc_ep_queue(UDC, prv_ep(EP_OUT), s_msc.buf[i], s_msc.buf_len[i]);
    }
    return;
  }

  if (s_msc.buf_state[0] == BUF_FREE && s_msc.buf_state[1] == BUF_FREE) {
    prv_finish();
  }
}

static void prv_reset(enum prv_state state) {
  s_msc.gen++;
  s_msc.state = state;
  s_msc.usb_idx = 0U;
  s_msc.disk_idx = 0U;
  s_msc.buf_state[0] = BUF_FREE;
  s_msc.buf_state[1] = BUF_FREE;
}

static uint8_t prv_sense(const struct pbl_usb_msc_lun *lun, uint8_t key, uint8_t asc,
                         uint8_t ascq) {
  lun->state->sense_key = key;
  lun->state->asc = asc;
  lun->state->ascq = ascq;

  return key == SENSE_NO_SENSE ? CSW_PASSED : CSW_FAILED;
}

// The disk, if the host has not ejected it, else not ready
static uint8_t prv_medium(const struct pbl_usb_msc_lun *lun, struct pbl_disk_info *info) {
  if (lun->state->ejected || pbl_disk_get_info(lun->disk, info) != 0 || info->sector_count == 0U) {
    return prv_sense(lun, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT, 0U);
  }

  return CSW_PASSED;
}

static bool prv_read_only(const struct pbl_usb_msc_lun *lun, const struct pbl_disk_info *info) {
  return lun->read_only || info->read_only;
}

// Response to a command that returns data, len bytes in the first buffer, cut to what the host
// allows
static void prv_data_in(struct prv_cmd *cmd, uint32_t len, uint32_t alloc) {
  cmd->in = true;
  cmd->len = PBL_MIN(len, alloc);
}

static void prv_pad(uint8_t *dst, const char *src, size_t len) {
  size_t n = PBL_MIN(strlen(src), len);

  memcpy(dst, src, n);
  memset(&dst[n], ' ', len - n);
}

static uint8_t prv_inquiry(const struct pbl_usb_msc_lun *lun, const uint8_t *cb,
                           struct prv_cmd *cmd) {
  uint8_t *out = s_msc.buf[0];

  if ((cb[1] & 0x01U) != 0U) {
    return prv_sense(lun, SENSE_ILLEGAL_REQUEST, ASC_INVALID_FIELD_IN_CDB, 0U);
  }

  memset(out, 0, INQUIRY_LEN);
  out[1] = 0x80U;
  out[2] = 0x04U;
  out[3] = 0x02U;
  out[4] = INQUIRY_LEN - 5U;
  prv_pad(&out[8], CONFIG_USB_MSC_VENDOR, 8U);
  prv_pad(&out[16], CONFIG_USB_MSC_PRODUCT, 16U);
  prv_pad(&out[32], "1.00", 4U);
  prv_data_in(cmd, INQUIRY_LEN, pbl_get_be16(&cb[3]));

  return CSW_PASSED;
}

static uint8_t prv_request_sense(const struct pbl_usb_msc_lun *lun, const uint8_t *cb,
                                 struct prv_cmd *cmd) {
  uint8_t *out = s_msc.buf[0];

  memset(out, 0, REQUEST_SENSE_LEN);
  out[0] = 0x70U;
  out[2] = lun->state->sense_key;
  out[7] = REQUEST_SENSE_LEN - 8U;
  out[12] = lun->state->asc;
  out[13] = lun->state->ascq;
  prv_data_in(cmd, REQUEST_SENSE_LEN, cb[4]);

  return prv_sense(lun, SENSE_NO_SENSE, 0U, 0U);
}

// The caching page, write cache off, alone or as all the pages there are, after the header
static uint8_t prv_mode_sense(const struct pbl_usb_msc_lun *lun, const uint8_t *cb,
                              struct prv_cmd *cmd, bool ten) {
  struct pbl_disk_info info;
  uint8_t *out = s_msc.buf[0];
  uint8_t page = cb[2] & 0x3fU;
  uint8_t hdr = ten ? 8U : 4U;
  uint8_t len = hdr + CACHING_PAGE_LEN;
  uint8_t ret;

  ret = prv_medium(lun, &info);
  if (ret != CSW_PASSED) {
    return ret;
  }

  if (page != MODE_PAGE_ALL && page != MODE_PAGE_CACHING) {
    return prv_sense(lun, SENSE_ILLEGAL_REQUEST, ASC_INVALID_FIELD_IN_CDB, 0U);
  }

  memset(out, 0, len);
  out[hdr] = MODE_PAGE_CACHING;
  out[hdr + 1U] = CACHING_PAGE_LEN - 2U;
  if (ten) {
    pbl_put_be16(len - 2U, &out[0]);
    out[3] = prv_read_only(lun, &info) ? MODE_WP : 0U;
    prv_data_in(cmd, len, pbl_get_be16(&cb[7]));
  } else {
    out[0] = len - 1U;
    out[2] = prv_read_only(lun, &info) ? MODE_WP : 0U;
    prv_data_in(cmd, len, cb[4]);
  }

  return CSW_PASSED;
}

static uint8_t prv_start_stop(const struct pbl_usb_msc_lun *lun, const uint8_t *cb) {
  bool loej = (cb[4] & 0x02U) != 0U;
  bool start = (cb[4] & 0x01U) != 0U;

  if (!loej) {
    return CSW_PASSED;
  }

  if (start) {
    lun->state->ejected = false;
    return CSW_PASSED;
  }

  if (lun->state->prevent) {
    return prv_sense(lun, SENSE_ILLEGAL_REQUEST, ASC_MEDIUM_REMOVAL, ASCQ_REMOVAL_PREVENTED);
  }

  (void)pbl_disk_sync(lun->disk);
  lun->state->ejected = true;

  return CSW_PASSED;
}

static uint8_t prv_read_format_capacities(const struct pbl_usb_msc_lun *lun, const uint8_t *cb,
                                          struct prv_cmd *cmd) {
  struct pbl_disk_info info = {0};
  uint8_t *out = s_msc.buf[0];
  bool present = prv_medium(lun, &info) == CSW_PASSED;

  memset(out, 0, 12U);
  out[3] = 8U;
  pbl_put_be32(info.sector_count, &out[4]);
  // Formatted media, or no media
  out[8] = present ? 0x02U : 0x03U;
  pbl_put_be24(info.sector_size, &out[9]);
  prv_data_in(cmd, 12U, pbl_get_be16(&cb[7]));

  return prv_sense(lun, SENSE_NO_SENSE, 0U, 0U);
}

static uint8_t prv_read_capacity(const struct pbl_usb_msc_lun *lun, const uint8_t *cb,
                                 struct prv_cmd *cmd) {
  struct pbl_disk_info info;
  uint8_t *out = s_msc.buf[0];
  uint8_t ret;

  ret = prv_medium(lun, &info);
  if (ret != CSW_PASSED) {
    return ret;
  }

  pbl_put_be32(info.sector_count - 1U, &out[0]);
  pbl_put_be32(info.sector_size, &out[4]);
  cmd->in = true;
  cmd->len = 8U;

  return CSW_PASSED;
}

static uint8_t prv_rw(const struct pbl_usb_msc_lun *lun, const uint8_t *cb, struct prv_cmd *cmd,
                      bool write) {
  struct pbl_disk_info info;
  uint32_t lba = pbl_get_be32(&cb[2]);
  uint32_t count = pbl_get_be16(&cb[7]);
  uint8_t ret;

  ret = prv_medium(lun, &info);
  if (ret != CSW_PASSED) {
    return ret;
  }

  if (write && prv_read_only(lun, &info)) {
    return prv_sense(lun, SENSE_DATA_PROTECT, ASC_WRITE_PROTECTED, 0U);
  }

  if (lba > info.sector_count || count > info.sector_count - lba) {
    return prv_sense(lun, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0U);
  }

  if (info.sector_size > BUF_SIZE || BUF_SIZE % info.sector_size != 0U) {
    return prv_sense(lun, SENSE_ILLEGAL_REQUEST, ASC_INVALID_FIELD_IN_CDB, 0U);
  }

  cmd->in = !write;
  cmd->len = count * info.sector_size;
  cmd->disk = true;
  cmd->lba = lba;
  cmd->sector_size = info.sector_size;

  return CSW_PASSED;
}

static uint8_t prv_sync(const struct pbl_usb_msc_lun *lun) {
  struct pbl_disk_info info;
  uint8_t ret;

  ret = prv_medium(lun, &info);
  if (ret != CSW_PASSED) {
    return ret;
  }

  if (pbl_disk_sync(lun->disk) != 0) {
    return prv_sense(lun, SENSE_MEDIUM_ERROR, ASC_WRITE_ERROR, 0U);
  }

  return CSW_PASSED;
}

// Runs a command up to its data phase, which moves cmd->len bytes
static uint8_t prv_scsi(const struct pbl_usb_msc_lun *lun, const uint8_t *cb, struct prv_cmd *cmd) {
  struct pbl_disk_info info;

  if (cb[0] != SCSI_REQUEST_SENSE) {
    (void)prv_sense(lun, SENSE_NO_SENSE, 0U, 0U);
  }

  switch (cb[0]) {
    case SCSI_TEST_UNIT_READY:
      return prv_medium(lun, &info);
    case SCSI_REQUEST_SENSE:
      return prv_request_sense(lun, cb, cmd);
    case SCSI_INQUIRY:
      return prv_inquiry(lun, cb, cmd);
    case SCSI_MODE_SENSE_6:
      return prv_mode_sense(lun, cb, cmd, false);
    case SCSI_MODE_SENSE_10:
      return prv_mode_sense(lun, cb, cmd, true);
    case SCSI_START_STOP_UNIT:
      return prv_start_stop(lun, cb);
    case SCSI_PREVENT_ALLOW_REMOVAL:
      lun->state->prevent = (cb[4] & 0x01U) != 0U;
      return CSW_PASSED;
    case SCSI_READ_FORMAT_CAPACITY:
      return prv_read_format_capacities(lun, cb, cmd);
    case SCSI_READ_CAPACITY_10:
      return prv_read_capacity(lun, cb, cmd);
    case SCSI_READ_10:
      return prv_rw(lun, cb, cmd, false);
    case SCSI_WRITE_10:
      return prv_rw(lun, cb, cmd, true);
    case SCSI_SYNCHRONIZE_CACHE_10:
      return prv_sync(lun);
    default:
      PBL_LOG_DBG("msc: command 0x%02x not supported", cb[0]);
      return prv_sense(lun, SENSE_ILLEGAL_REQUEST, ASC_INVALID_OPCODE, 0U);
  }
}

static bool prv_cbw_valid(const uint8_t *cbw, uint32_t len) {
  uint8_t cb_len = cbw[14];

  return len == CBW_LEN && pbl_get_le32(&cbw[0]) == CBW_SIGNATURE &&
         (cbw[12] & ~CBW_FLAG_IN) == 0U && cbw[13] < s_msc.num_luns && cb_len != 0U &&
         cb_len <= CB_MAX_LEN;
}

// Runs the CBW that arrived; with interrupts locked, which it unlocks
static void prv_command(void) {
  uint8_t cbw[CBW_LEN];
  struct prv_cmd cmd = {0};
  const struct pbl_usb_msc_lun *lun;
  uint32_t gen = s_msc.gen;
  uint32_t host_len;
  bool host_in;

  if (!prv_cbw_valid(s_msc.cbw, s_msc.cbw_len)) {
    s_msc.state = ST_WEDGED;
    prv_halt(HALT_IN);
    prv_halt(HALT_OUT);
    pbl_irq_unlock();
    PBL_LOG_WRN("msc: invalid CBW");
    return;
  }

  memcpy(cbw, s_msc.cbw, sizeof(cbw));
  s_msc.state = ST_EXEC;
  pbl_irq_unlock();

  host_len = pbl_get_le32(&cbw[8]);
  host_in = (cbw[12] & CBW_FLAG_IN) != 0U;
  lun = &PBL_ITERABLE_SECTION_START(usb_msc_luns)[cbw[13]];
  cmd.status = prv_scsi(lun, &cbw[15], &cmd);

  // The thirteen cases of the transport: what the host and the device expect must agree, or the
  // command ends with a phase error and nothing moves
  if (cmd.len != 0U && (host_len < cmd.len || host_in != cmd.in)) {
    cmd.status = CSW_PHASE_ERROR;
    cmd.len = 0U;
    cmd.disk = false;
  }

  pbl_irq_lock();
  if (gen != s_msc.gen) {
    pbl_irq_unlock();
    return;
  }

  s_msc.tag = pbl_get_le32(&cbw[4]);
  s_msc.host_len = host_len;
  s_msc.host_in = host_in;
  s_msc.residue = host_len - cmd.len;
  s_msc.status = cmd.status;
  s_msc.failed = false;
  if (cmd.len == 0U) {
    prv_finish();
    pbl_irq_unlock();
    return;
  }

  s_msc.state = ST_DATA;
  s_msc.lun = lun;
  s_msc.disk = cmd.disk;
  s_msc.lba = cmd.lba;
  s_msc.sector_size = cmd.sector_size;
  s_msc.usb_left = cmd.len;
  s_msc.usb_idx = 0U;
  s_msc.disk_idx = 0U;
  if (cmd.disk) {
    s_msc.disk_left = cmd.len / cmd.sector_size;
  } else {
    s_msc.disk_left = 0U;
    s_msc.buf_len[0] = cmd.len;
    s_msc.buf_state[0] = BUF_READY;
  }
  if (cmd.in) {
    prv_in_next();
  } else {
    prv_out_next();
  }
  pbl_irq_unlock();
}

// Reads the next sectors into the buffer after the last one filled; with interrupts locked, which
// it unlocks
static void prv_disk_read(uint8_t i) {
  uint32_t gen = s_msc.gen;
  uint32_t ss = s_msc.sector_size;
  uint32_t n = PBL_MIN(s_msc.disk_left, BUF_SIZE / ss);
  uint32_t lba = s_msc.lba;
  int ret;

  s_msc.buf_state[i] = BUF_DISK;
  pbl_irq_unlock();

  ret = pbl_disk_read(s_msc.lun->disk, lba, n, s_msc.buf[i]);

  pbl_irq_lock();
  if (gen != s_msc.gen) {
    pbl_irq_unlock();
    return;
  }

  if (ret != 0) {
    PBL_LOG_ERR("msc: read of %u sectors at %u failed (%d)", (unsigned int)n, (unsigned int)lba,
                ret);
    (void)prv_sense(s_msc.lun, SENSE_MEDIUM_ERROR, ASC_UNRECOVERED_READ, 0U);
    s_msc.failed = true;
    s_msc.buf_state[i] = BUF_FREE;
  } else {
    s_msc.buf_len[i] = n * ss;
    s_msc.buf_state[i] = BUF_READY;
    s_msc.disk_idx ^= 1U;
    s_msc.lba += n;
    s_msc.disk_left -= n;
  }
  prv_in_next();
  pbl_irq_unlock();
}

// Writes the oldest buffer the bus filled; with interrupts locked, which it unlocks. After a
// failure, the rest of the data is received and dropped.
static void prv_disk_write(uint8_t i) {
  uint32_t gen = s_msc.gen;
  uint32_t ss = s_msc.sector_size;
  uint32_t len = s_msc.buf_len[i];
  uint32_t lba = s_msc.lba;
  bool failed = s_msc.failed;
  int ret = 0;

  s_msc.buf_state[i] = BUF_DISK;
  pbl_irq_unlock();

  if (!failed) {
    ret = (len % ss == 0U) ? pbl_disk_write(s_msc.lun->disk, lba, len / ss, s_msc.buf[i]) : -EIO;
  }

  pbl_irq_lock();
  if (gen != s_msc.gen) {
    pbl_irq_unlock();
    return;
  }

  if (ret != 0) {
    PBL_LOG_ERR("msc: write of %u bytes at %u failed (%d)", (unsigned int)len, (unsigned int)lba,
                ret);
    (void)prv_sense(s_msc.lun, SENSE_MEDIUM_ERROR, ASC_WRITE_ERROR, 0U);
    s_msc.failed = true;
  }
  s_msc.lba += len / ss;
  s_msc.buf_state[i] = BUF_FREE;
  s_msc.disk_idx ^= 1U;
  prv_out_next();
  pbl_irq_unlock();
}

static void prv_work(struct pbl_work *work) {
  for (;;) {
    uint8_t i;

    pbl_irq_lock();
    if (s_msc.state == ST_CMD) {
      prv_command();
      continue;
    }

    i = s_msc.disk_idx;
    if (s_msc.state == ST_DATA && s_msc.disk && s_msc.host_in && s_msc.buf_state[i] == BUF_FREE &&
        s_msc.disk_left != 0U && !s_msc.failed) {
      prv_disk_read(i);
      continue;
    }

    if (s_msc.state == ST_DATA && s_msc.disk && !s_msc.host_in && s_msc.buf_state[i] == BUF_READY) {
      prv_disk_write(i);
      continue;
    }

    pbl_irq_unlock();
    return;
  }
}

static void prv_xfer_done(uint8_t ep, int ret) {
  uint8_t i = s_msc.usb_idx;

  if (ret < 0) {
    return;
  }

  if (ep == EP_OUT && s_msc.state == ST_CBW) {
    s_msc.cbw_len = (uint32_t)ret;
    s_msc.state = ST_CMD;
    (void)pbl_work_submit(&pbl_sys_workq, &s_msc.work);
  } else if (ep == EP_OUT && s_msc.state == ST_DATA && !s_msc.host_in) {
    // A short packet ends what the host sends
    if ((uint32_t)ret < s_msc.buf_len[i]) {
      s_msc.residue += s_msc.usb_left - (uint32_t)ret;
      s_msc.usb_left = (uint32_t)ret;
      s_msc.failed = true;
    }
    s_msc.buf_len[i] = (uint32_t)ret;
    s_msc.buf_state[i] = BUF_READY;
    s_msc.usb_left -= (uint32_t)ret;
    s_msc.usb_idx ^= 1U;
    prv_out_next();
    (void)pbl_work_submit(&pbl_sys_workq, &s_msc.work);
  } else if (ep == EP_IN && s_msc.state == ST_DATA && s_msc.host_in) {
    s_msc.buf_state[i] = BUF_FREE;
    s_msc.usb_left -= s_msc.buf_len[i];
    s_msc.usb_idx ^= 1U;
    prv_in_next();
    (void)pbl_work_submit(&pbl_sys_workq, &s_msc.work);
  } else if (ep == EP_IN && s_msc.state == ST_CSW) {
    prv_cbw_rx();
  }
}

// In flight transfers end, the halts stay
static void prv_abort(void) {
  uint16_t mps = pbl_usbd_bulk_mps();

  (void)pbl_udc_ep_disable(UDC, prv_ep(EP_IN));
  (void)pbl_udc_ep_disable(UDC, prv_ep(EP_OUT));
  (void)pbl_udc_ep_enable(UDC, prv_ep(EP_IN), PBL_UDC_EP_BULK, mps);
  (void)pbl_udc_ep_enable(UDC, prv_ep(EP_OUT), PBL_UDC_EP_BULK, mps);
  if ((s_msc.halted & HALT_IN) != 0U) {
    prv_halt(HALT_IN);
  }
  if ((s_msc.halted & HALT_OUT) != 0U) {
    prv_halt(HALT_OUT);
  }
}

static int prv_class_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  if ((setup->bmRequestType & PBL_USB_TYPE_MASK) != PBL_USB_TYPE_CLASS || s_msc.state == ST_OFF ||
      setup->wValue != 0U) {
    return -ENOTSUP;
  }

  switch (setup->bRequest) {
    case REQ_GET_MAX_LUN:
      if ((setup->bmRequestType & PBL_USB_DIR_IN) == 0U || setup->wLength != 1U ||
          s_msc.num_luns == 0U) {
        return -EINVAL;
      }
      s_msc.ctrl = s_msc.num_luns - 1U;
      *data = &s_msc.ctrl;
      *len = 1U;
      return 0;
    case REQ_RESET:
      if ((setup->bmRequestType & PBL_USB_DIR_IN) != 0U || setup->wLength != 0U) {
        return -EINVAL;
      }
      prv_abort();
      prv_reset(ST_CBW);
      prv_cbw_rx();
      return 0;
    default:
      return -ENOTSUP;
  }
}

static bool prv_clear_halt(uint8_t ep) {
  if (s_msc.state == ST_WEDGED) {
    return false;
  }

  s_msc.halted &= ~(ep == EP_IN ? HALT_IN : HALT_OUT);

  return true;
}

static void prv_configured(bool on) {
  ptrdiff_t n = PBL_ITERABLE_SECTION_END(usb_msc_luns) - PBL_ITERABLE_SECTION_START(usb_msc_luns);

  s_msc.halted = 0U;
  if (!on) {
    prv_reset(ST_OFF);
    return;
  }

  s_msc.num_luns = (uint8_t)PBL_MIN(n, (ptrdiff_t)MAX_LUNS);
  for (uint8_t i = 0U; i < s_msc.num_luns; i++) {
    struct pbl_usb_msc__lun_state *st = PBL_ITERABLE_SECTION_START(usb_msc_luns)[i].state;

    st->ejected = false;
    st->prevent = false;
  }

  prv_reset(ST_CBW);
  prv_cbw_rx();
}

PBL_USBD_FUNCTION_DEFINE(msc) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
  .configured = prv_configured,
  .class_request = prv_class_request,
  .xfer_done = prv_xfer_done,
  .clear_halt = prv_clear_halt,
};
