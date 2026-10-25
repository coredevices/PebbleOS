/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file
 * @brief Zephyr's Bluetooth host as a second, BR/EDR host next to NimBLE.
 *
 * It runs only on boots that route HCI through the HCI router, and only
 * between zbt_start() and zbt_stop(): its RAM is the worker RAM, borrowed for
 * that time (workers are closed and not launched meanwhile).
 *
 * The link key of the bonded phone is derived at zbt_start() from the phone's
 * LE Secure Connections pairing (both sides agree on it with the LinkKey bit of
 * the LE key distribution). Without one, the key kept in the settings file
 * "bt_classic_keys", read only by zbt_start(), is used.
 */

enum zbt_state {
  //! Not running; the worker RAM is not borrowed.
  ZBT_STATE_OFF,
  //! Waiting for the worker RAM, or bringing the host up.
  ZBT_STATE_STARTING,
  ZBT_STATE_READY,
  //! The start failed (enable_err). If the RAM is still held, zbt_stop() releases it.
  ZBT_STATE_FAILED,
  //! The port caught a Zephyr assert or panic. zbt_stop() releases the RAM.
  ZBT_STATE_HALTED,
  //! Disconnecting, then waiting for the host threads to exit.
  ZBT_STATE_STOPPING,
};

//! BR/EDR link to the phone.
enum zbt_link {
  ZBT_LINK_NONE,
  ZBT_LINK_CONNECTING,
  ZBT_LINK_CONNECTED,
  ZBT_LINK_DISCONNECTED,
};

//! Secure Simple Pairing, as seen from the watch.
enum zbt_pairing {
  ZBT_PAIRING_NONE,
  //! Numeric comparison: show the passkey, then zbt_pairing_reply().
  ZBT_PAIRING_CONFIRM,
  //! Just Works: zbt_pairing_reply() to accept.
  ZBT_PAIRING_CONSENT,
  ZBT_PAIRING_DONE,
  ZBT_PAIRING_FAILED,
  ZBT_PAIRING_CANCELLED,
};

//! Hands-free service level connection (SLC) to the phone.
enum zbt_hfp {
  ZBT_HFP_NONE,
  //! Looking the phone's hands-free gateway up (SDP).
  ZBT_HFP_SDP,
  //! RFCOMM and the SLC set-up.
  ZBT_HFP_CONNECTING,
  ZBT_HFP_CONNECTED,
  ZBT_HFP_DISCONNECTED,
};

//! The phone's call, as the hands-free indicators tell it.
enum zbt_call {
  ZBT_CALL_NONE,
  ZBT_CALL_INCOMING,
  ZBT_CALL_OUTGOING,
  //! Outgoing, the other side rings.
  ZBT_CALL_ALERTING,
  ZBT_CALL_ACTIVE,
  ZBT_CALL_HELD,
  ZBT_CALL_ENDED,
};

#define ZBT_CLIP_MAX 24

//! Phone book access (PBAP): reading the phone's books into the watch.
enum zbt_pbap {
  ZBT_PBAP_IDLE,
  ZBT_PBAP_SEARCHING,
  ZBT_PBAP_CONNECTING,
  ZBT_PBAP_PULLING,
  ZBT_PBAP_DONE,
  ZBT_PBAP_FAILED,
};

//! The phone's books the watch keeps.
enum zbt_book {
  ZBT_BOOK_CONTACTS,
  //! The phone's call history, newest first.
  ZBT_BOOK_HISTORY,
  ZBT_BOOK_FAVORITES,
  ZBT_BOOK_COUNT,
};

#define ZBT_BOOK_ALL ((1U << ZBT_BOOK_COUNT) - 1)
#define ZBT_NAME_MAX 64
#define ZBT_SORT_MAX 24

//! Where a contact goes in an index of the book: a Latin letter, a kana row (あ, か, ... わ)
//! from its reading, or the rest.
enum zbt_group {
  ZBT_GROUP_A = 0,
  ZBT_GROUP_Z = 25,
  ZBT_GROUP_KANA_A = 26,
  ZBT_GROUP_KANA_WA = 35,
  ZBT_GROUP_OTHER = 36,
  ZBT_GROUP_COUNT,
};

struct zbt_stack {
  uint16_t size;
  uint16_t used;
};

struct zbt_status {
  enum zbt_state state;
  //! bt_enable() result, or -ETIMEDOUT when the worker did not close in time.
  int enable_err;

  //! Whether the host holds the worker RAM, and how much of it the host uses.
  bool ram_held;
  uint32_t ram_used;
  //! How long the last start waited for the worker to close, in ms.
  uint32_t worker_wait_ms;
  //! Worker launches refused since the last start.
  uint32_t worker_refused;
  uint16_t starts;
  uint16_t stops;
  //! 0, or -ETIMEDOUT when the last stop could not end the host threads.
  int stop_err;

  //! Last command the Zephyr host sent, and the last one it got a reply for.
  uint16_t last_cmd_sent;
  uint16_t last_cmd_done;
  uint32_t rx_evt;
  uint32_t rx_acl;
  //! Packets for the Zephyr host dropped (no buffer, host not listening).
  uint32_t rx_drop;
  uint32_t tx_cmd;
  uint32_t tx_acl;

  //! Result of the last discoverable change, and whether it is on.
  int discoverable_err;
  bool discoverable;

  //! Link: state, and the HCI error or disconnect reason.
  enum zbt_link link;
  int link_err;
  //! The phone turned the last link down: its key or a pairing refused, or a pairing the
  //! watch did not ask the user for.
  bool link_refused;
  //! Pairing: stage, the passkey to compare, the failure reason, and whether it bonded.
  enum zbt_pairing pairing;
  uint32_t passkey;
  int pairing_err;
  bool bonded;
  //! Security level (bt_security_t) and whether the link is encrypted.
  uint8_t security;
  bool encrypted;
  int security_err;

  //! Link key: one was in the key file and given to the host at start; one is in the key
  //! file now; last key file error.
  bool key_loaded;
  bool key_stored;
  int key_err;

  //! Link key derived at start from the LE pairing with the phone (it then takes the key
  //! file's place): result (-ENOENT no Secure Connections pairing, -E2BIG more than one,
  //! -EADDRNOTAVAIL no public identity address), phone, whether it is authenticated.
  int derive_err;
  bool key_derived;
  bool derived_auth;
  uint8_t derived_addr[6];
  //! The key the host holds now: 's' the key file's, 'd' the derived one, '-' none; and how
  //! many times a link refused before encryption made it take the other one.
  char key_source;
  uint8_t key_swaps;

  //! Hands-free: SLC state and its last error (or the SDP / connect result), the gateway's
  //! RFCOMM channel, the phone's indicators (-1 until known), the call, the calling number.
  enum zbt_hfp hfp;
  int hfp_err;
  uint8_t hfp_channel;
  int8_t ind_service;
  int8_t ind_signal;
  int8_t ind_roam;
  int8_t ind_battery;
  //! The gateway's features (AT+BRSF; bit 3: in-band ring tone), and its last +BSIR (-1 none).
  uint32_t ag_features;
  int8_t inband_ring;
  enum zbt_call call;
  //! When the call was answered, UTC seconds (0 until then, and after it).
  uint32_t call_since;
  uint8_t rings;
  //! Result of the last zbt_hfp_answer() / zbt_hfp_hangup().
  int call_err;
  char clip[ZBT_CLIP_MAX];
  //! The last caller ID (+CLIP) the phone sent, kept after its call.
  char last_clip[ZBT_CLIP_MAX];
  //! The phone's own number (AT+CNUM), empty until it gives one.
  char own_number[ZBT_CLIP_MAX];
  //! The call's audio is kept on the phone (zbt_hfp_audio_on_watch(false): answered there, or
  //! dialed on the phone), until moved to the watch.
  bool audio_on_phone;
  //! The watch dialed (or redialed) the call being set up or under way. One the phone sets up
  //! otherwise is dialed on the phone, or the phone's audio for something else (Android's
  //! virtual call, e.g. playing a recorded call), which has nowhere to go but the watch.
  bool dialed_here;
  //! The phone lists the call it set up as outgoing without a number: Android's virtual call,
  //! the phone's audio for an app (a recorded call played back, Discord's calls), no call to
  //! show.
  bool call_virtual;
  //! Voice link (SCO): up or not, how many came up, the last disconnect reason.
  bool sco;
  uint16_t sco_count;
  uint8_t sco_reason;

  //! Phone book access: state, the book being read, the last error (negative errno, or the
  //! phone's OBEX response code), and how many entries each book got at its last reading.
  enum zbt_pbap pbap;
  uint8_t pbap_book;
  int pbap_err;
  uint16_t pbap_entries[ZBT_BOOK_COUNT];
  //! Each book's last reading: 0 kept, the phone's OBEX response code (> 0), or a negative
  //! errno (-ETIMEDOUT no answer, -ECONNRESET link lost, -ENOBUFS entries lost, -ENOSPC or
  //! another file error saving it).
  int16_t pbap_result[ZBT_BOOK_COUNT];

  //! The phone's link: active (0) or sniff (2), the sniff interval (0.625 ms slots), how many
  //! times it went to sniff, the last sniff request's result, and the link policy set.
  uint8_t link_mode;
  uint16_t sniff_interval;
  uint16_t sniff_count;
  int sniff_err;
  uint16_t link_policy;

  //! The battery's current, sampled every 30 s while the host runs, averaged apart while
  //! the phone's link is up and idle (no call) and while it is down. 0 samples: none yet.
  int32_t bat_linked_ua;
  uint32_t bat_linked_samples;
  int32_t bat_unlinked_ua;
  uint32_t bat_unlinked_samples;
  //! And while the host is off (since its first start), to compare.
  int32_t bat_off_ua;
  uint32_t bat_off_samples;

  //! Work queue loop passes and HCI packets the host handled (a busy loop shows here).
  uint32_t exec_loops;
  uint32_t rx_items;

  //! Host threads: ZephyrBT (work queues) and ZephyrRX (HCI input). Kept after a stop.
  struct zbt_stack exec_stack;
  struct zbt_stack rx_stack;

  //! Where the port halted, if it did.
  const char *fatal_file;
  int fatal_line;
};

/**
 * @brief Start the Zephyr host.
 *
 * Returns at once. Borrows the worker RAM (closing any worker, at most a few
 * seconds), then runs bt_enable() on the Zephyr thread; the outcome shows in
 * zbt_get_status() (STARTING, then READY or FAILED).
 *
 * @return 0 once started, -ENODEV when the HCI router is not active this boot,
 *         -EALREADY when the host is not off.
 */
int zbt_start(void);

/**
 * @brief Stop the Zephyr host and give the worker RAM back.
 *
 * Returns at once. Disconnects BR/EDR links and stops scanning, ends the host
 * threads, detaches from the HCI router and relaunches the default worker;
 * the state goes STOPPING, then OFF.
 *
 * @return 0 once stopping, -EALREADY when the host holds nothing to release.
 */
int zbt_stop(void);

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
/**
 * @brief Stop the host and wait for it (or for a start under way first), up to @p timeout_ms:
 *        before the controller is powered down. From any task, KernelMain included.
 * @return 0 once off (or not running), -ETIMEDOUT or -EBUSY when it is not.
 */
int zbt_stop_wait(uint32_t timeout_ms);
#endif

/**
 * @brief Make the Zephyr host discoverable and connectable over BR/EDR.
 *
 * Runs on the Zephyr thread; the outcome shows in zbt_get_status().
 *
 * @param name Local name, also put in the extended inquiry response; NULL keeps the
 *             watch's name (the one LE advertises), which the host takes at start.
 * @param cod Class of Device.
 * @return 0 once queued, -EAGAIN when the host is not ready.
 */
int zbt_set_discoverable(const char *name, uint32_t cod);

/** @brief Stop being discoverable and connectable, like zbt_set_discoverable(). */
int zbt_clear_discoverable(void);

/**
 * @brief Connect to a BR/EDR device and authenticate the link.
 *
 * Asks for an authenticated (MITM) link, so a new pairing uses numeric
 * comparison: the passkey shows in zbt_get_status() with pairing CONFIRM, and
 * zbt_pairing_reply() answers it. Once the link is encrypted, sets the
 * hands-free connection up.
 *
 * @param addr Device address, least significant octet first.
 * @return 0 once queued, -EAGAIN when the host is not ready.
 */
int zbt_connect(const uint8_t addr[6]);

/**
 * @brief Answer the incoming call (ATA), over the hands-free connection.
 * @return 0 once queued, -EAGAIN when the host is not ready.
 */
int zbt_hfp_answer(void);

/** @brief End the call (AT+CHUP), like zbt_hfp_answer(). */
int zbt_hfp_hangup(void);

/**
 * @brief Where the call's audio goes until it ends: the watch (the default) or the phone.
 *
 * On the phone, the watch drops the call's audio link, now and whenever the phone opens it
 * again, so the phone keeps the audio.
 * @return 0 once queued, -EAGAIN when the host is not ready.
 */
int zbt_hfp_audio_on_watch(bool on_watch);

/**
 * @brief Call @p number (ATD), over the hands-free connection; the audio comes to the watch.
 * @return 0 once queued, -EAGAIN when the host is not ready, -EINVAL for an empty or too
 *         long number, -EBUSY while the last dial is still being sent.
 */
int zbt_hfp_dial(const char *number);

/**
 * @brief zbt_hfp_dial(), sent @p delay_ms from now: until then zbt_hfp_dial_cancel() (or
 *        zbt_hfp_hangup()) takes it back, nothing sent to the phone.
 */
int zbt_hfp_dial_after(const char *number, uint32_t delay_ms);

/** @brief Cancel a dial not sent yet. @return Whether there was one. */
bool zbt_hfp_dial_cancel(void);

/** @brief Call the phone's last dialed number again (AT+BLDN), like zbt_hfp_dial(). */
int zbt_hfp_redial(void);

enum zbt_call_kind {
  ZBT_CALL_KIND_INCOMING,
  ZBT_CALL_KIND_OUTGOING,
  //! Incoming, never answered (rejected included).
  ZBT_CALL_KIND_MISSED,
};

//! A call that ended while the host ran; the number is empty when unknown.
struct zbt_call_record {
  char number[ZBT_CLIP_MAX];
  //! When it ended, UTC seconds.
  uint32_t time;
  //! Seconds from answered to ended.
  uint16_t duration_s;
  uint8_t kind;
};

#define ZBT_CALL_LOG_MAX 30

/**
 * @brief The calls the watch saw, newest first, kept across reboots.
 * @return How many were copied to @p out (up to @p max).
 */
int zbt_call_log_read(struct zbt_call_record *out, int max);

//! An entry of one of the phone's books: one number of a contact, or one call.
struct zbt_book_entry {
  //! UTF-8, empty when the phone has none.
  char name[ZBT_NAME_MAX];
  char number[ZBT_CLIP_MAX];
  //! How the name reads (SORT-STRING or the phonetic name; else empty), to sort by.
  char sort[ZBT_SORT_MAX];
  //! enum zbt_group, from the reading if any, else the name.
  uint8_t group;
  //! Calls: enum zbt_call_kind, and when, in the phone's local time (year 0 when unknown).
  uint8_t kind;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint16_t year;
};

/**
 * @brief Read the phone's books (bits of 1 << enum zbt_book) into the watch, over PBAP.
 *
 * The phone asks once whether the watch may read its contacts and call history. Progress
 * shows in zbt_get_status(); a book is replaced only once read in full.
 * @return 0 once queued, -EAGAIN when the host is not ready, -EBUSY while reading.
 */
int zbt_pbap_sync(unsigned books);

/**
 * @brief Read again, in the background, those of @p books (as for zbt_pbap_sync()) not read in
 *        full within the last @p max_age_ms (or not since the host started).
 * @return 0, also when none needed it; -EAGAIN when the host is not ready.
 */
int zbt_pbap_refresh(unsigned books, uint32_t max_age_ms);

/**
 * @brief The name of @p number in the phone's contacts on the watch (the trailing digits
 *        compared, so +81 90... finds 090...).
 * @return Whether one was found, copied to @p name.
 */
bool zbt_book_find_name(const char *number, char *name, size_t size);

//! How many entries @p book has on the watch.
int zbt_book_count(enum zbt_book book);

//! Copy entries @p start on of @p book to @p out; returns how many (up to @p max).
int zbt_book_read(enum zbt_book book, int start, struct zbt_book_entry *out, int max);

/** @brief Answer a pairing in CONFIRM or CONSENT: accept, or reject it. */
int zbt_pairing_reply(bool accept);

/**
 * @brief Forget the bonded phone: delete the key file, and the host's key if it runs.
 *
 * @return 0 once queued, -ENOMEM when the background task queue is full.
 */
int zbt_clear_keys(void);

void zbt_get_status(struct zbt_status *out);

/**
 * @brief Notes a call event for the BT Classic Probe's call view: the hands-free link's call
 *        (I incoming, O outgoing, A alerting, C active, H held, E ended), its voice link (S up,
 *        s down), caller ID (L), call list (Q with a number, q without), what the watch asked
 *        of it (a answer, h hang up, k a dial cancelled before sent, p audio to the phone,
 *        w to the watch), and the phone's
 *        app's call messages and what the watch did with them (lower case). Any thread.
 */
void zbt_trace(char event);

/**
 * @brief The last call events, oldest first, each with its time in seconds after the oldest.
 * @return The length written to @p out (@p size with its terminator).
 */
size_t zbt_trace_format(char *out, size_t size);
