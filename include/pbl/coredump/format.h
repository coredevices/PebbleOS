/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup coredump_format Image format
 * @ingroup coredump
 * @brief Layout of a core dump image, parsed by @c tools/readcore.py.
 *
 * An image is a @ref pbl_coredump_image_hdr followed by chunks, each a @ref pbl_coredump_chunk_hdr
 * and a payload, up to one with the key @ref PBL_COREDUMP_CHUNK_TERMINATOR. All fields are
 * little-endian. Registers are stored in the order r0-r12, sp, lr, pc, xpsr, the same order as
 * @ref pbl_thread_reg.
 * @{
 */

/** @brief Value of @ref pbl_coredump_image_hdr::magic. */
#define PBL_COREDUMP_MAGIC 0xF00DCAFE
/** @brief Value of @ref pbl_coredump_image_hdr::version. */
#define PBL_COREDUMP_VERSION 1

/** @brief Number of core registers in a record: r0-r12, sp, lr, pc and xpsr. */
#define PBL_COREDUMP_NUM_REGISTERS 17
/** @brief Size of @ref pbl_coredump_thread::name in bytes, including the NUL terminator. */
#define PBL_COREDUMP_THREAD_NAME_SIZE 16

/** @brief Ends the image; has no payload. */
#define PBL_COREDUMP_CHUNK_TERMINATOR 0xFFFFFFFF
/** @brief Deprecated: RAM, now in @ref PBL_COREDUMP_CHUNK_MEMORY. */
#define PBL_COREDUMP_CHUNK_RAM 1
/** @brief A @ref pbl_coredump_thread. */
#define PBL_COREDUMP_CHUNK_THREAD 2
/** @brief A @ref pbl_coredump_extra_regs. */
#define PBL_COREDUMP_CHUNK_EXTRA_REG 3
/** @brief A @ref pbl_coredump_memory_hdr and the memory contents. */
#define PBL_COREDUMP_CHUNK_MEMORY 4

/** @brief Start of an image. */
struct PBL_PACKED pbl_coredump_image_hdr {
  /** @ref PBL_COREDUMP_MAGIC. */
  uint32_t magic;
  /** See @c include/pebbleos/chip_id.h. */
  uint32_t core_number : 8;
  /** @ref PBL_COREDUMP_VERSION. */
  uint32_t version : 24;
  /** Wall clock time of the dump, in seconds. */
  uint32_t time_stamp;
  /** Device serial number, NUL-terminated. */
  uint8_t serial_number[16];
  /** Firmware build ID, NUL-terminated. */
  uint8_t build_id[64];
};

/** @brief Start of a chunk. */
struct PBL_PACKED pbl_coredump_chunk_hdr {
  /** PBL_COREDUMP_CHUNK_* key. */
  uint32_t key;
  /** Size of the payload that follows. */
  uint32_t size;
};

/** @brief Start of the payload of a @ref PBL_COREDUMP_CHUNK_MEMORY chunk. */
struct PBL_PACKED pbl_coredump_memory_hdr {
  /** Address of the memory that follows. */
  uint32_t start;
};

/**
 * @brief Payload of a @ref PBL_COREDUMP_CHUNK_THREAD chunk.
 *
 * One per thread, plus an "ISR" pseudo-thread when an exception was running.
 */
struct PBL_PACKED pbl_coredump_thread {
  /** Thread name, NUL-terminated. */
  int8_t name[PBL_COREDUMP_THREAD_NAME_SIZE];
  /** Thread identifier: the thread object's address, 1 for the "ISR" pseudo-thread. */
  uint32_t id;
  /** 1 if this thread was running when the dump was taken, else 0. */
  uint8_t running;
  /** Registers r0-r12, sp, lr, pc, xpsr; those that cannot be recovered read 0xa5a5a5a5. */
  uint32_t registers[PBL_COREDUMP_NUM_REGISTERS];
};

/** @brief Payload of a @ref PBL_COREDUMP_CHUNK_EXTRA_REG chunk: special registers. */
struct PBL_PACKED pbl_coredump_extra_regs {
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
};

/** @} */
