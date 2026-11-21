/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpu.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sem.h>

/**
 * @defgroup drivers_gpu_stm32_dma2d STM32 DMA2D
 * @ingroup drivers_gpu
 * @brief The Chrom-ART accelerator of the STM32: fills, copies with format conversion and
 * blending, without rotation or scaling.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_gpu_stm32_dma2d_state {
  struct pbl_sem done;
};
/** @endcond */

/** @brief STM32 DMA2D. */
struct pbl_gpu_stm32_dma2d {
  /** GPU. */
  struct pbl_gpu gpu;
  /** Driver runtime state. */
  struct pbl_gpu_stm32_dma2d_state *state;
  /** Registers. */
  uintptr_t base;
  /** Interrupt. */
  pbl_irq_t irq;
  /** Clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpu_ops pbl_gpu_stm32_dma2d_ops;
void pbl_gpu_stm32_dma2d_irq_handler(const struct pbl_gpu_stm32_dma2d *dma2d);
/** @endcond */

/**
 * @brief Define a DMA2D.
 *
 * @param sym Symbol.
 * @param _name Name.
 * @param _base Base address of the registers.
 * @param _irq Interrupt.
 * @param _clk Clock, a struct pbl_clk initializer.
 */
#define PBL_GPU_STM32_DMA2D_DEFINE(sym, _name, _base, _irq, _clk)                             \
  PBL_GPU_STATE_DEFINE(sym);                                                                  \
  static struct pbl_gpu_stm32_dma2d_state sym##_dma2d_state = {                               \
    .done = PBL_SEM_INITIALIZER(0, 1),                                                        \
  };                                                                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpu_stm32_dma2d, struct pbl_gpu, gpu){                  \
    .gpu = PBL_GPU_INIT(                                                                      \
        sym, _name, nullptr, &pbl_gpu_stm32_dma2d_ops,                                        \
        .dst_formats = PBL_BIT(PBL_GPU_FORMAT_RGB565) | PBL_BIT(PBL_GPU_FORMAT_RGB888) |      \
                       PBL_BIT(PBL_GPU_FORMAT_ARGB8888),                                      \
        .src_formats = PBL_BIT(PBL_GPU_FORMAT_RGB565) | PBL_BIT(PBL_GPU_FORMAT_RGB888) |      \
                       PBL_BIT(PBL_GPU_FORMAT_ARGB8888) | PBL_BIT(PBL_GPU_FORMAT_A8)),        \
    .state = &sym##_dma2d_state,                                                              \
    .base = (_base),                                                                          \
    .irq = (_irq),                                                                            \
    .clk = _clk,                                                                              \
  };                                                                                          \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPU_STM32_DMA2D_IRQ_PRIO, pbl_gpu_stm32_dma2d_irq_handler, \
                      &sym##_inst, 0)

/** @} */
