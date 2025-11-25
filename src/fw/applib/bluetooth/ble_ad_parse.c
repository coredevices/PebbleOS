/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_ad_parse.h"

#include "applib/applib_malloc.auto.h"

#include "syscall/syscall.h"
#include "system/passert.h"

#include "util/math.h"
#include "util/net.h"

#include <btutil/bt_uuid.h>

// -----------------------------------------------------------------------------
//! Internal parsed advertisement data structures.

// -----------------------------------------------------------------------------
//! AD TYPE Values as specified by the Bluetooth 4.0 Spec.
//! See "Appendix C (Normative): EIR and AD Formats" in Core_v4.0.pdf
typedef enum {
  BLEAdTypeFlags                     = 0x01,
  BLEAdTypeService16BitUUIDPartial   = 0x02,
  BLEAdTypeService16BitUUIDComplete  = 0x03,
  BLEAdTypeService32BitUUIDPartial   = 0x04,
  BLEAdTypeService32BitUUIDComplete  = 0x05,
  BLEAdTypeService128BitUUIDPartial  = 0x06,
  BLEAdTypeService128BitUUIDComplete = 0x07,

  BLEAdTypeLocalNameShortened        = 0x08,
  BLEAdTypeLocalNameComplete         = 0x09,

  BLEAdTypeTxPowerLevel              = 0x0a,

  BLEAdTypeManufacturerSpecific      = 0xff,
} BLEAdType;

// -----------------------------------------------------------------------------
//! AD DATA element header
typedef struct __attribute__((__packed__)) {
  uint8_t length;
  BLEAdType type:8;
} BLEAdElementHeader;

typedef struct __attribute__((__packed__)) {
  BLEAdElementHeader header;
  uint8_t data[];
} BLEAdElement;

// -----------------------------------------------------------------------------
// Consuming BLEAdData:
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//! Internal parser callback. Gets called for a parsed Service UUIDs element.
//! @param uuids The array with Service UUIDs
//! @param count The number of Service UUIDs the array contains
//! @param cb_data Pointer to arbitrary client data as passed to the parse call.
//! @return true to continue parsing or false to stop after returning.
typedef bool (*BLEAdParseServicesCallback)(const Uuid uuids[], uint8_t count,
                                           void *cb_data);

// -----------------------------------------------------------------------------
//! Internal parser callback. Gets called for a parsed Local Name element.
//! @param local_name_bytes This is a *NON* zero terminated UTF-8 string.
//! @param length The length of local_name_bytes
//! @param cb_data Pointer to arbitrary client data as passed to the parse call.
//! @return true to continue parsing or false to stop after returning.
typedef bool (*BLEAdParseLocalNameCallback)(const uint8_t *local_name_bytes,
                                            uint8_t length, void *cb_data);

// -----------------------------------------------------------------------------
//! Internal parser callback. Gets called for a parsed TX Power Level element.
//! @param tx_power_level The TX Power Level value.
//! @param cb_data Pointer to arbitrary client data as passed to the parse call.
//! @return true to continue parsing or false to stop after returning.
typedef bool (*BLEAdParseTXPowerLevelCallback)(int8_t tx_power_level,
                                               void *cb_data);

// -----------------------------------------------------------------------------
//! Internal parser callback. Gets called for a Manufacturer Specific data elem.
//! @param company_id The Company ID
//! @param data The Manufacturer Specific data
//! @param length The length in bytes of data
//! @param cb_data Pointer to arbitrary client data as passed to the parse call.
//! @return true to continue parsing or false to stop after returning.
typedef bool (*BLEAdParseManufacturerSpecificCallback)(uint16_t company_id,
                                                       const uint8_t *data,
                                                       uint8_t length,
                                                       void *cb_data);

typedef struct {
  BLEAdParseServicesCallback services_cb;
  BLEAdParseLocalNameCallback local_name_cb;
  BLEAdParseTXPowerLevelCallback tx_power_level_cb;
  BLEAdParseManufacturerSpecificCallback manufacturer_cb;
} BLEAdParseCallbacks;

// -----------------------------------------------------------------------------
//! Parser for Services List data elements.
static bool parse_services_list(const BLEAdElement *elem,
                                const BLEAdParseCallbacks *callbacks,
                                void *cb_data) {
  const uint8_t uuid_type = elem->header.type / 2;
  // Most common is probably 128-bit UUID, then 16-bit, then 32-bit:
  const size_t uuid_width = (uuid_type == 3) ? 16 : ((uuid_type == 1) ? 2 : 4);
  const size_t num_uuids = (elem->header.length - 1 /* Type byte */)
                                                                  / uuid_width;

  if (!num_uuids) {
    return true; // continue parsing
  }

  Uuid uuids[num_uuids];

  // Iterate through the list, expanding each UUID to 128-bit equivalents,
  // then copying them into the uuids[] array:
  const uint8_t *uuid_data = elem->data;
  for (size_t i = 0; i < num_uuids; ++i) {
    switch (uuid_type) {
      case 1: { // 16 bit
        uint16_t u16 = *(uint16_t *) uuid_data;
        uuids[i] = bt_uuid_expand_16bit(u16);
        uuid_data += sizeof(uint16_t);
        break;
      }
      case 2: { // 32 bit
        uint32_t u32 = *(uint32_t *) uuid_data;
        uuids[i] = bt_uuid_expand_32bit(u32);
        uuid_data += sizeof(uint32_t);
        break;
      }
      case 3: // 128-bit
        uuids[i] = UuidMakeFromLEBytes(uuid_data);
        uuid_data += sizeof(Uuid);
        break;
    }
  }

  // Call back to client with parsed data:
  return callbacks->services_cb(uuids, num_uuids, cb_data);
}

// -----------------------------------------------------------------------------
//! Parser for Local Name data element.
static bool parse_local_name(const BLEAdElement *elem,
                             const BLEAdParseCallbacks *callbacks,
                             void *cb_data) {
  // Length of the raw string:
  const uint8_t raw_length = elem->header.length - 1 /* -1 Type byte */;
  if (!raw_length) {
    return true; // continue parsing
  }

  // Call back to client with parsed data:
  return callbacks->local_name_cb(elem->data, raw_length, cb_data);
}

// -----------------------------------------------------------------------------
//! Parser for TX Power Level data element.
static bool parse_power_level(const BLEAdElement *elem,
                              const BLEAdParseCallbacks *callbacks,
                              void *cb_data) {
  if (elem->header.length != 2) {
    // In case the length is not what it should be, do not add data element.
    return true; // continue parsing
  }

  const int8_t tx_power_level = *(int8_t *)elem->data;

  // Call back to client with parsed data:
  return callbacks->tx_power_level_cb(tx_power_level, cb_data);
}

// -----------------------------------------------------------------------------
//! Parser for Manufacturer Specific data element.

static bool parse_manufact_spec(const BLEAdElement *elem,
                                const BLEAdParseCallbacks *callbacks,
                                void *cb_data) {

  if (elem->header.length < 3) {
    // The first 2 octets should be the Company Identifier Code
    // (+1 for Type byte)
    return true;
  }

  if (callbacks->manufacturer_cb) {
    // Little-endian:
    const uint16_t *company_id = (uint16_t *) elem->data;

    // Call back to client with parsed data:
    const uint8_t manufacturer_data_size =
        elem->header.length - sizeof(*company_id) - 1 /* -1 Type Byte */;
    return callbacks->manufacturer_cb(ltohs(*company_id),
                                      elem->data + sizeof(*company_id),
                                      manufacturer_data_size,
                                      cb_data);
  }

  return true; // continue parsing
}

// -----------------------------------------------------------------------------
//! @param ad_data The advertising data and scan response data to parse.
//! @param callbacks Callbacks for each of the types of data the client wants
//! to receive parse callbacks for. You can leave a callback NULL if you are not
//! interested in receiving callbacks for that type of data.
//! @param cb_data Pointer to client data that is passed into the callback.
static void ble_ad_parse_ad_data(const BLEAdData *ad_data,
                                 const BLEAdParseCallbacks *callbacks,
                                 void *cb_data) {
  const uint8_t *cursor = ad_data->data;
  const uint8_t *end = cursor + ad_data->ad_data_length +
                            ad_data->scan_resp_data_length;
  while (cursor < end) {

    const BLEAdElement *elem = (const BLEAdElement *) cursor;
    if (elem->header.length == 0) {
      // We've hit a padding zero. We should be done, or this packet is corrupt.
      return;
    }

    if (cursor + elem->header.length + 1 /* +1 length byte */ > end) {
      return; // corrupted
    }

    bool (*parse_func)(const BLEAdElement *,
                       const BLEAdParseCallbacks *, void *) = NULL;

    switch (elem->header.type) {
      case BLEAdTypeService16BitUUIDPartial ... BLEAdTypeService128BitUUIDComplete:
        if (callbacks->services_cb) {
          parse_func = parse_services_list;
        }
        break;

      case BLEAdTypeLocalNameShortened ... BLEAdTypeLocalNameComplete:
        if (callbacks->local_name_cb) {
          parse_func = parse_local_name;
        }
        break;

      case BLEAdTypeTxPowerLevel:
        if (callbacks->tx_power_level_cb) {
          parse_func = parse_power_level;
        }
        break;

      case BLEAdTypeManufacturerSpecific:
        if (callbacks->manufacturer_cb) {
          parse_func = parse_manufact_spec;
        }
        break;

      default: // parse_func == NULL
        break;

    } // switch()

    if (parse_func) {
      if (!parse_func(elem, callbacks, cb_data)) {
        // The callback indicated we should not continue parsing
        return;
      }
    }

    // The Length byte itself is not counted, so +1:
    cursor += elem->header.length + 1;
  }
}

// -----------------------------------------------------------------------------
//! ble_ad_includes_service() wrapper and helper function:

struct IncludesServiceCtx {
  const Uuid *service_uuid;
  bool included;
};

static bool includes_service_parse_cb(const Uuid uuids[], uint8_t count,
                                      void *cb_data) {
  struct IncludesServiceCtx *ctx = (struct IncludesServiceCtx *) cb_data;
  for (int i = 0; i < count; ++i) {
    if (uuid_equal(ctx->service_uuid, &uuids[i])) {
      // Found!
      ctx->included = true;
      return false; // stop parsing
    }
  }
  return true; // continue parsing
}

bool ble_ad_includes_service(const BLEAdData *ad, const Uuid *service_uuid) {
   struct IncludesServiceCtx ctx = {
    .service_uuid = service_uuid,
    .included = false,
  };
  const BLEAdParseCallbacks callbacks = (const BLEAdParseCallbacks) {
    .services_cb = includes_service_parse_cb,
  };
  ble_ad_parse_ad_data(ad, &callbacks, &ctx);
  return ctx.included;
}

// -----------------------------------------------------------------------------
//! ble_ad_copy_service_uuids() wrapper and helper function:

struct CopyServiceUUIDsCtx {
  Uuid *uuids_out;
  const uint8_t max;
  uint8_t copied;
  uint8_t total;
};

static bool copy_services_parse_cb(const Uuid uuids[], uint8_t count,
                                   void *cb_data) {
  struct CopyServiceUUIDsCtx *ctx = (struct CopyServiceUUIDsCtx *)cb_data;
  for (int i = 0; i < count; ++i) {
    if (ctx->copied < ctx->max) {
      // Still space left, so copy:
      const Uuid *uuid = &uuids[i];
      memcpy(&ctx->uuids_out[ctx->copied++], uuid, sizeof(Uuid));
    }
    ++ctx->total;
  }
  return false; // stop parsing, only one Services UUID element allowed by spec
}

uint8_t ble_ad_copy_service_uuids(const BLEAdData *ad,
                                  Uuid *uuids_out,
                                  uint8_t num_uuids) {
  struct CopyServiceUUIDsCtx ctx = {
    .uuids_out = uuids_out,
    .max = num_uuids,
  };
  const BLEAdParseCallbacks callbacks = {
    .services_cb = copy_services_parse_cb,
  };
  ble_ad_parse_ad_data(ad, &callbacks, &ctx);
  return ctx.total;
}

// -----------------------------------------------------------------------------
//! ble_ad_get_tx_power_level() wrapper and helper function:

struct TxPowerLevelCtx {
  int8_t *tx_power_level_out;
  bool included;
};

static bool tx_power_level_cb(int8_t tx_power_level,
                              void *cb_data) {
  struct TxPowerLevelCtx *ctx = (struct TxPowerLevelCtx *)cb_data;
  *ctx->tx_power_level_out = tx_power_level;
  ctx->included = true;
  return false; // stop parsing
}

bool ble_ad_get_tx_power_level(const BLEAdData *ad,
                               int8_t *tx_power_level_out) {
  struct TxPowerLevelCtx ctx = {
    .tx_power_level_out = tx_power_level_out,
    .included = false,
  };
  const BLEAdParseCallbacks callbacks = {
    .tx_power_level_cb = tx_power_level_cb,
  };
  ble_ad_parse_ad_data(ad, &callbacks, &ctx);
  return ctx.included;
}

// -----------------------------------------------------------------------------
//! ble_ad_copy_local_name() wrapper and helper function:

struct LocalNameCtx {
  char *buffer;
  const uint8_t size;
  size_t copied_size;
};

static bool copy_local_name_parse_cb(const uint8_t *local_name_bytes,
                                     uint8_t length, void *cb_data) {
  struct LocalNameCtx *ctx = (struct LocalNameCtx *)cb_data;
  const uint8_t copied_size = MIN(ctx->size, length + 1 /* zero terminator */);

  memcpy(ctx->buffer, local_name_bytes, copied_size - 1);
  ctx->buffer[copied_size - 1] = 0; // zero terminator
  ctx->copied_size = copied_size;

  return false; // stop parsing
}

size_t ble_ad_copy_local_name(const BLEAdData *ad, char *buffer, size_t size) {
  struct LocalNameCtx ctx = {
    .buffer = buffer,
    .size = MIN(size, 0xff),
    .copied_size = 0,
  };
  const BLEAdParseCallbacks callbacks = {
    .local_name_cb = copy_local_name_parse_cb,
  };
  ble_ad_parse_ad_data(ad, &callbacks, &ctx);
  return ctx.copied_size;
}

// -----------------------------------------------------------------------------
//! ble_ad_get_raw_data_size() wrapper and helper function:

size_t ble_ad_get_raw_data_size(const BLEAdData *ad) {
  return ad->ad_data_length + ad->scan_resp_data_length;
}

// -----------------------------------------------------------------------------
//! ble_ad_copy_raw_data() wrapper and helper function:

size_t ble_ad_copy_raw_data(const BLEAdData *ad, uint8_t *buffer, size_t size) {
  const size_t size_to_copy = ble_ad_get_raw_data_size(ad);
  if (size < size_to_copy) {
    return 0;
  }
  memcpy(buffer, ad->data, size_to_copy);
  return size_to_copy;
}

// -----------------------------------------------------------------------------
//! ble_ad_get_manufacturer_specific_data() wrapper and helper function:

struct ManufacturerSpecificCtx {
  uint16_t company_id;
  uint8_t *buffer;
  const uint8_t size;
  size_t copied_size;
};

static bool copy_manufacturer_specific_parse_cb(uint16_t company_id,
                                                const uint8_t *data,
                                                uint8_t length,
                                                void *cb_data) {

  struct ManufacturerSpecificCtx *ctx =
      (struct ManufacturerSpecificCtx *) cb_data;
  const uint8_t copied_size = MIN(ctx->size, length);

  memcpy(ctx->buffer, data, copied_size);
  ctx->copied_size = copied_size;
  ctx->company_id = company_id;

  return false; // stop parsing
}

size_t ble_ad_copy_manufacturer_specific_data(const BLEAdData *ad,
                                              uint16_t *company_id,
                                              uint8_t *buffer, size_t size) {
  struct ManufacturerSpecificCtx ctx = {
    .size = size,
    .buffer = buffer,
  };
  const BLEAdParseCallbacks callbacks = {
    .manufacturer_cb = copy_manufacturer_specific_parse_cb,
  };
  ble_ad_parse_ad_data(ad, &callbacks, &ctx);
  if (company_id) {
    *company_id = ctx.company_id;
  }
  return ctx.copied_size;
}

// -----------------------------------------------------------------------------
// Creating BLEAdData:
// -----------------------------------------------------------------------------

//! Magic high bit used as scan_resp_data_length to indicate that the ad_data
//! has been finalized and the next write should be counted towards the scan
//! response payload. The maximum scan_resp_data_length is 31 bytes, so this
//! value lies outside of the valid range. This is basically a memory savings
//! optimization, saving another "finalized" bool.
#define BLE_AD_DATA_FINALIZED ((uint8_t) 0x80)

bool prv_ad_is_finalized(const BLEAdData *ad_data) {
  // Scan response data has already been added / started
  return (ad_data->scan_resp_data_length != 0);
}

void ble_ad_start_scan_response(BLEAdData *ad_data) {
  if (prv_ad_is_finalized(ad_data)) {
    // Already finalized
    return;
  }
  ad_data->scan_resp_data_length = BLE_AD_DATA_FINALIZED;
}

// -----------------------------------------------------------------------------
//! Helper to calculate whether a number of bytes will still fit when appended.
//! @param length Pointer to the length of the part for which to try to fit in
//! size_to_write number of bytes.
//! @param size_to_write The number of bytes to that need to be appended.
//! @return Pointer to length if it could still appends size_to_write bytes or
//! NULL if not.
static uint8_t *prv_length_ptr_if_fits_or_null(uint8_t *length,
                                               size_t size_to_write) {
  // Unset finalized bit:
  const uint8_t used = *length & (~BLE_AD_DATA_FINALIZED);
  const uint8_t left = GAP_LE_AD_REPORT_DATA_MAX_LENGTH - used;
  // Return pointer to the pointer if size_to_write will fit, or NULL otherwise:
  return (left >= size_to_write) ? length : NULL;
}

// -----------------------------------------------------------------------------
//! @return Pointer to the length that is incremented when writing size_to_write
//! number of bytes, or NULL if there is not enough space left.
static uint8_t* prv_length_to_increase(BLEAdData *ad_data,
                                size_t size_to_write) {
  if (ad_data->scan_resp_data_length) {
    // The scan response part is already being populated:
    return prv_length_ptr_if_fits_or_null(&ad_data->scan_resp_data_length,
                                          size_to_write);
  } else {
    // The advertisement is still being populated:
    uint8_t *length = prv_length_ptr_if_fits_or_null(&ad_data->ad_data_length,
                                                     size_to_write);
    if (length) {
      // Hurray, the size_to_write fits in the advertisement part:
      return length;
    }
    // Last resort, try fitting into scan response part:
    return prv_length_ptr_if_fits_or_null(&ad_data->scan_resp_data_length,
                                          size_to_write);
  }
}

// -----------------------------------------------------------------------------
bool prv_write_element_to_ad_data(BLEAdData *ad_data,
                                  const BLEAdElement *element) {
  if (!ad_data || !element) {
    return false;
  }
  const size_t size_to_write = element->header.length + 1 /* Length Byte */;
  uint8_t* length = prv_length_to_increase(ad_data, size_to_write);
  if (!length) {
    // Not enough space...
    return false;
  }

  // Undo the magic number trick:
  if (*length == BLE_AD_DATA_FINALIZED) {
    *length = 0;
  }

  // Append the element to the end:
  uint8_t * const end = ad_data->data +
                        ad_data->ad_data_length +
                        ad_data->scan_resp_data_length;
  memcpy(end, (const uint8_t *) element, size_to_write);

  // Length book-keeping:
  *length += size_to_write;

  return true;
}

// -----------------------------------------------------------------------------
BLEAdData * ble_ad_create(void) {
  const size_t max_ad_data_size = sizeof(BLEAdData) +
                                      (GAP_LE_AD_REPORT_DATA_MAX_LENGTH * 2);
  BLEAdData *ad_data = applib_malloc(max_ad_data_size);
  if (ad_data) {
    memset(ad_data, 0, sizeof(BLEAdData));
  }
  return ad_data;
}

// -----------------------------------------------------------------------------
void ble_ad_destroy(BLEAdData *ad) {
  applib_free(ad);
}

// -----------------------------------------------------------------------------
//! The smallest UUID width, by reducing the width when a UUID is based on the
//! Bluetooth base UUID. @see bt_uuid_expand_16bit @see bt_uuid_expand_32bit
static uint8_t prv_smallest_bt_uuid_width_in_bytes(const Uuid *uuid) {
  const Uuid bt_uuid_base = bt_uuid_expand_16bit(0);

  // The bytes after the first 4 contain the Bluetooth base.
  // Check if the uuid is based off of the Bluetooth base UUID:
  const bool is_bt_uuid_based = (memcmp(&bt_uuid_base.byte4, &uuid->byte4,
                                    sizeof(Uuid) - offsetof(Uuid, byte4)) == 0);
  if (!is_bt_uuid_based) {
    // Not based on the Bluetooth base UUID, so use 128-bits:
    return sizeof(Uuid);
  }
  if (uuid->byte0 || uuid->byte1) {
    // If byte0 and byte1 not zero: 32-bit UUID, Bluetooth base UUID based:
    return sizeof(uint32_t);
  }
  // If byte0 and byte1 are zero: 16-bit UUID, Bluetooth base UUID based:
  return sizeof(uint16_t);
}

// -----------------------------------------------------------------------------
//! Finds the largest common UUID width. For UUIDs that are based on the
//! Bluetooth base UUID, a reduced width will be taken of either 16-bits or
//! 32-bits.
static uint8_t prv_largest_common_bt_uuid_width(const Uuid uuids[],
                                         uint8_t num_uuids) {
  uint8_t max_width_bytes = sizeof(uint16_t);
  for (unsigned int i = 0; i < num_uuids; ++i) {
    const Uuid *uuid = &uuids[i];
    const uint8_t width_bytes = prv_smallest_bt_uuid_width_in_bytes(uuid);
    max_width_bytes = MAX(width_bytes, max_width_bytes);
  }
  return max_width_bytes;
}

// -----------------------------------------------------------------------------
//! Helper to reduces a 128-bit UUID to 16-bits. Note: this function does not
//! check whether the original UUID is based on the Bluetooth base.
static uint16_t prv_convert_to_16bit_uuid(const Uuid *uuid) {
  uint16_t uuid_16bits = 0;
  // Use bytes 2-3 of the Uuid:
  for (int i = 2; i < 4; ++i) {
    uuid_16bits <<= 8;
    uuid_16bits += ((const uint8_t *) uuid)[i];
  }
  return uuid_16bits;
}

// -----------------------------------------------------------------------------
//! Helper to reduces a 128-bit UUID to 32-bits. Note: this function does not
//! check whether the original UUID is based on the Bluetooth base.
static uint32_t prv_convert_to_32bit_uuid(const Uuid *uuid) {
  uint32_t uuid_32bits = 0;
  // Use bytes 0-3 of the Uuid:
  for (int i = 0; i < 4; ++i) {
    uuid_32bits <<= 8;
    uuid_32bits += ((const uint8_t *) uuid)[i];
  }
  return uuid_32bits;
}

// -----------------------------------------------------------------------------
bool ble_ad_set_service_uuids(BLEAdData *ad,
                              const Uuid uuids[], uint8_t num_uuids) {
  struct __attribute__((__packed__)) BLEAdElementService {
    BLEAdElementHeader header;
    union {
      Uuid uuid_128[0];
      uint32_t uuid_32[0];
      uint16_t uuid_16[0];
    };
  };

  const uint8_t max_width_bytes = prv_largest_common_bt_uuid_width(uuids,
                                                                   num_uuids);
  // Allocate buffer:
  const size_t buffer_size = sizeof(struct BLEAdElementService) +
                                (max_width_bytes * num_uuids);
  uint8_t element_buffer[buffer_size];
  struct BLEAdElementService *element = (struct BLEAdElementService *) element_buffer;

  // Set header fields (assume Complete):
  switch (max_width_bytes) {
    case 16: element->header.type = BLEAdTypeService128BitUUIDComplete; break;
    case 4: element->header.type = BLEAdTypeService32BitUUIDComplete; break;
    case 2: element->header.type = BLEAdTypeService16BitUUIDComplete; break;
    default:
      WTF;
  }

  element->header.length = buffer_size - 1 /* -1 Length byte */;

  // Copy UUIDs:
  for (unsigned int i = 0; i < num_uuids; ++i) {
    switch (max_width_bytes) {
      case 16: element->uuid_128[i] = uuids[i]; break;
      case 4: element->uuid_32[i] = prv_convert_to_32bit_uuid(&uuids[i]); break;
      case 2: element->uuid_16[i] = prv_convert_to_16bit_uuid(&uuids[i]); break;
      default:
        WTF;
    }
  }

  return prv_write_element_to_ad_data(ad, (const BLEAdElement *) element);
}

// -----------------------------------------------------------------------------
bool ble_ad_set_local_name(BLEAdData *ad,
                           const char *local_name) {
  if (!local_name) {
    return false;
  }
  const size_t length = strlen(local_name);
  uint8_t element_buffer[sizeof(BLEAdElement) + length];
  BLEAdElement *element = (BLEAdElement *) &element_buffer;
  element->header.length = length + 1 /* +1 Type byte */;
  element->header.type = BLEAdTypeLocalNameComplete; /* assume Complete */
  // Note: *not* zero terminated by design
  memcpy(element->data, local_name, length);
  return prv_write_element_to_ad_data(ad, element);
}

// -----------------------------------------------------------------------------
bool ble_ad_set_tx_power_level(BLEAdData *ad) {
  uint8_t element_buffer[sizeof(BLEAdElement) + sizeof(int8_t)];
  BLEAdElement *element = (BLEAdElement *) element_buffer;
  element->header.length = sizeof(int8_t) + 1 /* +1 Type byte */;
  element->header.type = BLEAdTypeTxPowerLevel;
  *((int8_t *) element->data) = sys_ble_get_advertising_tx_power();

  return prv_write_element_to_ad_data(ad, element);
}

// -----------------------------------------------------------------------------
bool ble_ad_set_manufacturer_specific_data(BLEAdData *ad, uint16_t company_id,
                                           const uint8_t *data, size_t size) {
  struct __attribute__((__packed__)) BLEAdElementManufacturerSpecific {
    BLEAdElementHeader header;
    uint16_t company_id;
    uint8_t data[];
  };

  uint8_t element_buffer[sizeof(struct BLEAdElementManufacturerSpecific) + size];
  struct BLEAdElementManufacturerSpecific *element =
      (struct BLEAdElementManufacturerSpecific *) element_buffer;
  element->header.length = sizeof(struct BLEAdElementManufacturerSpecific)
                                          - 1 /* -1 Length byte */ + size;
  element->header.type = BLEAdTypeManufacturerSpecific;
  element->company_id = ltohs(company_id);
  memcpy(element->data, data, size);
  return prv_write_element_to_ad_data(ad, (const BLEAdElement *) element);
}

// -----------------------------------------------------------------------------
bool ble_ad_set_flags(BLEAdData *ad, uint8_t flags) {
  struct __attribute__((__packed__)) BLEAdElementManufacturerSpecific {
    BLEAdElementHeader header;
    uint8_t flags;
  } element = {
    .header = {
      .length = sizeof(struct BLEAdElementManufacturerSpecific)
                - 1 /* -1 Length byte */,
      .type = BLEAdTypeFlags,
    },
    .flags = flags,
  };
  return prv_write_element_to_ad_data(ad, (const BLEAdElement *) &element);
}
