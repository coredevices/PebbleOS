/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Hands-free (HFP HF) on the Zephyr host: once the link to the phone is
// encrypted, look up the phone's hands-free gateway (SDP) and set the service
// level connection up; then follow the phone's indicators and its call, and
// answer or end the call. Zephyr's HF registers its SDP record and accepts the
// phone's SCO link itself; the voice does not go through the host.
//
// CVSD only: no codec negotiation (CONFIG_BT_HFP_HF_CODEC_NEG off), so the
// phone stays with CVSD.

#include "zbt_hfp.h"

#include "zbt_pbap.h"
#include "zbt_power.h"

#include <errno.h>
#include <string.h>

#include <zbt/zbt.h>
#include <zbt_link.h>
#include <zephyr/bluetooth/classic/hfp_hf.h>
#include <zephyr/bluetooth/classic/sdp.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>

// For the gateway's features, which the HF API does not hand out: hfp_hf.c's own includes,
// in its order.
#include <zephyr/bluetooth/classic/rfcomm.h>
// clang-format off
#include <host/hci_core.h>
#include <host/conn_internal.h>
#include "classic/l2cap_br_internal.h"
#include "classic/rfcomm_internal.h"
#include "classic/at.h"
#include "classic/sco_internal.h"
#include "classic/hfp_hf_internal.h"
// clang-format on

// The protocol descriptor list of the phone's gateway record (about 30 bytes).
#define SDP_BUF_SIZE 128

NET_BUF_POOL_FIXED_DEFINE(s_sdp_pool, 1, SDP_BUF_SIZE, CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static struct bt_conn *s_acl;
static struct bt_hfp_hf *s_hf;
static struct bt_hfp_hf_call *s_call;
static struct bt_sdp_discover_params s_sdp;
static struct k_work s_slc_work;
static struct k_work s_answer_work;
static struct k_work s_hangup_work;
static struct k_work s_audio_phone_work;
static struct k_work s_audio_watch_work;
static struct k_work s_sco_drop_work;
// The dial, sent after a delay the caller gives (zbt_hfp_dial_after()), which it can cancel.
static struct k_work_delayable s_dial_work;
static struct k_work s_redial_work;
static struct k_work s_bia_work;
// The phone's own number (AT+CNUM), which Android lists for its virtual call.
// The calls the phone lists for the call (AT+CLCC) were looked at once.
static bool s_call_listed;
static struct k_work_delayable s_own_number_work;
static uint8_t s_own_number_tries;
#define OWN_NUMBER_MS    1000
#define OWN_NUMBER_TRIES 5
// A call the phone set up: asked about (AT+CLCC) PHONE_CALL_MS after, and on again while busy,
// up to PHONE_CALL_TRIES times. One with a number is dialed on the phone, and talked there;
// one without (Android's virtual call, the phone's audio for something else, as a recorded
// call played back) is left alone: dropping its voice link would silence it.
#define PHONE_CALL_MS    300
#define PHONE_CALL_RETRY 500
#define PHONE_CALL_TRIES 5
static struct k_work_delayable s_phone_call_work;
static uint8_t s_phone_call_tries;
// During a call, the phone's list of calls every CALL_CHECK_MS, and CALL_CHECK_SCO_MS after
// its voice link went down: a call the phone no longer lists ends here (Zephyr does so on the
// list), where the phone did not say so (Discord's calls, through Android's virtual call).
#define CALL_CHECK_MS     10000
#define CALL_CHECK_SCO_MS 1000
static struct k_work_delayable s_call_check_work;
// zbt_hfp_audio_on_watch() was asked during this call: no deciding for it. Zephyr thread.
static bool s_audio_chosen;
// The watch answered this call (zbt_hfp_answer()). One answered otherwise was on the phone:
// talked there, as Android takes its audio to the hands-free device all the same. Zephyr thread.
static bool s_answered_here;
// The number being dialed, and the next outgoing call takes it. Zephyr thread, but for the
// copy into s_dial_number before s_dial_work is queued.
static char s_dial_number[ZBT_CLIP_MAX];
static bool s_dial_pending;
// The watch dialed (or redialed): the next outgoing call is the watch's (dialed_here). Zephyr
// thread.
static bool s_watch_dialing;
// The call in progress, for its log record. Zephyr thread only.
static struct {
  bool outgoing;
  bool answered;
  int64_t answered_ms;
  char number[ZBT_CLIP_MAX];
} s_rec;
// The call's audio stays on the phone: drop its SCO links. Zephyr thread only.
static bool s_audio_on_phone;

static void prv_set_audio_on_phone(bool on_phone) {
  s_audio_on_phone = on_phone;
  zbt_ctl_status()->audio_on_phone = on_phone;
}

/* SLC set-up */

static uint8_t prv_sdp_cb(struct bt_conn *conn, struct bt_sdp_client_result *result,
                          const struct bt_sdp_discover_params *params) {
  struct zbt_status *st = zbt_ctl_status();
  uint16_t channel = 0;
  if (result == NULL || result->resp_buf == NULL ||
      bt_sdp_get_proto_param(result->resp_buf, BT_SDP_PROTO_RFCOMM, &channel) != 0 ||
      channel == 0) {
    st->hfp = ZBT_HFP_DISCONNECTED;
    st->hfp_err = -ENOENT;
    return BT_SDP_DISCOVER_UUID_STOP;
  }
  st->hfp_channel = (uint8_t)channel;
  k_work_submit(&s_slc_work);
  return BT_SDP_DISCOVER_UUID_STOP;
}

static void prv_slc_work(struct k_work *work) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_acl == NULL || s_hf != NULL) {
    // Gone, or the phone set the SLC up itself.
    return;
  }
  struct bt_hfp_hf *hf;
  st->hfp = ZBT_HFP_CONNECTING;
  st->hfp_err = bt_hfp_hf_connect(s_acl, &hf, st->hfp_channel);
  if (st->hfp_err != 0) {
    st->hfp = ZBT_HFP_DISCONNECTED;
  }
}

void zbt_hfp_link_encrypted(struct bt_conn *conn) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_acl != NULL) {
    return;
  }
  s_acl = bt_conn_ref(conn);
  if (s_hf != NULL) {
    return;
  }
  static struct bt_uuid_16 uuid = BT_UUID_INIT_16(BT_SDP_HANDSFREE_AGW_SVCLASS);
  static struct bt_sdp_attribute_id_range ranges[] = {
    {BT_SDP_ATTR_PROTO_DESC_LIST, BT_SDP_ATTR_PROTO_DESC_LIST},
  };
  static struct bt_sdp_attribute_id_list ids = {
    .count = ARRAY_SIZE(ranges),
    .ranges = ranges,
  };
  s_sdp = (struct bt_sdp_discover_params){
    .uuid = &uuid.uuid,
    .func = prv_sdp_cb,
    .type = BT_SDP_DISCOVER_SERVICE_SEARCH_ATTR,
    .pool = &s_sdp_pool,
    .ids = &ids,
  };
  st->hfp = ZBT_HFP_SDP;
  st->hfp_err = bt_sdp_discover(conn, &s_sdp);
  if (st->hfp_err != 0) {
    st->hfp = ZBT_HFP_DISCONNECTED;
  }
}

struct bt_conn *zbt_hfp_acl(void) {
  return s_acl;
}

struct net_buf_pool *zbt_hfp_sdp_pool(void) {
  return &s_sdp_pool;
}

void zbt_hfp_link_down(struct bt_conn *conn) {
  if (conn == s_acl) {
    bt_conn_unref(s_acl);
    s_acl = NULL;
  }
}

/* HF callbacks */

static void prv_connected(struct bt_conn *conn, struct bt_hfp_hf *hf) {
  struct zbt_status *st = zbt_ctl_status();
  s_hf = hf;
  st->hfp = ZBT_HFP_CONNECTED;
  st->hfp_err = 0;
  st->ag_features = hf->ag_features;
  // Once the phone has settled the hands-free link.
  zbt_pbap_auto(ZBT_BOOK_ALL, 3000);
  k_work_submit(&s_bia_work);
  zbt_ctl_status()->own_number[0] = '\0';
  s_own_number_tries = 0;
  (void)k_work_reschedule(&s_own_number_work, K_MSEC(OWN_NUMBER_MS));
}

// Only the indicators a call needs: the signal strength, roaming and the phone's battery
// would wake the link (and the watch) at each change.
static void prv_bia_work(struct k_work *work) {
  if (s_hf != NULL) {
    (void)bt_hfp_hf_indicator_status(s_hf, BIT(HF_SERVICE_IND) | BIT(HF_CALL_IND) |
                                               BIT(HF_CALL_SETUP_IND) | BIT(HF_CALL_HELD_IND));
  }
}

// Asked apart from the other commands: the hands-free link has few command buffers.
static void prv_own_number_work(struct k_work *work) {
  if (s_hf != NULL && bt_hfp_hf_query_subscriber(s_hf) != 0 &&
      ++s_own_number_tries < OWN_NUMBER_TRIES) {
    (void)k_work_reschedule(&s_own_number_work, K_MSEC(OWN_NUMBER_MS));
  }
}

static void prv_subscriber_number(struct bt_hfp_hf *hf, const char *number, uint8_t type,
                                  uint8_t service) {
  if (number != NULL && service != 5) {
    char *own = zbt_ctl_status()->own_number;
    strncpy(own, number, ZBT_CLIP_MAX - 1);
    own[ZBT_CLIP_MAX - 1] = '\0';
  }
}

static void prv_disconnected(struct bt_hfp_hf *hf) {
  struct zbt_status *st = zbt_ctl_status();
  s_hf = NULL;
  s_call = NULL;
  prv_set_audio_on_phone(false);
  s_watch_dialing = false;
  st->dialed_here = false;
  st->call_virtual = false;
  st->hfp = ZBT_HFP_DISCONNECTED;
  if (st->call != ZBT_CALL_NONE) {
    st->call = ZBT_CALL_ENDED;
    zbt_ctl_call_changed();
  }
}

static void prv_sco_connected(struct bt_hfp_hf *hf, struct bt_conn *sco_conn) {
  struct zbt_status *st = zbt_ctl_status();
  zbt_trace('S');
  st->sco = true;
  st->sco_count++;
  if (s_audio_on_phone) {
    k_work_submit(&s_sco_drop_work);
    return;
  }
  zbt_ctl_sco(true);
}

static void prv_sco_disconnected(struct bt_conn *sco_conn, uint8_t reason) {
  struct zbt_status *st = zbt_ctl_status();
  zbt_trace('s');
  st->sco = false;
  if (s_call != NULL) {
    (void)k_work_reschedule(&s_call_check_work, K_MSEC(CALL_CHECK_SCO_MS));
  }
  st->sco_reason = reason;
  zbt_ctl_sco(false);
  zbt_power_busy();
}

static void prv_log_call(void) {
  const struct zbt_status *st = zbt_ctl_status();
  struct zbt_call_record rec = {
    .kind = s_rec.outgoing   ? ZBT_CALL_KIND_OUTGOING
            : s_rec.answered ? ZBT_CALL_KIND_INCOMING
                             : ZBT_CALL_KIND_MISSED,
  };
  strncpy(rec.number, s_rec.outgoing ? s_rec.number : st->clip, sizeof(rec.number) - 1);
  if (s_rec.answered) {
    const int64_t s = (k_uptime_get() - s_rec.answered_ms) / 1000;
    rec.duration_s = s > UINT16_MAX ? UINT16_MAX : (uint16_t)s;
  }
  zbt_ctl_call_ended(&rec);
  // The phone's history has the call by then.
  zbt_pbap_auto(1U << ZBT_BOOK_HISTORY, 3000);
}

static void prv_set_call(struct bt_hfp_hf_call *call, enum zbt_call state) {
  struct zbt_status *st = zbt_ctl_status();
  zbt_trace("-IOACHE"[state]);
  st->call = state;
  zbt_ctl_call_changed();
  if (state == ZBT_CALL_ENDED) {
    if (call == s_call) {
      (void)k_work_cancel_delayable(&s_call_check_work);
      prv_log_call();
      s_call = NULL;
      prv_set_audio_on_phone(false);
      st->dialed_here = false;
      st->call_virtual = false;
    }
    return;
  }
  if (call != s_call) {
    // A new call: forget the last one's number and rings.
    s_call = call;
    s_call_listed = false;
    st->rings = 0;
    st->clip[0] = '\0';
    s_rec.outgoing = state == ZBT_CALL_OUTGOING || state == ZBT_CALL_ALERTING;
    s_rec.answered = false;
    s_rec.number[0] = '\0';
    if (s_rec.outgoing && s_dial_pending) {
      memcpy(s_rec.number, s_dial_number, sizeof(s_rec.number));
      // The other side's number, as the caller ID is for an incoming call.
      memcpy(st->clip, s_dial_number, sizeof(st->clip));
    }
    s_dial_pending = false;
    st->dialed_here = s_rec.outgoing && s_watch_dialing;
    s_watch_dialing = false;
    s_audio_chosen = false;
    s_answered_here = false;
    st->call_virtual = false;
    (void)k_work_reschedule(&s_call_check_work, K_MSEC(CALL_CHECK_MS));
    if (s_rec.outgoing && !st->dialed_here) {
      s_phone_call_tries = 0;
      (void)k_work_reschedule(&s_phone_call_work, K_MSEC(PHONE_CALL_MS));
    }
  }
  if (state == ZBT_CALL_ACTIVE && !s_rec.answered) {
    s_rec.answered = true;
    s_rec.answered_ms = k_uptime_get();
    if (!s_rec.outgoing && !s_answered_here && !s_audio_chosen && !s_audio_on_phone) {
      // A ring answered on the phone: its audio stays there.
      prv_set_audio_on_phone(true);
      if (st->sco) {
        k_work_submit(&s_sco_drop_work);
      }
    }
  }
}

static void prv_outgoing(struct bt_hfp_hf *hf, struct bt_hfp_hf_call *call) {
  prv_set_call(call, ZBT_CALL_OUTGOING);
}

static void prv_remote_ringing(struct bt_hfp_hf_call *call) {
  prv_set_call(call, ZBT_CALL_ALERTING);
}

static void prv_incoming(struct bt_hfp_hf *hf, struct bt_hfp_hf_call *call) {
  prv_set_call(call, ZBT_CALL_INCOMING);
}

static void prv_accept(struct bt_hfp_hf_call *call) {
  prv_set_call(call, ZBT_CALL_ACTIVE);
}

static void prv_ended(struct bt_hfp_hf_call *call) {
  prv_set_call(call, ZBT_CALL_ENDED);
}

static void prv_held(struct bt_hfp_hf_call *call) {
  prv_set_call(call, ZBT_CALL_HELD);
}

static void prv_ring(struct bt_hfp_hf_call *call) {
  zbt_ctl_status()->rings++;
}

// Cuts @p s (@p len bytes) before a UTF-8 sequence left short at its end, as copying a caller
// ID (an app's call may give a name) into ZBT_CLIP_MAX bytes can.
static void prv_utf8_whole(char *s, size_t len) {
  size_t lead = len;
  while (lead > 0 && ((uint8_t)s[lead - 1] & 0xc0) == 0x80) {
    lead--;
  }
  if (lead > 0) {
    const uint8_t c = (uint8_t)s[lead - 1];
    const size_t want = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
    if (len - (lead - 1) < want) {
      s[lead - 1] = '\0';
    }
  }
}

static void prv_clip(struct bt_hfp_hf_call *call, const char *number, uint8_t type) {
  struct zbt_status *st = zbt_ctl_status();
  zbt_trace('L');
  strncpy(st->clip, number, sizeof(st->clip) - 1);
  st->clip[sizeof(st->clip) - 1] = '\0';
  prv_utf8_whole(st->clip, strlen(st->clip));
  memcpy(st->last_clip, st->clip, sizeof(st->last_clip));
}

static void prv_service(struct bt_hfp_hf *hf, uint32_t value) {
  zbt_ctl_status()->ind_service = (int8_t)value;
}

static void prv_signal(struct bt_hfp_hf *hf, uint32_t value) {
  zbt_ctl_status()->ind_signal = (int8_t)value;
}

static void prv_roam(struct bt_hfp_hf *hf, uint32_t value) {
  zbt_ctl_status()->ind_roam = (int8_t)value;
}

static void prv_battery(struct bt_hfp_hf *hf, uint32_t value) {
  zbt_ctl_status()->ind_battery = (int8_t)value;
}

static void prv_inband_ring(struct bt_hfp_hf *hf, bool inband) {
  zbt_ctl_status()->inband_ring = inband;
}

static void prv_query_call(struct bt_hfp_hf *hf, struct bt_hfp_hf_current_call *call);

static struct bt_hfp_hf_cb s_hf_cb = {
  .connected = prv_connected,
  .disconnected = prv_disconnected,
  .sco_connected = prv_sco_connected,
  .sco_disconnected = prv_sco_disconnected,
  .service = prv_service,
  .outgoing = prv_outgoing,
  .remote_ringing = prv_remote_ringing,
  .incoming = prv_incoming,
  .accept = prv_accept,
  .reject = prv_ended,
  .terminate = prv_ended,
  .held = prv_held,
  .retrieve = prv_accept,
  .signal = prv_signal,
  .roam = prv_roam,
  .battery = prv_battery,
  .inband_ring = prv_inband_ring,
  .ring_indication = prv_ring,
  .clip = prv_clip,
  .query_call = prv_query_call,
  .subscriber_number = prv_subscriber_number,
};

/* Call control, on the Zephyr thread */

static void prv_answer_work(struct k_work *work) {
  zbt_trace('a');
  s_answered_here = true;
  zbt_ctl_status()->call_err = s_call != NULL ? bt_hfp_hf_accept(s_call) : -ENOENT;
}

static void prv_hangup_work(struct k_work *work) {
  zbt_trace('h');
  zbt_ctl_status()->call_err = s_call != NULL ? bt_hfp_hf_terminate(s_call) : -ENOENT;
}

static void prv_sco_drop_work(struct k_work *work) {
  struct bt_conn *sco = s_hf != NULL ? atomic_ptr_get(&s_hf->sco_conn) : NULL;
  if (sco != NULL) {
    zbt_ctl_status()->call_err = bt_conn_disconnect(sco, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
  }
}

// Ask the phone for its calls (AT+CLCC): in the trace, B for one still being asked, F for
// another failure.
static int prv_list_calls(void) {
  // A listing the host has as pending but neither sending nor queued: its AT+CLCC failed to be
  // queued (hf_query_current_calls() leaves the flag set then), and every later one would be
  // skipped. Let it go.
  if (atomic_test_bit(s_hf->flags, BT_HFP_HF_FLAG_CLCC_PENDING) &&
      !atomic_test_bit(s_hf->flags, BT_HFP_HF_FLAG_TX_ONGOING) &&
      k_fifo_is_empty(&s_hf->tx_pending)) {
    zbt_trace('X');
    atomic_clear_bit(s_hf->flags, BT_HFP_HF_FLAG_CLCC_PENDING);
    atomic_clear_bit(s_hf->flags, BT_HFP_HF_FLAG_USR_CLCC_PND);
    atomic_clear_bit(s_hf->flags, BT_HFP_HF_FLAG_USR_CLCC_CMD);
  }
  const int err = bt_hfp_hf_query_list_of_current_calls(s_hf);
  if (err != 0) {
    zbt_trace(err == -EBUSY ? 'B' : 'F');
  }
  return err;
}

static void prv_phone_call_work(struct k_work *work) {
  if (s_hf == NULL || s_call == NULL || zbt_ctl_status()->dialed_here) {
    return;
  }
  const int err = prv_list_calls();
  if (err == -EBUSY && ++s_phone_call_tries < PHONE_CALL_TRIES) {
    (void)k_work_reschedule(&s_phone_call_work, K_MSEC(PHONE_CALL_RETRY));
  }
}

static void prv_call_check_work(struct k_work *work) {
  if (s_hf == NULL || s_call == NULL) {
    return;
  }
  (void)prv_list_calls();
  (void)k_work_reschedule(&s_call_check_work, K_MSEC(CALL_CHECK_MS));
}

// The phone's list of current calls (from prv_phone_call_work() and prv_call_check_work()).
static void prv_query_call(struct bt_hfp_hf *hf, struct bt_hfp_hf_current_call *call) {
  struct zbt_status *st = zbt_ctl_status();
  zbt_trace((call->number != NULL && call->number[0] != '\0') ? 'Q' : 'q');
  const bool first = !s_call_listed;
  s_call_listed = true;
  if (call->dir != BT_HFP_HF_CALL_DIR_OUTGOING || s_call == NULL || st->dialed_here ||
      st->call_virtual) {
    return;
  }
  // Android's virtual call is listed active from the start, with the phone's own number (or
  // none); a call dialed on the phone is dialing or ringing first, with the other side's.
  const bool no_number = call->number == NULL || call->number[0] == '\0';
  const bool own_number =
      !no_number && st->own_number[0] != '\0' && strcmp(call->number, st->own_number) == 0;
  const bool active_first = first && call->status == BT_HFP_HF_CALL_STATUS_ACTIVE;
  if (no_number || own_number || active_first) {
    zbt_trace('v');
    st->call_virtual = true;
    zbt_ctl_call_changed();
    return;
  }
  st->call_virtual = false;
  // Its number, for the call's screen and log.
  if (st->clip[0] == '\0') {
    strncpy(st->clip, call->number, sizeof(st->clip) - 1);
    st->clip[sizeof(st->clip) - 1] = '\0';
    prv_utf8_whole(st->clip, strlen(st->clip));
  }
  if (s_rec.number[0] == '\0') {
    strncpy(s_rec.number, call->number, sizeof(s_rec.number) - 1);
    s_rec.number[sizeof(s_rec.number) - 1] = '\0';
  }
  if (!s_audio_chosen && !s_audio_on_phone) {
    prv_set_audio_on_phone(true);
    k_work_submit(&s_sco_drop_work);
  }
}

static void prv_audio_phone_work(struct k_work *work) {
  zbt_trace('p');
  s_audio_chosen = true;
  prv_set_audio_on_phone(true);
  prv_sco_drop_work(work);
}

static void prv_audio_watch_work(struct k_work *work) {
  zbt_trace('w');
  struct zbt_status *st = zbt_ctl_status();
  s_audio_chosen = true;
  prv_set_audio_on_phone(false);
  // A call under way with its audio on the phone: bring it over. (Before answering a ring, the
  // phone does when answered.)
  if (s_hf != NULL && s_call != NULL && !st->sco &&
      (st->call == ZBT_CALL_OUTGOING || st->call == ZBT_CALL_ALERTING ||
       st->call == ZBT_CALL_ACTIVE)) {
    st->call_err = bt_hfp_hf_audio_connect(s_hf);
  }
}

static void prv_dial_work(struct k_work *work) {
  s_dial_pending = true;
  s_watch_dialing = true;
  // Before the phone reports the call: its app's report must not take the audio to the phone.
  zbt_ctl_status()->dialed_here = true;
  const int err = s_hf != NULL ? bt_hfp_hf_number_call(s_hf, s_dial_number) : -ENOTCONN;
  zbt_ctl_status()->call_err = err;
  if (err != 0) {
    s_dial_pending = false;
    s_watch_dialing = false;
    zbt_ctl_status()->dialed_here = false;
  }
}

static void prv_redial_work(struct k_work *work) {
  s_watch_dialing = true;
  zbt_ctl_status()->dialed_here = true;
  zbt_ctl_status()->call_err = s_hf != NULL ? bt_hfp_hf_redial(s_hf) : -ENOTCONN;
  if (zbt_ctl_status()->call_err != 0) {
    s_watch_dialing = false;
    zbt_ctl_status()->dialed_here = false;
  }
}

int zbt_host_hfp_answer(void) {
  k_work_submit(&s_answer_work);
  return 0;
}

int zbt_host_hfp_hangup(void) {
  // A dial not sent yet is not made.
  if (zbt_host_hfp_dial_cancel()) {
    return 0;
  }
  k_work_submit(&s_hangup_work);
  return 0;
}

int zbt_host_hfp_audio_on_watch(bool on_watch) {
  k_work_submit(on_watch ? &s_audio_watch_work : &s_audio_phone_work);
  return 0;
}

int zbt_host_hfp_dial(const char *number, uint32_t delay_ms) {
  if (k_work_delayable_busy_get(&s_dial_work) != 0) {
    return -EBUSY;
  }
  strncpy(s_dial_number, number, sizeof(s_dial_number) - 1);
  s_dial_number[sizeof(s_dial_number) - 1] = '\0';
  (void)k_work_reschedule(&s_dial_work, K_MSEC(delay_ms));
  return 0;
}

bool zbt_host_hfp_dial_cancel(void) {
  // Only one not sent yet: waiting, or queued but not running.
  const int busy = k_work_delayable_busy_get(&s_dial_work);
  if (!(busy & (K_WORK_DELAYED | K_WORK_QUEUED))) {
    return false;
  }
  if (k_work_cancel_delayable(&s_dial_work) != 0) {
    // Running by now: sent.
    return false;
  }
  zbt_trace('k');
  return true;
}

int zbt_host_hfp_redial(void) {
  k_work_submit(&s_redial_work);
  return 0;
}

int zbt_hfp_init(void) {
  k_work_init(&s_slc_work, prv_slc_work);
  k_work_init(&s_answer_work, prv_answer_work);
  k_work_init(&s_hangup_work, prv_hangup_work);
  k_work_init(&s_audio_phone_work, prv_audio_phone_work);
  k_work_init(&s_audio_watch_work, prv_audio_watch_work);
  k_work_init(&s_sco_drop_work, prv_sco_drop_work);
  k_work_init_delayable(&s_dial_work, prv_dial_work);
  k_work_init(&s_redial_work, prv_redial_work);
  k_work_init(&s_bia_work, prv_bia_work);
  k_work_init_delayable(&s_own_number_work, prv_own_number_work);
  k_work_init_delayable(&s_phone_call_work, prv_phone_call_work);
  k_work_init_delayable(&s_call_check_work, prv_call_check_work);
  return bt_hfp_hf_register(&s_hf_cb);
}
