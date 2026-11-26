/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <errno.h>
#include <string.h>

#include <pbl/util/byteorder.h>

static uint32_t prv_key(uint16_t index, uint8_t subindex) {
  return ((uint32_t)index << 8) | subindex;
}

/* First entry at or after @p key. */
static size_t prv_lower_bound(const struct pbl_canopen_od *od, uint32_t key) {
  size_t lo = 0U;
  size_t hi = od->num_entries;

  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2U;
    const struct pbl_canopen_od_entry *e = &od->entries[mid];

    if (prv_key(e->index, e->subindex) < key) {
      lo = mid + 1U;
    } else {
      hi = mid;
    }
  }

  return lo;
}

const struct pbl_canopen_od_entry *pbl_canopen__od_find(const struct pbl_canopen_od *od,
                                                        uint16_t index, uint8_t subindex) {
  size_t i = prv_lower_bound(od, prv_key(index, subindex));

  if (i < od->num_entries && od->entries[i].index == index && od->entries[i].subindex == subindex) {
    return &od->entries[i];
  }

  return nullptr;
}

bool pbl_canopen__od_has_index(const struct pbl_canopen_od *od, uint16_t index) {
  size_t i = prv_lower_bound(od, prv_key(index, 0U));

  return i < od->num_entries && od->entries[i].index == index;
}

static bool prv_is_string(const struct pbl_canopen_od_entry *e) {
  return e->type == PBL_CANOPEN_TYPE_VISIBLE_STRING || e->type == PBL_CANOPEN_TYPE_OCTET_STRING;
}

static uint32_t prv_get_uint(const struct pbl_canopen_od_entry *e) {
  switch (e->size) {
    case 1U:
      return *(const uint8_t *)e->data;
    case 2U:
      return *(const uint16_t *)e->data;
    case 4U:
      return *(const uint32_t *)e->data;
    default:
      return 0U;
  }
}

static void prv_set_uint(const struct pbl_canopen_od_entry *e, uint32_t val) {
  switch (e->size) {
    case 1U:
      *(uint8_t *)e->data = (uint8_t)val;
      break;
    case 2U:
      *(uint16_t *)e->data = (uint16_t)val;
      break;
    case 4U:
      *(uint32_t *)e->data = val;
      break;
    default:
      break;
  }
}

uint32_t pbl_canopen__od_get_u32(const struct pbl_canopen_od *od, uint16_t index,
                                 uint8_t subindex) {
  const struct pbl_canopen_od_entry *e = pbl_canopen__od_find(od, index, subindex);

  return e != nullptr ? prv_get_uint(e) : 0U;
}

void pbl_canopen__od_set_u32(const struct pbl_canopen_od *od, uint16_t index, uint8_t subindex,
                             uint32_t val) {
  const struct pbl_canopen_od_entry *e = pbl_canopen__od_find(od, index, subindex);

  if (e != nullptr) {
    prv_set_uint(e, val);
  }
}

size_t pbl_canopen__od_len(const struct pbl_canopen_od_entry *e) {
  if (e->type == PBL_CANOPEN_TYPE_VISIBLE_STRING) {
    const char *end = memchr(e->data, '\0', e->size);

    return end != nullptr ? (size_t)(end - (const char *)e->data) : e->size;
  }

  return e->size;
}

static void prv_put(uint8_t *dst, const void *src, size_t size) {
  switch (size) {
    case 2U:
      pbl_put_le16(*(const uint16_t *)src, dst);
      break;
    case 4U:
      pbl_put_le32(*(const uint32_t *)src, dst);
      break;
    case 8U:
      pbl_put_le64(*(const uint64_t *)src, dst);
      break;
    default:
      memcpy(dst, src, size);
      break;
  }
}

static void prv_get(void *dst, const uint8_t *src, size_t size) {
  switch (size) {
    case 2U:
      *(uint16_t *)dst = pbl_get_le16(src);
      break;
    case 4U:
      *(uint32_t *)dst = pbl_get_le32(src);
      break;
    case 8U:
      *(uint64_t *)dst = pbl_get_le64(src);
      break;
    default:
      memcpy(dst, src, size);
      break;
  }
}

void pbl_canopen__od_to_wire(const struct pbl_canopen_od_entry *e, uint8_t *dst, size_t len) {
  if (prv_is_string(e)) {
    memcpy(dst, e->data, len);
  } else {
    prv_put(dst, e->data, e->size);
  }
}

void pbl_canopen__od_from_wire(const struct pbl_canopen_od_entry *e, void *dst, const uint8_t *src,
                               size_t len) {
  if (prv_is_string(e)) {
    memcpy(dst, src, len);
  } else {
    prv_get(dst, src, e->size);
  }
}

static uint32_t prv_check_len(const struct pbl_canopen_od_entry *e, size_t len) {
  if (len > e->size) {
    return PBL_CANOPEN__ABORT_TYPE_LEN_HIGH;
  }
  if (len < e->size && e->type != PBL_CANOPEN_TYPE_VISIBLE_STRING) {
    return PBL_CANOPEN__ABORT_TYPE_LEN_LOW;
  }

  return 0U;
}

static uint32_t prv_errno_to_abort(int err) {
  switch (err) {
    case 0:
      return 0U;
    case -EINVAL:
    case -ERANGE:
      return PBL_CANOPEN__ABORT_VALUE;
    case -EBUSY:
      return PBL_CANOPEN__ABORT_DATA_STATE;
    case -EACCES:
    case -EPERM:
      return PBL_CANOPEN__ABORT_DATA_LOCAL;
    default:
      return PBL_CANOPEN__ABORT_DATA;
  }
}

static uint32_t prv_check(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                          const void *val, size_t len) {
  const struct pbl_canopen_callbacks *cb = co->cfg.cb;
  uint32_t v = 0U;

  if (e->index >= 0x2000U) {
#ifdef CONFIG_CANOPEN_CIA402
    uint32_t abort = pbl_canopen__cia402_check(co, e, val);

    if (abort != 0U) {
      return abort;
    }
#endif
    if (cb == nullptr || cb->write == nullptr) {
      return 0U;
    }
    return prv_errno_to_abort(cb->write(co, e->index, e->subindex, val, len, co->cfg.arg));
  }

  if (!prv_is_string(e)) {
    switch (e->size) {
      case 1U:
        v = *(const uint8_t *)val;
        break;
      case 2U:
        v = *(const uint16_t *)val;
        break;
      case 4U:
        v = *(const uint32_t *)val;
        break;
      default:
        break;
    }
  }

  switch (e->index) {
    case PBL_CANOPEN__IDX_ERRORS:
    case PBL_CANOPEN__IDX_EMCY:
      return pbl_canopen__emcy_check(co, e, &v);
    case PBL_CANOPEN__IDX_SYNC:
      return (v & (PBL_CANOPEN__COB_GEN | PBL_CANOPEN__COB_INVALID)) != 0U
                 ? PBL_CANOPEN__ABORT_VALUE
                 : 0U;
    case PBL_CANOPEN__IDX_HB_CONSUMER:
      return pbl_canopen__hb_consumer_check(co, e->subindex, v);
    default:
      break;
  }

  if (e->index >= PBL_CANOPEN__IDX_RPDO_COMM && e->index < 0x1c00U) {
    return pbl_canopen__pdo_check(co, e, &v);
  }

  return 0U;
}

static void prv_apply(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e) {
  if (!co->started) {
    return;
  }

  switch (e->index) {
    case PBL_CANOPEN__IDX_HB_CONSUMER:
    case PBL_CANOPEN__IDX_HB_PRODUCER:
      pbl_canopen__hb_config(co);
      return;
    default:
      break;
  }

  if (e->index >= PBL_CANOPEN__IDX_RPDO_COMM && e->index < 0x1c00U) {
    pbl_canopen__pdo_written(co, e);
  }
#ifdef CONFIG_CANOPEN_CIA402
  pbl_canopen__cia402_written(co, e);
#endif
}

void pbl_canopen__od_store(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                           const void *val, size_t len) {
  bool changed = len != pbl_canopen__od_len(e) || memcmp(e->data, val, len) != 0;

  memcpy(e->data, val, len);
  if (len < e->size) {
    memset((uint8_t *)e->data + len, 0, e->size - len);
  }

  prv_apply(co, e);
  if (changed && (e->attr & PBL_CANOPEN_ATTR_TPDO) != 0U) {
    pbl_canopen__tpdo_changed(co, e);
  }
}

uint32_t pbl_canopen__od_write_bus(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                   const uint8_t *data, size_t len) {
  uint64_t num;
  const void *val = data;
  uint32_t abort;

  if ((e->attr & PBL_CANOPEN_ATTR_WRITE) == 0U) {
    return PBL_CANOPEN__ABORT_READ_ONLY;
  }

  abort = prv_check_len(e, len);
  if (abort != 0U) {
    return abort;
  }

  if (!prv_is_string(e)) {
    pbl_canopen__od_from_wire(e, &num, data, len);
    val = &num;
  }

#ifdef CONFIG_CANOPEN_STORAGE
  /* Commands, not values: they read back as what the node supports. */
  if (e->index == PBL_CANOPEN__IDX_STORE || e->index == PBL_CANOPEN__IDX_RESTORE) {
    return pbl_canopen__storage_write(co, e, *(const uint32_t *)val);
  }
#endif

  abort = prv_check(co, e, val, len);
  if (abort != 0U) {
    return abort;
  }

  pbl_canopen__od_store(co, e, val, len);

  return 0U;
}

void pbl_canopen__od_reset(struct pbl_canopen *co, bool app) {
  const struct pbl_canopen_od *od = co->cfg.od;

  memcpy(od->comm, od->comm_defaults, od->comm_size);
  if (app) {
    memcpy(od->app, od->app_defaults, od->app_size);
  }

  for (size_t i = 0U; i < od->num_entries; i++) {
    const struct pbl_canopen_od_entry *e = &od->entries[i];

    if ((e->attr & PBL_CANOPEN_ATTR_NODEID) != 0U) {
      prv_set_uint(e, prv_get_uint(e) + co->cfg.node_id);
    }
  }
}
