/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

/**
 * @defgroup pebbleos_core_dump_structs Core dump records
 * @ingroup pebbleos
 * @brief Register records stored in a core dump.
 *
 * A core dump is a stream of chunks, each a 32-bit key and a 32-bit payload size followed by the
 * payload. These are the payloads of the thread and extra register chunks, shared with the
 * Bluetooth controller's core dump code and parsed by @c tools/readcore.py. All fields are
 * little-endian. Registers are stored in the order r0-r12, sp, lr, pc, xpsr, the same order as
 * @ref pbl_thread_reg.
 * @{
 */

/** @brief Number of core registers in a record: r0-r12, sp, lr, pc and xpsr. */
#define CORE_DUMP_NUM_REGISTERS 17

/** @brief Size of @ref CoreDumpThreadInfo::name in bytes, including the NUL terminator. */
#define CORE_DUMP_THREAD_NAME_SIZE 16

/**
 * @brief Payload of a thread chunk.
 *
 * One per thread, plus an "ISR" pseudo-thread when the dump was triggered from an exception
 * handler.
 */
typedef struct PBL_PACKED {
  /** Thread name, NUL-terminated. */
  int8_t name[CORE_DUMP_THREAD_NAME_SIZE];
  /** Thread identifier: the thread object's address, 1 for the "ISR" pseudo-thread. */
  uint32_t id;
  /** 1 if this thread was running when the dump was taken, else 0. */
  uint8_t running;
  /**
   * Registers r0-r12, sp, lr, pc, xpsr. For the running thread interrupted by an exception,
   * only the hardware-stacked registers and sp are valid; the others read 0xa5a5a5a5.
   */
  uint32_t registers[CORE_DUMP_NUM_REGISTERS];
} CoreDumpThreadInfo;

/** @brief Payload of the extra registers chunk: special registers at dump time. */
typedef struct PBL_PACKED {
  /** Main stack pointer. */
  uint32_t msp;
  /** Process stack pointer. */
  uint32_t psp;
  /** PRIMASK register. */
  uint8_t primask;
  /** BASEPRI register. */
  uint8_t basepri;
  /** FAULTMASK register. */
  uint8_t faultmask;
  /** CONTROL register. */
  uint8_t control;
} CoreDumpExtraRegInfo;

/**
 * @brief Processor state captured on entry to the core dump handler.
 *
 * Saved by the NMI handler that performs the core dump, into a static variable, before any C code
 * runs; the handler's assembly depends on the order and packing of this structure. Not written
 * to the dump as such, but reachable in RAM through the @c s_saved_registers symbol.
 */
typedef struct PBL_PACKED {
  /**
   * Registers r0-r12, sp, lr, pc, xpsr. sp is the MSP and lr the EXC_RETURN value at handler
   * entry; pc is the handler's own address.
   */
  uint32_t core_reg[CORE_DUMP_NUM_REGISTERS];
  /** Special registers. */
  CoreDumpExtraRegInfo extra_reg;
} CoreDumpSavedRegisters;

/** @} */
