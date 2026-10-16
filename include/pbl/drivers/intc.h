/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_intc Interrupt controller
 * @ingroup drivers
 * @brief Secondary interrupt controller class.
 *
 * A secondary interrupt controller, a GPIO port or a peripheral that funnels several interrupt
 * sources into one SoC line, is a struct pbl_intc device. SoC lines themselves are bound at
 * build time with PBL_IRQ_CONNECT() (see @ref kernel_irq) and are not part of this class.
 *
 * A consumer holds a struct pbl_irq_spec naming the controller, the line on it and its trigger
 * type, as wired on the board, and lists the controller among its device dependencies. It
 * requests the line once and enables it while it wants interrupts:
 *
 * @code{.c}
 * .int1 = PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, 38, PBL_IRQ_TYPE_EDGE_RISING),
 *
 * static void prv_int1_handler(void *arg) {
 *   // ISR context
 * }
 *
 * pbl_intc_request(&dev->int1, prv_int1_handler, dev);
 * pbl_intc_enable(&dev->int1);
 * @endcode
 *
 * The controller driver calls pbl_intc_dispatch() from its SoC interrupt handler for each
 * pending line.
 * @{
 */

/**
 * @name Trigger types
 * Physical trigger of a line, as wired on the board.
 * @{
 */
/** Rising edge. */
#define PBL_IRQ_TYPE_EDGE_RISING 1U
/** Falling edge. */
#define PBL_IRQ_TYPE_EDGE_FALLING 2U
/** Both edges. */
#define PBL_IRQ_TYPE_EDGE_BOTH (PBL_IRQ_TYPE_EDGE_RISING | PBL_IRQ_TYPE_EDGE_FALLING)
/** High level. */
#define PBL_IRQ_TYPE_LEVEL_HIGH 4U
/** Low level. */
#define PBL_IRQ_TYPE_LEVEL_LOW 8U
/** @} */

/** @brief Line handler, called in ISR context. */
typedef void (*pbl_intc_handler_t)(void *arg);

struct pbl_intc;

/** @brief Class of the interrupt controllers. */
extern const struct pbl_device_class pbl_intc_class;

/** @brief Interrupt controller driver operations, called with interrupts locked. */
struct pbl_intc_ops {
  /** Set the trigger type of a disabled line. Returns 0, @c -ENOTSUP or a negative errno. */
  int (*configure)(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type);
  /** Unmask a line. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_intc *intc, uint32_t hwirq);
  /** Mask a line. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_intc *intc, uint32_t hwirq);
};

/** @brief Runtime state of a line. */
struct pbl_intc_line {
  /** Handler, NULL if the line is free. */
  pbl_intc_handler_t handler;
  /** Handler argument. */
  void *arg;
  /** Trigger type. */
  uint8_t type;
  /** Whether the line is enabled. */
  bool enabled;
};

/** @brief An interrupt controller. */
struct pbl_intc {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_intc_ops *ops;
  /** State of each line. */
  struct pbl_intc_line *lines;
  /** Number of lines. */
  uint16_t num_irqs;
};

/**
 * @brief Define the class state of interrupt controller @p sym, for the driver's define macro.
 *
 * @param sym Symbol of the controller instance.
 * @param _num_irqs Number of lines.
 */
#define PBL_INTC_STATE_DEFINE(sym, _num_irqs) \
  PBL_DEVICE_STATE_DEFINE(sym);               \
  static struct pbl_intc_line sym##_intc_lines[_num_irqs]

/**
 * @brief Initializer for the struct pbl_intc of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_INTC_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_INTC_INIT(sym, _name, _init, _parent, _ops)                                 \
  {                                                                                     \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, nullptr, &pbl_intc_class), \
    .ops = (_ops),                                                                      \
    .lines = sym##_intc_lines,                                                          \
    .num_irqs = ARRAY_LENGTH(sym##_intc_lines),                                         \
  }

/** @brief An interrupt line, as used by a consumer. */
struct pbl_irq_spec {
  /** Controller, NULL if not connected. */
  const struct pbl_intc *intc;
  /** Line number on the controller. */
  uint16_t hwirq;
  /** Trigger type. */
  uint8_t type;
};

/**
 * @brief Initializer for a struct pbl_irq_spec.
 *
 * @param _intc Interrupt controller.
 * @param _hwirq Line number on the controller.
 * @param _type Trigger type.
 */
#define PBL_IRQ_SPEC(_intc, _hwirq, _type) {.intc = (_intc), .hwirq = (_hwirq), .type = (_type)}

/**
 * @brief Check whether an interrupt line is connected.
 *
 * @param spec Line.
 * @return True if the line has a controller.
 */
static inline bool pbl_irq_spec_is_connected(const struct pbl_irq_spec *spec) {
  return spec->intc != nullptr;
}

/**
 * @brief Install the handler of a line and set its trigger type.
 *
 * The line is left disabled.
 *
 * @param spec Line, on a ready controller.
 * @param handler Handler, called in ISR context.
 * @param arg Handler argument.
 * @return 0, @c -EBUSY if the line is already requested, @c -EINVAL for an invalid line or type,
 * @c -ENOTSUP if the controller does not support the type, or another negative errno.
 */
int pbl_intc_request(const struct pbl_irq_spec *spec, pbl_intc_handler_t handler, void *arg);

/**
 * @brief Disable a line and remove its handler.
 *
 * @param spec Requested line.
 * @return 0, @c -EINVAL if the line is not requested, or another negative errno.
 */
int pbl_intc_free(const struct pbl_irq_spec *spec);

/**
 * @brief Enable a requested line.
 *
 * @param spec Requested line.
 * @return 0, @c -EINVAL if the line is not requested, or another negative errno.
 */
int pbl_intc_enable(const struct pbl_irq_spec *spec);

/**
 * @brief Disable a requested line.
 *
 * @param spec Requested line.
 * @return 0, @c -EINVAL if the line is not requested, or another negative errno.
 */
int pbl_intc_disable(const struct pbl_irq_spec *spec);

/**
 * @brief Run the handler of a pending line, for the controller driver.
 *
 * A line without a handler is disabled.
 *
 * @param intc Controller.
 * @param hwirq Pending line.
 */
void pbl_intc_dispatch(const struct pbl_intc *intc, uint32_t hwirq);

/** @} */
