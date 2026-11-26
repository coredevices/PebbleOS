/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>
#include <string.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/section.h>
#include <pbl/rtt/rtt.h>

#define RTT_ID                 "SEGGER RTT"
#define RTT_MODE_NO_BLOCK_SKIP 0U
#define FLUSH_STEP_US          100U

static_assert(sizeof(RTT_ID) <= PBL_RTT_ID_SIZE, "RTT identifier too long");

// The probe reads and writes these behind any data cache
struct pbl_rtt_cb _SEGGER_RTT PBL_NOCACHE;

static uint8_t s_up[CONFIG_RTT_NUM_UP][CONFIG_RTT_UP_SIZE] PBL_NOCACHE;
static uint8_t s_down[CONFIG_RTT_NUM_DOWN][CONFIG_RTT_DOWN_SIZE] PBL_NOCACHE;
// Read offset of each up channel last seen, to tell whether a probe reads it
static uint32_t s_rd_seen[CONFIG_RTT_NUM_UP];
static bool s_reader[CONFIG_RTT_NUM_UP];

// The identifier comes last, so a probe never finds a half set up block
static void prv_setup_locked(void) {
  if (_SEGGER_RTT.max_up != 0) {
    return;
  }

  for (size_t i = 0U; i < CONFIG_RTT_NUM_UP; i++) {
    _SEGGER_RTT.up[i] = (struct pbl_rtt_buffer){
      .name = (i == 0U) ? "Terminal" : nullptr,
      .buf = s_up[i],
      .size = CONFIG_RTT_UP_SIZE,
      .flags = RTT_MODE_NO_BLOCK_SKIP,
    };
  }
  for (size_t i = 0U; i < CONFIG_RTT_NUM_DOWN; i++) {
    _SEGGER_RTT.down[i] = (struct pbl_rtt_buffer){
      .name = (i == 0U) ? "Terminal" : nullptr,
      .buf = s_down[i],
      .size = CONFIG_RTT_DOWN_SIZE,
      .flags = RTT_MODE_NO_BLOCK_SKIP,
    };
  }
  _SEGGER_RTT.max_up = CONFIG_RTT_NUM_UP;
  _SEGGER_RTT.max_down = CONFIG_RTT_NUM_DOWN;

  atomic_thread_fence(memory_order_seq_cst);
  memcpy(_SEGGER_RTT.id, RTT_ID, sizeof(RTT_ID));
  atomic_thread_fence(memory_order_seq_cst);
}

static void prv_note_reader_locked(unsigned int ch) {
  const uint32_t rd = _SEGGER_RTT.up[ch].rd;

  if (rd != s_rd_seen[ch]) {
    s_rd_seen[ch] = rd;
    s_reader[ch] = true;
  }
}

void pbl_rtt_init(void) {
  pbl_irq_lock();
  prv_setup_locked();
  pbl_irq_unlock();
}

size_t pbl_rtt_write(unsigned int ch, const void *data, size_t len) {
  const uint8_t *bytes = data;

  if (ch >= CONFIG_RTT_NUM_UP) {
    return 0U;
  }

  pbl_irq_lock();
  prv_setup_locked();
  prv_note_reader_locked(ch);

  struct pbl_rtt_buffer *up = &_SEGGER_RTT.up[ch];
  uint32_t wr = up->wr;
  const uint32_t rd = up->rd;
  const uint32_t avail = (rd > wr) ? rd - wr - 1U : up->size - (wr - rd) - 1U;

  if (len > avail) {
    pbl_irq_unlock();
    return 0U;
  }

  for (size_t i = 0U; i < len; i++) {
    up->buf[wr] = bytes[i];
    wr = (wr + 1U) % up->size;
  }
  // The data before the offset that hands it to the probe
  atomic_thread_fence(memory_order_release);
  up->wr = wr;
  pbl_irq_unlock();

  return len;
}

size_t pbl_rtt_read(unsigned int ch, void *data, size_t len) {
  uint8_t *bytes = data;
  size_t n = 0U;

  if (ch >= CONFIG_RTT_NUM_DOWN) {
    return 0U;
  }

  pbl_irq_lock();
  prv_setup_locked();

  struct pbl_rtt_buffer *down = &_SEGGER_RTT.down[ch];
  uint32_t rd = down->rd;
  const uint32_t wr = down->wr;

  // The offset the probe wrote before the data it covers
  atomic_thread_fence(memory_order_acquire);
  while (n < len && rd != wr) {
    bytes[n++] = down->buf[rd];
    rd = (rd + 1U) % down->size;
  }
  // The data read before the probe may overwrite it
  atomic_thread_fence(memory_order_release);
  down->rd = rd;
  pbl_irq_unlock();

  return n;
}

bool pbl_rtt_flush(unsigned int ch, uint32_t timeout_us) {
  uint32_t waited_us = 0U;

  if (ch >= CONFIG_RTT_NUM_UP) {
    return true;
  }

  for (;;) {
    pbl_irq_lock();
    prv_setup_locked();
    prv_note_reader_locked(ch);
    const bool empty = _SEGGER_RTT.up[ch].rd == _SEGGER_RTT.up[ch].wr;
    const bool reader = s_reader[ch];
    if (!empty && reader && waited_us >= timeout_us) {
      // Gone: later flushes do not wait for it until it reads again
      s_reader[ch] = false;
    }
    pbl_irq_unlock();

    if (empty) {
      return true;
    }
    if (!reader || waited_us >= timeout_us) {
      return false;
    }

    pbl_busy_wait_us(FLUSH_STEP_US);
    waited_us += FLUSH_STEP_US;
  }
}

#if UNITTEST
void pbl_rtt_reset_for_test(void) {
  memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));
  memset(s_rd_seen, 0, sizeof(s_rd_seen));
  memset(s_reader, 0, sizeof(s_reader));
}
#endif
