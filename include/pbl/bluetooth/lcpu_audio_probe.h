/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @file
 * @brief The SF32LB52 LCPU's voice (SCO) path.
 *
 * Only on boots with the HCI router active. The LCPU ROM sets its voice path
 * up by itself when a SCO link comes up: each SCO packet it receives goes into
 * the ring towards the HCPU and rings IPC queue 6, and it sends what the HCPU
 * puts in the other ring. The HCPU only reads the first ring (moving its read
 * position) and writes the second; it never sets the rings up.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** struct rt_ringbuffer of the SiFli SDK, as the LCPU lays it out. */
struct lcpu_audio_ring {
  uint32_t rd_buffer_ptr;
  uint32_t wr_buffer_ptr;
  //! Each side's mirror and index, also as the one word the LCPU reads and writes them as.
  union {
    struct {
      uint16_t read_mirror;
      uint16_t read_index;
    };
    uint32_t read_word;
  };
  union {
    struct {
      uint16_t write_mirror;
      uint16_t write_index;
    };
    uint32_t write_word;
  };
  int16_t buffer_size;
  uint16_t pad;
};

struct lcpu_audio_probe {
  /** False when the LCPU is not running (or the router is not active). */
  bool valid;
  /** Chip revision (__HAL_SYSCFG_GET_REVID()). */
  uint8_t rev_id;
  /** LCPU variables pt_tx_rbf, pt_rx_rbf, p_sco_sta, p_hl_sco_para (pointers), at the addresses
   *  of the rev A LCPU image. */
  uint32_t vars[4];
  /** Shared memory: LCPU to HCPU (received voice) and HCPU to LCPU rings. */
  struct lcpu_audio_ring to_hcpu;
  struct lcpu_audio_ring to_lcpu;
  /** SCO status counters (handle err, buffer err, ring empty, ring full, param change, rx). */
  uint16_t sco_sta[6];
  /** SCO parameters the LCPU keeps (an HCI Synchronous Connection Complete), and where they
   *  were read: where p_hl_sco_para points if that is in the shared voice memory, else where the
   * SDK's layout puts them. */
  uint8_t sco_para[20];
  uint32_t sco_para_addr;
  /** "Voice packet" notifications from the LCPU (IPC queue 6) since boot. */
  uint32_t notifications;
  /** The LCPU configuration's soft CVSD word (0x5A5AA5A5: hand the air bits over as they are). */
  uint32_t soft_cvsd;
  /** Before this read woke it: the HCPU's requests to keep the LCPU awake (refcount), whether
   *  the request bit is set, and whether the LCPU was active. Outside a call, all 0 but for a
   *  moment now and then; a request left set keeps the LCPU from sleeping. */
  uint8_t lcpu_wake_refs;
  bool lcpu_wake_req;
  bool lcpu_active;
};

/** @brief Take a look; wakes the LCPU for the time of the reads. */
void lcpu_audio_probe_read(struct lcpu_audio_probe *out);

/** @brief Called from the IPC interrupt for each voice notification: only wake a task. */
typedef void (*lcpu_audio_notify_t)(void *context);

/** @brief Set (or, with NULL, clear) the notification callback. */
void lcpu_audio_set_notify(lcpu_audio_notify_t cb, void *context);

/**
 * @brief Keep the LCPU awake (for a call), or let it sleep again.
 * @return Whether the LCPU is now kept awake.
 */
bool lcpu_audio_hold(bool hold);

/**
 * @brief Take one packet of exactly @p len bytes from the ring towards the HCPU.
 * @return @p len, or 0 if no whole packet is there or the LCPU is not readable.
 */
size_t lcpu_audio_rx_read(uint8_t *out, size_t len);

/** @brief Bytes in front of each packet towards the HCPU: length, status, two reserved. */
#define LCPU_AUDIO_HDR_BYTES 4

/**
 * @brief Take the next whole packet (header and data, as long as its header says) from the ring
 *        towards the HCPU.
 * @return Its length; 0 if no whole packet is there or the LCPU is not readable. A header saying
 *         more than @p max drops what the ring holds and returns the header alone.
 */
size_t lcpu_audio_rx_packet(uint8_t *out, size_t max);

/**
 * @brief Put @p len bytes into the ring towards the LCPU.
 * @return @p len, or 0 if there is no room or the LCPU is not writable.
 */
size_t lcpu_audio_tx_write(const uint8_t *in, size_t len);
