/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Phone book access client: reads the phone's contacts, call history and favorites over PBAP
// (OBEX over RFCOMM), as vCard 3.0 with only the names, numbers and call times, and hands the
// entries to the control side to save. One OBEX packet at a time: the next is asked for once
// the control side saved the last one's entries. All on the Zephyr thread but for the request
// in zbt_host_pbap_sync() and zbt_host_pbap_next().

#include "zbt_pbap.h"

#include "zbt_hfp.h"
#include "zbt_power.h"

#include <errno.h>
#include <string.h>

#include <zbt/zbt.h>
#include <zbt_link.h>
#include <zephyr/bluetooth/classic/goep.h>
#include <zephyr/bluetooth/classic/obex.h>
#include <zephyr/bluetooth/classic/pbap.h>
#include <zephyr/bluetooth/classic/sdp.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>

// The phone stops answering: give up after this long. The first packet of a book can take a
// while, the phone building the book first.
#define PBAP_TIMEOUT_MS       15000
#define PBAP_FIRST_TIMEOUT_MS 60000
#define VCARD_LINE_MAX        160
#define CARD_TELS             4

// vCard properties to send (PBAP PropertySelector bits): VERSION, FN, N, TEL, SORT-STRING (the
// name's reading) and the call's X-IRMC-CALL-DATETIME.
#define PROPERTIES \
  ((1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 7) | (1ULL << 27) | (1ULL << 28))
#define FORMAT_VCARD30 0x01

static const char *const BOOK_PATH[ZBT_BOOK_COUNT] = {
  [ZBT_BOOK_CONTACTS] = "telecom/pb.vcf",
  [ZBT_BOOK_HISTORY] = "telecom/cch.vcf",
  [ZBT_BOOK_FAVORITES] = "telecom/fav.vcf",
};

static const uint16_t BOOK_MAX_COUNT[ZBT_BOOK_COUNT] = {
  [ZBT_BOOK_CONTACTS] = 1000,
  [ZBT_BOOK_HISTORY] = 50,
  [ZBT_BOOK_FAVORITES] = 50,
};

struct vcard {
  char fn[ZBT_NAME_MAX];
  char n[ZBT_NAME_MAX];
  char sort[ZBT_SORT_MAX];
  char phonetic_first[ZBT_SORT_MAX];
  char tel[CARD_TELS][ZBT_CLIP_MAX];
  uint8_t tels;
  uint8_t kind;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint16_t year;
  bool open;
};

// What lasts from one reading to the next, and whatever the host may still hold or run after
// a reading ends (the SDP request, the work items).
static struct {
  struct bt_sdp_discover_params sdp;
  struct k_work sync_work;
  struct k_work connect_work;
  struct k_work next_work;
  struct k_work_delayable timeout_work;
  struct k_work_delayable auto_work;
  //! Books zbt_pbap_auto() asked for.
  unsigned auto_books;
  //! Books asked for (zbt_host_pbap_sync(), any task).
  unsigned requested;
  uint8_t channel;
  bool ready;
} s_p;

enum transport {
  TRANSPORT_NONE,
  TRANSPORT_CONNECTING,
  TRANSPORT_UP,
};

// A reading: borrowed from the kernel heap (zbt_ctl_pbap_acquire()) for as long as it and its
// RFCOMM channel last, as the PCE holds the channel.
struct pbap_state {
  struct bt_pbap_pce pce;
  enum transport transport;
  //! The reading ended; the channel is to close, then the state is given back.
  bool finishing;
  uint32_t conn_id;
  bool connected;
  //! Books still to read.
  unsigned left;
  enum zbt_book book;
  //! The phone has more of the book (CONTINUE).
  bool more;
  //! The control side lost entries, or the phone failed: the book is not kept.
  bool book_ok;
  uint16_t cards;
  // vCard parsing, across packets.
  char line[VCARD_LINE_MAX];
  uint16_t line_len;
  bool line_start;
  struct vcard card;
};

BUILD_ASSERT(sizeof(struct pbap_state) <= ZBT_PBAP_MEM_SIZE, "PBAP state too big");

// The reading going on, or NULL.
static struct pbap_state *s;

static void prv_set_state(enum zbt_pbap state, int err) {
  struct zbt_status *st = zbt_ctl_status();
  st->pbap = state;
  if (err != 0) {
    st->pbap_err = err;
  }
}

/* vCard parsing ------------------------------------------------------------------------------*/

// Copies a vCard 3.0 value, unescaped, cut at a ';' when @p field is set.
static void prv_copy_value(char *out, size_t size, const char *value, bool field) {
  size_t o = 0;
  for (const char *p = value; *p != '\0' && o + 1 < size; p++) {
    if (*p == '\\' && p[1] != '\0') {
      p++;
      out[o++] = (*p == 'n' || *p == 'N') ? ' ' : *p;
    } else if (field && *p == ';') {
      break;
    } else {
      out[o++] = *p;
    }
  }
  // Do not leave a UTF-8 sequence cut short (a whole last character stays).
  size_t lead = o;
  while (lead > 0 && ((uint8_t)out[lead - 1] & 0xc0) == 0x80) {
    lead--;
  }
  if (lead > 0) {
    const uint8_t c = (uint8_t)out[lead - 1];
    const size_t want = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
    if (o - (lead - 1) < want) {
      o = lead - 1;
    }
  }
  out[o] = '\0';
}

// The N property ("Family;Given;..."), as "Family Given".
static void prv_name_from_n(char *out, size_t size, const char *value) {
  char family[ZBT_NAME_MAX];
  prv_copy_value(family, sizeof(family), value, true);
  const char *given = strchr(value, ';');
  char first[ZBT_NAME_MAX] = "";
  if (given != NULL) {
    prv_copy_value(first, sizeof(first), given + 1, true);
  }
  size_t len = strlen(family);
  memcpy(out, family, len + 1 < size ? len + 1 : size);
  out[size - 1] = '\0';
  if (first[0] != '\0') {
    len = strlen(out);
    if (len > 0 && len + 1 < size) {
      out[len++] = ' ';
      out[len] = '\0';
    }
    prv_copy_value(out + len, size - len, first, false);
  }
}

static void prv_copy_number(char *out, size_t size, const char *value) {
  size_t o = 0;
  for (const char *p = value; *p != '\0' && o + 1 < size; p++) {
    if ((*p >= '0' && *p <= '9') || *p == '+' || *p == '*' || *p == '#') {
      out[o++] = *p;
    }
  }
  out[o] = '\0';
}

static int prv_digits(const char *p, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (p[i] < '0' || p[i] > '9') {
      return -1;
    }
    v = v * 10 + (p[i] - '0');
  }
  return v;
}

// X-IRMC-CALL-DATETIME;RECEIVED:20261020T213000
static void prv_call_datetime(struct vcard *card, const char *params, const char *value) {
  if (strstr(params, "DIALED") != NULL) {
    card->kind = ZBT_CALL_KIND_OUTGOING;
  } else if (strstr(params, "MISSED") != NULL) {
    card->kind = ZBT_CALL_KIND_MISSED;
  } else {
    card->kind = ZBT_CALL_KIND_INCOMING;
  }
  if (strlen(value) < 13 || value[8] != 'T') {
    return;
  }
  const int year = prv_digits(value, 4);
  const int month = prv_digits(value + 4, 2);
  const int day = prv_digits(value + 6, 2);
  const int hour = prv_digits(value + 9, 2);
  const int minute = prv_digits(value + 11, 2);
  if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || minute < 0) {
    return;
  }
  card->year = (uint16_t)year;
  card->month = (uint8_t)month;
  card->day = (uint8_t)day;
  card->hour = (uint8_t)hour;
  card->minute = (uint8_t)minute;
}

// The code point of the UTF-8 sequence at @p s.
static uint32_t prv_first_code_point(const char *s) {
  const uint8_t *p = (const uint8_t *)s;
  if (p[0] < 0x80) {
    return p[0];
  }
  if ((p[0] & 0xe0) == 0xc0 && p[1] != 0) {
    return ((p[0] & 0x1fu) << 6) | (p[1] & 0x3fu);
  }
  if ((p[0] & 0xf0) == 0xe0 && p[1] != 0 && p[2] != 0) {
    return ((p[0] & 0x0fu) << 12) | ((p[1] & 0x3fu) << 6) | (p[2] & 0x3fu);
  }
  return 0;
}

static uint8_t prv_group(const char *key) {
  // Where each kana row starts, in hiragana.
  static const uint16_t ROWS[] = {0x3041, 0x304b, 0x3055, 0x305f, 0x306a, 0x306f,
                                  0x307e, 0x3083, 0x3089, 0x308e, 0x3094};
  uint32_t c = prv_first_code_point(key);
  if (c >= 0xff21 && c <= 0xff5a) {
    // Full-width Latin.
    c = c - 0xff21 + 'A';
  }
  if (c >= 'a' && c <= 'z') {
    c -= 'a' - 'A';
  }
  if (c >= 'A' && c <= 'Z') {
    return (uint8_t)(ZBT_GROUP_A + (c - 'A'));
  }
  if (c >= 0x30a1 && c <= 0x30f6) {
    // Katakana as hiragana.
    c -= 0x60;
  }
  for (int row = 0; row < 10; row++) {
    if (c >= ROWS[row] && c < ROWS[row + 1]) {
      return (uint8_t)(ZBT_GROUP_KANA_A + row);
    }
  }
  return ZBT_GROUP_OTHER;
}

static void prv_emit(void) {
  struct vcard *card = &s->card;
  const uint16_t index = s->cards++;
  // The phone book's first card (0.vcf) is the phone's owner.
  if (s->book == ZBT_BOOK_CONTACTS && index == 0) {
    return;
  }
  struct zbt_book_entry entry = {0};
  memcpy(entry.name, card->fn[0] != '\0' ? card->fn : card->n, sizeof(entry.name));
  if (card->sort[0] == '\0' && card->phonetic_first[0] != '\0') {
    memcpy(card->sort, card->phonetic_first, sizeof(card->sort));
  }
  memcpy(entry.sort, card->sort, sizeof(entry.sort));
  entry.group = prv_group(entry.sort[0] != '\0' ? entry.sort : entry.name);
  if (s->book == ZBT_BOOK_HISTORY) {
    memcpy(entry.number, card->tel[0], sizeof(entry.number));
    entry.kind = card->kind;
    entry.year = card->year;
    entry.month = card->month;
    entry.day = card->day;
    entry.hour = card->hour;
    entry.minute = card->minute;
    if (!zbt_ctl_book_entry(&entry)) {
      s->book_ok = false;
    }
    return;
  }
  for (int i = 0; i < card->tels; i++) {
    memcpy(entry.number, card->tel[i], sizeof(entry.number));
    if (!zbt_ctl_book_entry(&entry)) {
      s->book_ok = false;
    }
  }
}

static void prv_line(char *line) {
  char *colon = strchr(line, ':');
  if (colon == NULL) {
    return;
  }
  *colon = '\0';
  const char *value = colon + 1;
  // "item1.TEL;TYPE=CELL": the group goes, the parameters stay apart.
  char *name = line;
  char *params = strchr(name, ';');
  if (params != NULL) {
    *params++ = '\0';
  } else {
    params = colon;
  }
  char *dot = strchr(name, '.');
  if (dot != NULL) {
    name = dot + 1;
  }
  for (char *p = name; *p != '\0'; p++) {
    if (*p >= 'a' && *p <= 'z') {
      *p = (char)(*p - 'a' + 'A');
    }
  }
  for (char *p = params; *p != '\0'; p++) {
    if (*p >= 'a' && *p <= 'z') {
      *p = (char)(*p - 'a' + 'A');
    }
  }

  struct vcard *card = &s->card;
  if (strcmp(name, "BEGIN") == 0) {
    memset(card, 0, sizeof(*card));
    card->open = true;
  } else if (!card->open) {
    return;
  } else if (strcmp(name, "END") == 0) {
    prv_emit();
    card->open = false;
  } else if (strcmp(name, "FN") == 0) {
    prv_copy_value(card->fn, sizeof(card->fn), value, false);
  } else if (strcmp(name, "N") == 0) {
    prv_name_from_n(card->n, sizeof(card->n), value);
  } else if (strcmp(name, "TEL") == 0) {
    if (card->tels < CARD_TELS) {
      prv_copy_number(card->tel[card->tels], sizeof(card->tel[0]), value);
      if (card->tel[card->tels][0] != '\0') {
        card->tels++;
      }
    }
  } else if (strcmp(name, "SORT-STRING") == 0) {
    prv_copy_value(card->sort, sizeof(card->sort), value, false);
  } else if (strcmp(name, "X-PHONETIC-LAST-NAME") == 0 && card->sort[0] == '\0') {
    prv_copy_value(card->sort, sizeof(card->sort), value, false);
  } else if (strcmp(name, "X-PHONETIC-FIRST-NAME") == 0) {
    prv_copy_value(card->phonetic_first, sizeof(card->phonetic_first), value, false);
  } else if (strcmp(name, "X-IRMC-CALL-DATETIME") == 0) {
    prv_call_datetime(card, params, value);
  }
}

static void prv_flush_line(void) {
  if (s->line_len > 0) {
    s->line[s->line_len] = '\0';
    prv_line(s->line);
  }
  s->line_len = 0;
}

// Lines end in CRLF; one that starts with a space or a tab continues the last (folding).
static void prv_parse(const uint8_t *data, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    const char c = (char)data[i];
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      s->line_start = true;
      continue;
    }
    if (s->line_start) {
      s->line_start = false;
      if (c == ' ' || c == '\t') {
        continue;
      }
      prv_flush_line();
    }
    if (s->line_len < VCARD_LINE_MAX - 1) {
      s->line[s->line_len++] = c;
    }
  }
}

/* PBAP ---------------------------------------------------------------------------------------*/

// Gives the reading's state back, once nothing of the host refers to it.
static void prv_end(void) {
  s = NULL;
  zbt_ctl_pbap_release();
}

static void prv_finish(int err) {
  (void)k_work_cancel_delayable(&s_p.timeout_work);
  prv_set_state(err != 0 ? ZBT_PBAP_FAILED : ZBT_PBAP_DONE, err);
  s->left = 0;
  s->more = false;
  s->finishing = true;
  switch (s->transport) {
    case TRANSPORT_UP:
      // The channel goes when the phone answers the disconnect, or with the link.
      if (!s->connected || bt_pbap_pce_disconnect(&s->pce, NULL) != 0) {
        (void)bt_pbap_pce_rfcomm_disconnect(&s->pce);
      }
      s->connected = false;
      break;
    case TRANSPORT_CONNECTING:
      // Closed once it connects, or given back once it fails.
      break;
    case TRANSPORT_NONE:
    default:
      prv_end();
      break;
  }
}

static int prv_pull(bool first) {
  struct net_buf *buf = bt_pbap_pce_create_pdu(&s->pce, NULL);
  if (buf == NULL) {
    return -ENOBUFS;
  }
  int err = 0;
  if (first) {
    const char *path = BOOK_PATH[s->book];
    uint8_t name[2 * 20];
    const size_t len = strlen(path);
    for (size_t i = 0; i <= len; i++) {
      name[2 * i] = 0;
      name[2 * i + 1] = (uint8_t)path[i];
    }
    static const uint8_t format = FORMAT_VCARD30;
    const uint8_t properties[8] = {
      (uint8_t)(PROPERTIES >> 56), (uint8_t)(PROPERTIES >> 48), (uint8_t)(PROPERTIES >> 40),
      (uint8_t)(PROPERTIES >> 32), (uint8_t)(PROPERTIES >> 24), (uint8_t)(PROPERTIES >> 16),
      (uint8_t)(PROPERTIES >> 8),  (uint8_t)PROPERTIES,
    };
    const uint8_t max_count[2] = {
      (uint8_t)(BOOK_MAX_COUNT[s->book] >> 8), (uint8_t)BOOK_MAX_COUNT[s->book]
    };
    const struct bt_obex_tlv params[] = {
      {BT_PBAP_APPL_PARAM_TAG_ID_PROPERTY_SELECTOR, sizeof(properties), properties},
      {BT_PBAP_APPL_PARAM_TAG_ID_FORMAT, sizeof(format), &format},
      {BT_PBAP_APPL_PARAM_TAG_ID_MAX_LIST_COUNT, sizeof(max_count), max_count},
    };
    err = bt_obex_add_header_conn_id(buf, s->conn_id);
    if (err == 0) {
      err = bt_obex_add_header_name(buf, (uint16_t)(2 * (len + 1)), name);
    }
    if (err == 0) {
      err = bt_obex_add_header_type(buf, sizeof(BT_PBAP_PULL_PHONE_BOOK_TYPE),
                                    (const uint8_t *)BT_PBAP_PULL_PHONE_BOOK_TYPE);
    }
    if (err == 0) {
      err = bt_obex_add_header_app_param(buf, ARRAY_SIZE(params), params);
    }
  }
  if (err == 0) {
    err = bt_pbap_pce_pull_phone_book(&s->pce, buf);
  }
  if (err != 0) {
    net_buf_unref(buf);
    return err;
  }
  (void)k_work_reschedule(&s_p.timeout_work,
                          K_MSEC(first ? PBAP_FIRST_TIMEOUT_MS : PBAP_TIMEOUT_MS));
  return 0;
}

// How the book being read ended, before it is handed over.
static void prv_book_result(int result) {
  zbt_ctl_status()->pbap_result[s->book] = (int16_t)result;
}

// The next book asked for, or the end.
static void prv_next_book(void) {
  while (s->left != 0) {
    const enum zbt_book book = (enum zbt_book)__builtin_ctz(s->left);
    s->left &= ~(1U << book);
    s->book = book;
    s->cards = 0;
    s->book_ok = true;
    s->line_len = 0;
    s->line_start = false;
    s->card.open = false;
    zbt_ctl_status()->pbap_book = book;
    zbt_ctl_book_start(book);
    const int err = prv_pull(true);
    if (err == 0) {
      prv_set_state(ZBT_PBAP_PULLING, 0);
      return;
    }
    zbt_ctl_status()->pbap_err = err;
    prv_book_result(err);
  }
  prv_finish(0);
}

static void prv_pull_cb(struct bt_pbap_pce *pce, uint8_t rsp_code, struct net_buf *buf) {
  if (s->finishing) {
    return;
  }
  (void)k_work_cancel_delayable(&s_p.timeout_work);
  if (rsp_code != BT_PBAP_RSP_CODE_CONTINUE && rsp_code != BT_PBAP_RSP_CODE_SUCCESS) {
    // This book is missing or refused (favorites need PBAP 1.2): keep the old one, go on.
    zbt_ctl_status()->pbap_err = rsp_code;
    s->more = false;
    prv_book_result(rsp_code);
    zbt_ctl_book_hand_over(true, false);
    return;
  }
  uint16_t len = 0;
  const uint8_t *body = NULL;
  if (buf != NULL && bt_obex_get_header_body(buf, &len, &body) != 0 &&
      bt_obex_get_header_end_body(buf, &len, &body) != 0) {
    len = 0;
  }
  if (len > 0 && body != NULL) {
    prv_parse(body, len);
  }
  s->more = rsp_code == BT_PBAP_RSP_CODE_CONTINUE;
  if (!s->more) {
    prv_flush_line();
    prv_book_result(s->book_ok ? 0 : -ENOBUFS);
  }
  zbt_ctl_book_hand_over(!s->more, s->book_ok);
}

static void prv_next_work(struct k_work *work) {
  if (s == NULL || s->finishing) {
    return;
  }
  if (!s->more) {
    prv_next_book();
    return;
  }
  const int err = prv_pull(false);
  if (err != 0) {
    s->more = false;
    prv_book_result(err);
    zbt_ctl_book_hand_over(true, false);
  }
}

static void prv_connect_cb(struct bt_pbap_pce *pce, uint8_t rsp_code, uint8_t version,
                           uint16_t mopl, struct net_buf *buf) {
  if (s->finishing) {
    return;
  }
  if (rsp_code != BT_PBAP_RSP_CODE_SUCCESS) {
    // The phone refused: on Android, until the watch is allowed its contacts.
    prv_finish(rsp_code);
    return;
  }
  s->connected = true;
  s->conn_id = 0;
  if (buf != NULL) {
    (void)bt_obex_get_header_conn_id(buf, &s->conn_id);
  }
  prv_next_book();
}

static void prv_disconnect_cb(struct bt_pbap_pce *pce, uint8_t rsp_code, struct net_buf *buf) {
  (void)bt_pbap_pce_rfcomm_disconnect(&s->pce);
}

static void prv_rfcomm_connected(struct bt_conn *conn, struct bt_pbap_pce *pce) {
  s->transport = TRANSPORT_UP;
  if (s->finishing) {
    (void)bt_pbap_pce_rfcomm_disconnect(&s->pce);
    return;
  }
  const int err = bt_pbap_pce_connect(&s->pce, UINT16_MAX, NULL);
  if (err != 0) {
    prv_finish(err);
  }
}

static void prv_rfcomm_disconnected(struct bt_pbap_pce *pce) {
  s->transport = TRANSPORT_NONE;
  s->connected = false;
  if (!s->finishing) {
    // The phone or the link closed it mid-reading.
    if (zbt_ctl_status()->pbap == ZBT_PBAP_PULLING) {
      s->more = false;
      prv_book_result(-ECONNRESET);
      zbt_ctl_book_hand_over(true, false);
    }
    (void)k_work_cancel_delayable(&s_p.timeout_work);
    prv_set_state(ZBT_PBAP_FAILED, -ECONNRESET);
  }
  prv_end();
}

static struct bt_pbap_pce_cb s_pce_cb = {
  .rfcomm_connected = prv_rfcomm_connected,
  .rfcomm_disconnected = prv_rfcomm_disconnected,
  .connect = prv_connect_cb,
  .disconnect = prv_disconnect_cb,
  .pull_phone_book = prv_pull_cb,
};

static void prv_timeout_work(struct k_work *work) {
  if (s == NULL || s->finishing) {
    return;
  }
  if (s->more || zbt_ctl_status()->pbap == ZBT_PBAP_PULLING) {
    s->more = false;
    prv_book_result(-ETIMEDOUT);
    zbt_ctl_book_hand_over(true, false);
  }
  prv_finish(-ETIMEDOUT);
}

static void prv_connect_work(struct k_work *work) {
  struct bt_conn *acl = zbt_hfp_acl();
  if (s == NULL) {
    return;
  }
  if (acl == NULL || s_p.channel == 0) {
    prv_finish(-ENOTCONN);
    return;
  }
  prv_set_state(ZBT_PBAP_CONNECTING, 0);
  const int err = bt_pbap_pce_rfcomm_connect(acl, &s->pce, &s_pce_cb, s_p.channel);
  if (err != 0) {
    prv_finish(err);
    return;
  }
  s->transport = TRANSPORT_CONNECTING;
  (void)k_work_reschedule(&s_p.timeout_work, K_MSEC(PBAP_TIMEOUT_MS));
}

static uint8_t prv_sdp_cb(struct bt_conn *conn, struct bt_sdp_client_result *result,
                          const struct bt_sdp_discover_params *params) {
  uint16_t channel = 0;
  if (result == NULL || result->resp_buf == NULL ||
      bt_sdp_get_proto_param(result->resp_buf, BT_SDP_PROTO_RFCOMM, &channel) != 0 ||
      channel == 0) {
    if (s != NULL) {
      prv_finish(-ENOENT);
    }
    return BT_SDP_DISCOVER_UUID_STOP;
  }
  s_p.channel = (uint8_t)channel;
  k_work_submit(&s_p.connect_work);
  return BT_SDP_DISCOVER_UUID_STOP;
}

static void prv_sync_work(struct k_work *work) {
  struct bt_conn *acl = zbt_hfp_acl();
  const unsigned books = s_p.requested;
  s_p.requested = 0;
  if (acl == NULL) {
    prv_set_state(ZBT_PBAP_FAILED, -ENOTCONN);
    return;
  }
  if (s != NULL) {
    // The last reading's channel has not closed yet.
    prv_set_state(ZBT_PBAP_FAILED, -EBUSY);
    return;
  }
  s = zbt_ctl_pbap_acquire();
  if (s == NULL) {
    prv_set_state(ZBT_PBAP_FAILED, -ENOMEM);
    return;
  }
  memset(s, 0, sizeof(*s));
  s->left = books;
  s->more = false;
  zbt_ctl_status()->pbap_err = 0;
  prv_set_state(ZBT_PBAP_SEARCHING, 0);
  // Out of sniff: a packet each sniff interval would take minutes.
  zbt_power_busy();

  static struct bt_uuid_16 uuid = BT_UUID_INIT_16(BT_SDP_PBAP_PSE_SVCLASS);
  static struct bt_sdp_attribute_id_range ranges[] = {
    {BT_SDP_ATTR_PROTO_DESC_LIST, BT_SDP_ATTR_PROTO_DESC_LIST},
  };
  static struct bt_sdp_attribute_id_list ids = {
    .count = ARRAY_SIZE(ranges),
    .ranges = ranges,
  };
  s_p.sdp = (struct bt_sdp_discover_params){
    .uuid = &uuid.uuid,
    .func = prv_sdp_cb,
    .type = BT_SDP_DISCOVER_SERVICE_SEARCH_ATTR,
    .pool = zbt_hfp_sdp_pool(),
    .ids = &ids,
  };
  const int err = bt_sdp_discover(acl, &s_p.sdp);
  if (err != 0) {
    prv_finish(err);
  }
}

int zbt_host_pbap_sync(unsigned books) {
  if (!s_p.ready) {
    return -EAGAIN;
  }
  const enum zbt_pbap state = zbt_ctl_status()->pbap;
  if (state == ZBT_PBAP_SEARCHING || state == ZBT_PBAP_CONNECTING || state == ZBT_PBAP_PULLING ||
      k_work_busy_get(&s_p.sync_work) != 0) {
    return -EBUSY;
  }
  if (books == 0) {
    return 0;
  }
  s_p.requested = books;
  k_work_submit(&s_p.sync_work);
  return 0;
}

void zbt_host_pbap_next(void) {
  if (s_p.ready) {
    k_work_submit(&s_p.next_work);
  }
}

static void prv_auto_work(struct k_work *work) {
  const unsigned books = s_p.auto_books;
  if (zbt_hfp_acl() == NULL) {
    s_p.auto_books = 0;
    return;
  }
  if (zbt_host_pbap_sync(books) == -EBUSY) {
    (void)k_work_reschedule(&s_p.auto_work, K_SECONDS(5));
    return;
  }
  s_p.auto_books = 0;
}

void zbt_pbap_auto(unsigned books, uint32_t delay_ms) {
  if (!s_p.ready) {
    return;
  }
  s_p.auto_books |= books;
  (void)k_work_reschedule(&s_p.auto_work, K_MSEC(delay_ms));
}

// The watch's PCE record (PBAP 1.2), as Zephyr's shell has it: Android offers contact
// sharing, and lets a device read its phone book, only for one that has it.
static struct bt_sdp_attribute s_pce_attrs[] = {
  BT_SDP_NEW_SERVICE,
  BT_SDP_LIST(BT_SDP_ATTR_SVCLASS_ID_LIST, BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
              BT_SDP_DATA_ELEM_LIST(
                  {BT_SDP_TYPE_SIZE(BT_SDP_UUID16), BT_SDP_ARRAY_16(BT_SDP_PBAP_PCE_SVCLASS)}, )),
  BT_SDP_LIST(BT_SDP_ATTR_PROFILE_DESC_LIST, BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
              BT_SDP_DATA_ELEM_LIST(
                  {BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
                   BT_SDP_DATA_ELEM_LIST(
                       {BT_SDP_TYPE_SIZE(BT_SDP_UUID16), BT_SDP_ARRAY_16(BT_SDP_PBAP_SVCLASS)},
                       {BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16(0x0102)}, )}, )),
  BT_SDP_SERVICE_NAME("Phonebook Access PCE"),
};

static struct bt_sdp_record s_pce_rec = BT_SDP_RECORD(s_pce_attrs);

void zbt_pbap_init(void) {
  s = NULL;
  memset(&s_p, 0, sizeof(s_p));
  s_p.ready = true;
  k_work_init(&s_p.sync_work, prv_sync_work);
  k_work_init(&s_p.connect_work, prv_connect_work);
  k_work_init(&s_p.next_work, prv_next_work);
  k_work_init_delayable(&s_p.timeout_work, prv_timeout_work);
  k_work_init_delayable(&s_p.auto_work, prv_auto_work);
  struct zbt_status *st = zbt_ctl_status();
  st->pbap = ZBT_PBAP_IDLE;
  st->pbap_err = 0;
  const int err = bt_sdp_register_service(&s_pce_rec);
  if (err != 0) {
    st->pbap_err = err;
  }
}
