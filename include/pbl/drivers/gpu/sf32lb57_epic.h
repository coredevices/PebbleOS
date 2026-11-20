/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpu.h>
#include <pbl/drivers/reset.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sem.h>

/**
 * @defgroup drivers_gpu_sf32lb57_epic SF32LB57 ePIC
 * @ingroup drivers_gpu
 * @brief @ref drivers_gpu driver for the ePIC graphics accelerator of the SF32LB57.
 *
 * Draws into @ref PBL_GPU_FORMAT_RGB565 and @ref PBL_GPU_FORMAT_RGB888 buffers, from every
 * format, rotated and scaled within 512 pixels of the area drawn. The target is read back as a
 * layer under the source, so a blit blends in place, and what it leaves out of the source is not
 * touched. Its clock runs only while a job does. Short
 * jobs are waited for in place, longer ones on the interrupt of the accelerator, which an
 * instance connects itself, with priority @c CONFIG_GPU_SF32LB57_EPIC_IRQ_PRIO.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_gpu_sf32lb57_epic_state {
  struct pbl_sem done;
};
/** @endcond */

/** @brief SF32LB57 ePIC. */
struct pbl_gpu_sf32lb57_epic {
  /** GPU. */
  struct pbl_gpu gpu;
  /** Driver runtime state. */
  struct pbl_gpu_sf32lb57_epic_state *state;
  /** Registers. */
  uintptr_t base;
  /** Interrupt. */
  pbl_irq_t irq;
  /** Clock. */
  struct pbl_clk clk;
  /** Reset. */
  struct pbl_reset rst;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpu_ops pbl_gpu_sf32lb57_epic_ops;
/** @endcond */

/**
 * @brief Define an SF32LB57 ePIC.
 *
 * @param sym Symbol of the GPU.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt.
 * @param _clk Clock, from PBL_CLK().
 * @param _rst Reset, from PBL_RESET().
 */
#define PBL_GPU_SF32LB57_EPIC_DEFINE(sym, _name, _base, _irq, _clk, _rst)                         \
  PBL_GPU_STATE_DEFINE(sym);                                                                      \
  static struct pbl_gpu_sf32lb57_epic_state sym##_epic_state = {                                  \
    .done = PBL_SEM_INITIALIZER(0, 1),                                                            \
  };                                                                                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpu_sf32lb57_epic, struct pbl_gpu, gpu){                    \
    .gpu = PBL_GPU_INIT(                                                                          \
        sym, _name, nullptr, &pbl_gpu_sf32lb57_epic_ops,                                          \
        .dst_formats = PBL_BIT(PBL_GPU_FORMAT_RGB565) | PBL_BIT(PBL_GPU_FORMAT_RGB888),           \
        .src_formats = PBL_BIT(PBL_GPU_FORMAT_RGB565) | PBL_BIT(PBL_GPU_FORMAT_RGB888) |          \
                       PBL_BIT(PBL_GPU_FORMAT_ARGB8888) | PBL_BIT(PBL_GPU_FORMAT_A8),             \
        .flags = PBL_GPU_CAP_ROTATE | PBL_GPU_CAP_SCALE),                                         \
    .state = &sym##_epic_state,                                                                   \
    .base = (_base),                                                                              \
    .irq = (_irq),                                                                                \
    .clk = _clk,                                                                                  \
    .rst = _rst,                                                                                  \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPU_SF32LB57_EPIC_IRQ_PRIO, pbl_gpu_sf32lb57_epic_irq_handler, \
                      &sym##_inst, 0)

/**
 * @brief Interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param epic GPU.
 */
void pbl_gpu_sf32lb57_epic_irq_handler(const struct pbl_gpu_sf32lb57_epic *epic);

/** @} */
