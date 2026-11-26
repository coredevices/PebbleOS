/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/canopen/canopen.h>
#include <pbl/util/bits.h>

/**
 * @defgroup subsys_canopen_cia402 CiA 402 drive
 * @ingroup subsys_canopen
 * @brief Drive profile (CiA 402) on a CANopen node.
 *
 * A drive runs the CiA 402 state machine of a node whose object dictionary has a @c cia402
 * section: the controlword (0x6040) the master writes moves it from state to state, and the
 * statusword (0x6041) reports it. The application drives the motor from its callbacks, all called
 * with the node locked:
 *
 * @code{.c}
 * static int prv_enable(struct pbl_cia402 *drive, void *arg) {
 *   motor_enable();
 *   return 0;
 * }
 *
 * static const struct pbl_cia402_ops s_ops = {.enable = prv_enable, .disable = prv_disable};
 *
 * ret = pbl_canopen_init(&s_co, &cfg);
 * ret = pbl_cia402_init(&s_drive, &s_co, &s_ops, nullptr);
 * ret = pbl_canopen_start(&s_co);
 * @endcode
 *
 * The motor follows its targets, e.g. 0x607a or 0x60ff, while the drive is operation enabled. The
 * functions lock the node, so they are called from a thread or a callback, never an interrupt.
 * @{
 */

/** @brief Controlword. */
#define PBL_CIA402_CONTROLWORD 0x6040U
/** @brief Statusword. */
#define PBL_CIA402_STATUSWORD 0x6041U
/** @brief Modes of operation. */
#define PBL_CIA402_MODES_OF_OPERATION 0x6060U
/** @brief Modes of operation display. */
#define PBL_CIA402_MODES_OF_OPERATION_DISPLAY 0x6061U

/**
 * @name Statusword bits the application sets
 * @{
 */
/** Warning. */
#define PBL_CIA402_SW_WARNING PBL_BIT(7)
/** Target reached. */
#define PBL_CIA402_SW_TARGET_REACHED PBL_BIT(10)
/** Internal limit active. */
#define PBL_CIA402_SW_INTERNAL_LIMIT PBL_BIT(11)
/** Operation mode specific bits. */
#define PBL_CIA402_SW_MODE_SPECIFIC (PBL_BIT(12) | PBL_BIT(13))
/** Manufacturer specific bits. */
#define PBL_CIA402_SW_MANUFACTURER (PBL_BIT(14) | PBL_BIT(15))
/** @} */

/** @brief Modes of operation. */
enum pbl_cia402_mode {
  PBL_CIA402_MODE_NONE = 0,
  PBL_CIA402_MODE_PROFILE_POSITION = 1,
  PBL_CIA402_MODE_PROFILE_VELOCITY = 3,
  PBL_CIA402_MODE_PROFILE_TORQUE = 4,
  PBL_CIA402_MODE_CYCLIC_POSITION = 8,
  PBL_CIA402_MODE_CYCLIC_VELOCITY = 9,
  PBL_CIA402_MODE_CYCLIC_TORQUE = 10,
};

/** @brief States of the drive. */
enum pbl_cia402_state {
  PBL_CIA402_NOT_READY_TO_SWITCH_ON,
  PBL_CIA402_SWITCH_ON_DISABLED,
  PBL_CIA402_READY_TO_SWITCH_ON,
  PBL_CIA402_SWITCHED_ON,
  PBL_CIA402_OPERATION_ENABLED,
  PBL_CIA402_QUICK_STOP_ACTIVE,
  PBL_CIA402_FAULT_REACTION_ACTIVE,
  PBL_CIA402_FAULT,
};

struct pbl_cia402;

/**
 * @brief Drive callbacks, each optional.
 *
 * The power stage is on in switched on, operation enabled, quick stop active and fault reaction
 * active; the motor is enabled, following its targets, in operation enabled, and still energized
 * in quick stop active and fault reaction active, as their ramps run.
 */
struct pbl_cia402_ops {
  /** Turn the power stage on, without driving the motor. Returns 0 or a negative errno. */
  int (*switch_on)(struct pbl_cia402 *drive, void *arg);
  /** Turn the power stage off. */
  void (*switch_off)(struct pbl_cia402 *drive, void *arg);
  /** Enable the motor, to follow its targets. Returns 0 or a negative errno. */
  int (*enable)(struct pbl_cia402 *drive, void *arg);
  /** Disable the motor, as the disable operation option code (0x605c) says. */
  void (*disable)(struct pbl_cia402 *drive, void *arg);
  /**
   * Stop the motor as the quick stop option code (0x605a) says, then call
   * pbl_cia402_quick_stop_done(). Without it, the quick stop is done at once.
   */
  void (*quick_stop)(struct pbl_cia402 *drive, void *arg);
  /**
   * React to a fault as the fault reaction option code (0x605e) says, then call
   * pbl_cia402_fault_reaction_done(). Without it, the reaction is done at once.
   */
  void (*fault)(struct pbl_cia402 *drive, uint16_t code, void *arg);
  /**
   * Change the mode of operation, one the drive supports (0x6502). Returns 0, or a negative errno
   * to refuse it.
   */
  int (*set_mode)(struct pbl_cia402 *drive, enum pbl_cia402_mode mode, void *arg);
};

/** @brief A drive. */
struct pbl_cia402 {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_canopen *co;
  const struct pbl_cia402_ops *ops;
  void *arg;
  enum pbl_cia402_state state;
  uint16_t controlword;
  uint16_t status;
  uint16_t fault_code;
  bool powered;
  bool enabled;
  /** @endcond */
};

/**
 * @brief Attach a drive to a node, initialized and not started, in switch on disabled.
 *
 * @param drive Drive.
 * @param co Node.
 * @param ops Callbacks, or NULL.
 * @param arg Callback argument.
 * @return 0, or @c -ENOENT if the node's object dictionary has no drive.
 */
int pbl_cia402_init(struct pbl_cia402 *drive, struct pbl_canopen *co,
                    const struct pbl_cia402_ops *ops, void *arg);

/**
 * @brief Get the state of a drive.
 *
 * @param drive Drive.
 * @return State.
 */
enum pbl_cia402_state pbl_cia402_state(struct pbl_cia402 *drive);

/**
 * @brief Get the mode of operation of a drive.
 *
 * @param drive Drive.
 * @return Mode, as the modes of operation display (0x6061) shows it.
 */
enum pbl_cia402_mode pbl_cia402_mode(struct pbl_cia402 *drive);

/**
 * @brief Report a fault: the drive goes to fault reaction active, sets the error code (0x603f)
 * and raises an emergency.
 *
 * @param drive Drive.
 * @param code Error code, e.g. 0x2310 for a continuous over-current.
 * @return 0, or @c -EALREADY if the drive is in fault.
 */
int pbl_cia402_fault(struct pbl_cia402 *drive, uint16_t code);

/**
 * @brief End the fault reaction: the drive goes to fault, until the master resets it.
 *
 * @param drive Drive.
 */
void pbl_cia402_fault_reaction_done(struct pbl_cia402 *drive);

/**
 * @brief End a quick stop: the drive goes to switch on disabled, or stays in quick stop active
 * with quick stop option codes 5 to 8.
 *
 * @param drive Drive.
 */
void pbl_cia402_quick_stop_done(struct pbl_cia402 *drive);

/**
 * @brief Set statusword bits the application owns, @c PBL_CIA402_SW_* ones.
 *
 * @param drive Drive.
 * @param mask Bits to change.
 * @param bits Their values.
 */
void pbl_cia402_set_status(struct pbl_cia402 *drive, uint16_t mask, uint16_t bits);

/** @} */
