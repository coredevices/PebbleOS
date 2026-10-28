/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/new_timer/new_timer.h>

/**
 * @defgroup drivers_audio_qemu Emulated audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio devices of the emulators.
 *
 * The QEMU audio device plays mono through the host. The host microphone records mono audio the
 * host sends over the QEMU channel, and silence when it sends none, paced by a timer; it also
 * runs on native builds.
 *
 * @code{.c}
 * PBL_AUDIO_QEMU_DEFINE(s_speaker, "speaker", QEMU_AUDIO_BASE, PBL_IRQN(AUDIO));
 * PBL_IRQ_CONNECT(AUDIO, 5, pbl_audio_qemu_irq_handler, &s_speaker, 0);
 * PBL_AUDIO_HOST_MIC_DEFINE(s_mic, "mic");
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_qemu_state {
  uint8_t idx;
  size_t offset;
};
/** @endcond */

/** @brief QEMU audio device. */
struct pbl_audio_qemu {
  /** Audio device. */
  struct pbl_audio audio;
  /** Base address of the registers. */
  uintptr_t base;
  /** Interrupt. */
  pbl_irq_t irq;
  /** Driver runtime state. */
  struct pbl_audio_qemu_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_ops pbl_audio_qemu_ops;
/** @endcond */

/**
 * @brief Define the QEMU audio device, a playback device.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 * @param _base Base address of the registers.
 * @param _irq Interrupt; the board connects pbl_audio_qemu_irq_handler() to it.
 */
#define PBL_AUDIO_QEMU_DEFINE(sym, _name, _base, _irq)                                         \
  PBL_AUDIO_STATE_DEFINE(sym);                                                                 \
  static struct pbl_audio_qemu_state sym##_qemu_state;                                         \
  static const struct pbl_audio_qemu sym = {                                                   \
    .audio =                                                                                   \
        PBL_AUDIO_INIT(sym, _name, PBL_AUDIO_PLAYBACK, &pbl_audio_qemu_ops, nullptr, nullptr), \
    .base = (_base),                                                                           \
    .irq = (_irq),                                                                             \
    .state = &sym##_qemu_state,                                                                \
  };                                                                                           \
  PBL_DEVICE_REGISTER(sym, &sym.audio.dev)

/**
 * @brief QEMU audio device interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param dev Device.
 */
void pbl_audio_qemu_irq_handler(const struct pbl_audio_qemu *dev);

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_host_mic_state {
  TimerID timer;
  struct pbl_mutex lock;
  int16_t *ring;
  size_t ring_read;
  size_t ring_count;
  uint8_t idx;
};
/** @endcond */

/** @brief Host microphone. */
struct pbl_audio_host_mic {
  /** Audio device. */
  struct pbl_audio audio;
  /** Driver runtime state. */
  struct pbl_audio_host_mic_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_ops pbl_audio_host_mic_ops;
/** @endcond */

/**
 * @brief Define the host microphone, a capture device.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 */
#define PBL_AUDIO_HOST_MIC_DEFINE(sym, _name)                                                     \
  PBL_AUDIO_STATE_DEFINE(sym);                                                                    \
  static struct pbl_audio_host_mic_state sym##_host_mic_state;                                    \
  static const struct pbl_audio_host_mic sym = {                                                  \
    .audio =                                                                                      \
        PBL_AUDIO_INIT(sym, _name, PBL_AUDIO_CAPTURE, &pbl_audio_host_mic_ops, nullptr, nullptr), \
    .state = &sym##_host_mic_state,                                                               \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.audio.dev)

/**
 * @brief Handle a @c QemuProtocol_MicAudio message from the host.
 *
 * Queues the samples for the recording host microphone, dropping the oldest once the queue is
 * full. Ignored while it is not recording.
 *
 * @param data 16 kHz mono signed 16-bit little-endian PCM.
 * @param len Length of @p data in bytes.
 */
void qemu_mic_msg_callback(const uint8_t *data, uint32_t len);

/** @} */
