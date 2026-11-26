/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup subsys_rtt SEGGER RTT
 * @ingroup subsys
 * @brief Channels in RAM that a debug probe reads and writes over SWD while the target runs.
 *
 * The control block is the global @c _SEGGER_RTT, which debug probes find by that symbol in the
 * ELF. It holds @c CONFIG_RTT_NUM_UP channels from the target to the probe and
 * @c CONFIG_RTT_NUM_DOWN back, each with a buffer of @c CONFIG_RTT_UP_SIZE or
 * @c CONFIG_RTT_DOWN_SIZE bytes. Channel 0 is the terminal by convention.
 *
 * Nothing waits for a probe: a write that does not fit is dropped whole, as RTT's
 * @c NO_BLOCK_SKIP mode does. Only pbl_rtt_flush() waits, and only for a probe seen reading the
 * channel. The functions take the kernel lock, so they also run before the kernel and from a
 * fault handler, but not from a zero-latency interrupt.
 *
 * The probe reads the RAM behind any data cache, so with one the control block and the buffers
 * are in the kernel's non-cacheable section (@ref PBL_NOCACHE).
 *
 * @code{.c}
 * (void)pbl_rtt_write(0, "hello\n", 6);
 *
 * uint8_t c;
 * if (pbl_rtt_read(0, &c, 1) == 1) {
 *   // a byte from the probe
 * }
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_RTT_ID_SIZE 16U

// SEGGER RTT layout, as probes read it
struct pbl_rtt_buffer {
  const char *name;
  uint8_t *buf;
  uint32_t size;
  // Written by the producer, read by the consumer
  volatile uint32_t wr;
  // Written by the consumer, read by the producer
  volatile uint32_t rd;
  uint32_t flags;
};

struct pbl_rtt_cb {
  char id[PBL_RTT_ID_SIZE];
  int32_t max_up;
  int32_t max_down;
  struct pbl_rtt_buffer up[CONFIG_RTT_NUM_UP];
  struct pbl_rtt_buffer down[CONFIG_RTT_NUM_DOWN];
};

extern struct pbl_rtt_cb _SEGGER_RTT;
/** @endcond */

/**
 * @brief Set the control block up, if not done yet.
 *
 * The other functions do it too; this lets a probe find the block before anything is written.
 */
void pbl_rtt_init(void);

/**
 * @brief Write to an up channel.
 *
 * @param ch Up channel.
 * @param data Bytes to write.
 * @param len Number of bytes.
 * @return @p len, or 0 when they do not all fit, or @p ch does not exist, and nothing is written.
 */
size_t pbl_rtt_write(unsigned int ch, const void *data, size_t len);

/**
 * @brief Read from a down channel.
 *
 * @param ch Down channel.
 * @param[out] data Bytes read.
 * @param len Most bytes to read.
 * @return Number of bytes read, 0 when there are none or @p ch does not exist.
 */
size_t pbl_rtt_read(unsigned int ch, void *data, size_t len);

/**
 * @brief Wait for a probe to read what was written to an up channel.
 *
 * Returns at once unless a probe was seen reading the channel. A probe that does not catch up
 * within @p timeout_us counts as gone until it reads again, so later flushes do not wait.
 *
 * @param ch Up channel.
 * @param timeout_us Longest wait, in microseconds.
 * @return true when the channel is empty.
 */
bool pbl_rtt_flush(unsigned int ch, uint32_t timeout_us);

/** @} */
