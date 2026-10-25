/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Small Zephyr library functions the host needs, and stand-ins for the parts
// of the host left out of this configuration (LE pairing, host crypto).

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/uuid.h>

/* printk */

int vsnprintk(char *str, size_t size, const char *fmt, va_list ap) {
  return vsnprintf(str, size, fmt, ap);
}

int snprintk(char *str, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc = vsnprintf(str, size, fmt, ap);
  va_end(ap);
  return rc;
}

/* CRC-8, as Zephyr's lib/crc/crc8_sw.c */

uint8_t crc8(const uint8_t *src, size_t len, uint8_t polynomial, uint8_t initial_value,
             bool reversed) {
  uint8_t crc = initial_value;

  for (size_t i = 0; i < len; i++) {
    crc ^= src[i];
    for (int j = 0; j < 8; j++) {
      if (reversed) {
        crc = (crc & 0x01) ? (uint8_t)((crc >> 1) ^ polynomial) : (uint8_t)(crc >> 1);
      } else {
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ polynomial) : (uint8_t)(crc << 1);
      }
    }
  }
  return crc;
}

/* CRC-16, reflected, as Zephyr's subsys/crc/crc16_sw.c (L2CAP ERTM's FCS) */

uint16_t crc16_reflect(uint16_t poly, uint16_t seed, const uint8_t *src, size_t len) {
  uint16_t crc = seed;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)src[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc & 0x0001) ? (uint16_t)((crc >> 1) ^ poly) : (uint16_t)(crc >> 1);
    }
  }
  return crc;
}

/* Hex conversion, as Zephyr's lib/utils/hex.c */

static int prv_char2hex(char c, uint8_t *x) {
  if (c >= '0' && c <= '9') {
    *x = c - '0';
  } else if (c >= 'a' && c <= 'f') {
    *x = c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    *x = c - 'A' + 10;
  } else {
    return -EINVAL;
  }
  return 0;
}

size_t hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen) {
  uint8_t dec;

  if (buflen < hexlen / 2 + hexlen % 2) {
    return 0;
  }
  // An odd length means a leading zero nibble.
  if (hexlen % 2) {
    if (prv_char2hex(hex[0], &dec) < 0) {
      return 0;
    }
    buf[0] = dec;
    hex++;
    buf++;
  }
  for (size_t i = 0; i < hexlen / 2; i++) {
    if (prv_char2hex(hex[2 * i], &dec) < 0) {
      return 0;
    }
    buf[i] = dec << 4;
    if (prv_char2hex(hex[2 * i + 1], &dec) < 0) {
      return 0;
    }
    buf[i] += dec;
  }
  return hexlen / 2 + hexlen % 2;
}

/* UUID strings: only reached from the Bluetooth UUID string helpers, which
 * nothing in this port calls.
 */

int uuid_from_string(const char data[UUID_STR_LEN], struct uuid *out) {
  return -ENOTSUP;
}

int uuid_to_string(const struct uuid *data, char out[UUID_STR_LEN]) {
  return -ENOTSUP;
}

/* Stand-ins for the host parts left out (CONFIG_BT_SMP, CONFIG_BT_HOST_CRYPTO) */

int bt_rand(void *buf, size_t len) {
  sys_rand_get(buf, len);
  return 0;
}

bool bt_get_bondable(void) {
  // Without LE pairing this only answers for BR/EDR, where the host bonds.
  return true;
}

bool bt_conn_ltk_present(const struct bt_conn *conn) {
  // No LE keys: NimBLE owns LE.
  return false;
}

const bt_addr_le_t *bt_lookup_id_addr(uint8_t id, const bt_addr_le_t *addr) {
  // No resolving list without LE privacy.
  return addr;
}

int gatt_gap_svc_validate(void) {
  return 0;
}
