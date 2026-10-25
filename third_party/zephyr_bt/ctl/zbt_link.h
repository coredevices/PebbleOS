/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Between the control side (ctl/, in the kernel's RAM, alive for the whole
// boot) and the host side (the Zephyr host and port/, in the worker RAM, only
// alive between zbt_start() and zbt_stop()).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/thread.h>
#include <pbl/mcu/mpu.h>

#include <zbt/zbt.h>

//! A BR/EDR link key, as the host stores it.
#define ZBT_LINK_KEY_MAX 24
struct zbt_link_key {
  bool valid;
  uint8_t addr[6];
  uint8_t len;
  uint8_t data[ZBT_LINK_KEY_MAX];
};

//! A BR/EDR link key derived from the phone's LE pairing.
struct zbt_derived_key {
  bool valid;
  uint8_t addr[6];
  uint8_t val[16];
  //! The LE pairing was authenticated (MITM), so the key is too.
  bool authenticated;
};

//! One thread runs every Zephyr work queue, the host's ACL input (L2CAP, SDP, RFCOMM, AT)
//! included: answering the phone's SDP query for the hands-free record recurses with variable
//! length arrays (about 1.3 KB from the call graph); an incoming call peaked at 736 bytes.
#ifndef CONFIG_ZBT_EXECUTOR_STACK_SIZE
#define CONFIG_ZBT_EXECUTOR_STACK_SIZE 2048
#endif
//! The HCI input thread; its stack only needs bt_recv()'s in-place handling of command
//! completions.
#ifndef CONFIG_ZBT_RX_STACK_SIZE
#define CONFIG_ZBT_RX_STACK_SIZE 1024
#endif

//! The host threads' structures and stacks, the control side's. The scheduler reads and writes
//! them (a waiter's semaphore is on its thread's stack) from whichever thread or interrupt runs,
//! and threads without the worker RAM's MPU region reach the worker RAM uncached while those
//! with it cache it: in the kernel's RAM, every context sees them the same way.
struct zbt_host_threads {
  struct pbl_thread exec;
  struct pbl_thread rx;
  uint8_t exec_stack[CONFIG_ZBT_EXECUTOR_STACK_SIZE] __attribute__((aligned(8)));
  uint8_t rx_stack[CONFIG_ZBT_RX_STACK_SIZE] __attribute__((aligned(8)));
};

/* Host side, called by the control side --------------------------------------*/

//! Start the host threads and bt_enable(), with the worker RAM freshly set up, and give the
//! host @p derived, or else @p key (either may be NULL), and its BR/EDR name @p name (empty
//! keeps the default), once it is up. The threads run on @p threads and get @p regions, the MPU
//! regions of a kernel task, so they see the worker RAM as the kernel tasks do.
int zbt_host_start(const struct zbt_link_key *key, const struct zbt_derived_key *derived,
                   const char *name, const MpuRegion *const regions[PBL_THREAD_MAX_MEM_REGIONS],
                   struct zbt_host_threads *threads);

//! Ask the host to disconnect and stop scanning; zbt_host_quiesced() tells when it is done.
void zbt_host_stop(void);
bool zbt_host_quiesced(void);

//! End the host threads; zbt_host_stopped() tells when both have exited.
void zbt_host_quit(void);
bool zbt_host_stopped(void);

//! An HCI packet for the host, on the HCI receive thread.
void zbt_host_rx(const uint8_t *pkt, size_t len, bool evt);

//! @p name NULL keeps the current name.
int zbt_host_set_discoverable(bool on, const char *name, uint32_t cod);

//! @p unattended: the watch's own try, no one to confirm a pairing, which is then cancelled.
int zbt_host_connect(const uint8_t addr[6], bool unattended);
int zbt_host_hfp_answer(void);
int zbt_host_hfp_hangup(void);
int zbt_host_hfp_audio_on_watch(bool on_watch);
int zbt_host_hfp_dial(const char *number, uint32_t delay_ms);
bool zbt_host_hfp_dial_cancel(void);
int zbt_host_hfp_redial(void);
int zbt_host_pbap_sync(unsigned books);
//! The entries handed over are saved: ask the phone for more.
void zbt_host_pbap_next(void);
int zbt_host_pairing_reply(bool accept);

//! Drop the host's link keys.
int zbt_host_unpair(void);

void zbt_host_stacks(struct zbt_stack *exec, struct zbt_stack *rx);
void zbt_host_counts(uint32_t *exec_loops, uint32_t *rx_items);

/* Control side, called by the host side --------------------------------------*/

//! The status the host side updates; it lives on the control side.
struct zbt_status *zbt_ctl_status(void);

//! Attach to the HCI router's classic port, through the control side's gate.
int zbt_ctl_hci_open(void);

void zbt_ctl_enabled(int err);

//! The phone's link came up or went down.
void zbt_ctl_link_changed(bool up);

//! A new or changed link key, after the link got encrypted with it.
void zbt_ctl_save_key(const struct zbt_link_key *key);

//! A Zephyr assert or panic.
void zbt_ctl_fatal(const char *file, int line);

//! The call's voice link (SCO) came up or went down.
void zbt_ctl_sco(bool up);

//! The call's state changed (host thread).
void zbt_ctl_call_changed(void);

//! A call ended (host thread): log it, from KernelBG.
void zbt_ctl_call_ended(const struct zbt_call_record *rec);

//! Phone book reading (host thread): a book starts, entries come (false when full: hand
//! them over first), and they are handed over to KernelBG to save, which then calls
//! zbt_host_pbap_next() unless @p end. At the end, @p ok keeps the book, else the old one stays.
void zbt_ctl_book_start(enum zbt_book book);
bool zbt_ctl_book_entry(const struct zbt_book_entry *entry);
void zbt_ctl_book_hand_over(bool end, bool ok);

//! Memory for a phone book reading (host thread), from the kernel heap while it lasts: taken
//! when a reading starts (NULL: none left), given back once the host no longer refers to it.
//! The port checks its state fits.
#define ZBT_PBAP_MEM_SIZE 1280
void *zbt_ctl_pbap_acquire(void);
void zbt_ctl_pbap_release(void);

//! The host stopped: what a reading still held goes back (control side).
void zbt_book_host_stopped(void);

//! Port diagnostics: a wait with the interrupt lock held from outside (@p a: the caller), and a
//! waiter left in a wait list (@p a: the list, @p b: the waiter, @p c: the waker).
enum {
  ZBT_DIAG_BLOCK_LOCKED = 0xC1,
  ZBT_DIAG_STALE_WAITER = 0xC2,
};
//! Records the diagnostic and reboots (on the watch), or halts the host (elsewhere).
void zbt_ctl_diag(uint16_t kind, uint32_t a, uint32_t b, uint32_t c);
