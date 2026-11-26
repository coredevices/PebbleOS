/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/pdm/pdm.h>

/**
 * @defgroup drivers_audio_stm32_sai STM32 SAI
 * @ingroup drivers_audio
 * @brief DAI on a sub-block of an STM32 serial audio interface.
 *
 * For playback, the sub-block is the master of an I2S link: two 32-bit slots of 16-bit samples,
 * a bit clock of 64 times the rate, and a master clock of 256 times the rate for the codec,
 * divided from its kernel clock. A mono stream is sent on both slots.
 *
 * For capture, sub-block A takes a pair of PDM microphones on its PDM interface, of a card of
 * format pdm: the left one samples on the rising edge of the clock, the right one on the falling
 * edge, and a mono stream takes the left one. The clock runs at 64 times the rate, 128 times
 * at 8 kHz, and @ref pdm decimates the stream to the rate in software, with a gain of 0 to
 * 42 dB. The first 100 ms, while the microphones settle, are silence. The DMA controller fills a
 * ring of raw samples, which must be in memory it reaches: the memory region of the sub-block, as
 * the SRAM4 for the BDMA of SAI4, or else the heap.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_stm32_sai_state {
  struct pbl_audio_stream stream;
  struct pbl_pdm_filter *filters;
  uint8_t *ring;
  size_t half_bytes;
  uint32_t decimation;
  size_t frame;
  uint8_t half;
  uint8_t period;
  bool ring_allocated;
  bool mute;
};
/** @endcond */

/** @brief STM32 SAI sub-block. */
struct pbl_audio_stm32_sai {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Direction of the sub-block, one of @ref pbl_audio_dir. */
  uint8_t dir;
  /** Registers of the sub-block. */
  uintptr_t base;
  /** Common registers of the SAI. */
  uintptr_t common;
  /** Kernel clock. */
  struct pbl_clk clk;
  /** DMA channel. */
  const struct pbl_dma_chan *dma;
  /** Pins. */
  const struct pbl_pinctrl *pinctrl;
  /** Memory for the raw capture ring, or 0 to take it from the heap. */
  uintptr_t ring;
  /** Size of @ref ring, in bytes. */
  size_t ring_size;
  /** Runtime state. */
  struct pbl_audio_stm32_sai_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_stm32_sai_ops;
/** @endcond */

/**
 * @brief Define a SAI sub-block as a DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Registers of the sub-block.
 * @param _common Common registers of the SAI.
 * @param _clk Kernel clock, from PBL_CLK().
 * @param _tx Playback DMA channel, or NULL.
 * @param _rx Capture DMA channel, or NULL.
 * @param _dir Direction, one of @ref pbl_audio_dir.
 * @param _pinctrl Default state of the pins.
 * @param _ring Memory for the raw capture ring, or 0 to take it from the heap.
 * @param _ring_size Size of @p _ring, in bytes.
 */
#define PBL_AUDIO_STM32_SAI_DEFINE(sym, _name, _base, _common, _clk, _tx, _rx, _dir, _pinctrl, \
                                   _ring, _ring_size)                                          \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                             \
  static struct pbl_audio_stm32_sai_state sym##_sai_state;                                     \
  static const struct pbl_pinctrl sym##_sai_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_stm32_sai, struct pbl_audio_dai, dai) {            \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, nullptr, &pbl_audio_stm32_sai_ops), .dir = (_dir),   \
    .base = (_base), .common = (_common), .clk = _clk,                                         \
    .dma = ((_dir) == PBL_AUDIO_PLAYBACK) ? (_tx) : (_rx), .pinctrl = &sym##_sai_pins,         \
    .ring = (_ring), .ring_size = (_ring_size), .state = &sym##_sai_state,                     \
  }

/** @} */
