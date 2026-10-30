/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_spi_mem SPI memory
 * @ingroup drivers
 * @brief SPI memory controller class.
 *
 * A SPI memory controller runs operations made of a command, an address, dummy cycles and data
 * against a memory (NOR flash, PSRAM, ...) without knowing its protocol, which belongs to the
 * memory's driver. Each phase has its own bus width. A controller may not support every
 * operation, or may only carry part of the data in one go: drivers check with
 * pbl_spi_mem_supports_op() and split with pbl_spi_mem_adjust_op_size().
 *
 * @code{.c}
 * uint8_t sr;
 * struct pbl_spi_mem_op op = PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(0x05, 1), PBL_SPI_MEM_OP_NO_ADDR,
 *                                           PBL_SPI_MEM_OP_NO_DUMMY,
 *                                           PBL_SPI_MEM_OP_DATA_IN(1, &sr, 1));
 *
 * ret = pbl_spi_mem_exec_op(bus, &op);
 * @endcode
 *
 * Reads may also go through a memory-mapped window the controller sets up for a read operation the
 * memory driver picks, see pbl_spi_mem_dirmap_create(). A controller the CPU executes from is
 * flagged with @ref PBL_SPI_MEM_XIP, and its memory driver then adopts the state the boot code left
 * and runs every operation that leaves the memory busy through pbl_spi_mem_poll_status(), which
 * only returns once the memory is readable again.
 *
 * Operations are issued by the single memory on the controller, which serializes them; they
 * block until done, polling instead of sleeping once pbl_spi_mem_set_polling() is set.
 * @{
 */

struct pbl_spi_mem;

/** @brief Data direction of an operation. */
enum pbl_spi_mem_data_dir {
  /** No data phase. */
  PBL_SPI_MEM_NO_DATA,
  /** Data read from the memory. */
  PBL_SPI_MEM_DATA_IN,
  /** Data written to the memory. */
  PBL_SPI_MEM_DATA_OUT,
};

/** @brief An operation. */
struct pbl_spi_mem_op {
  /** Command phase. */
  struct {
    /** Opcode. */
    uint8_t opcode;
    /** Bus width. */
    uint8_t buswidth;
  } cmd;
  /** Address phase. */
  struct {
    /** Number of address bytes, 0 for none. */
    uint8_t nbytes;
    /** Bus width. */
    uint8_t buswidth;
    /** Address. */
    uint32_t val;
  } addr;
  /** Dummy phase. */
  struct {
    /** Number of dummy bytes, 0 for none. */
    uint8_t nbytes;
    /** Bus width. */
    uint8_t buswidth;
  } dummy;
  /** Data phase. */
  struct {
    /** Direction. */
    enum pbl_spi_mem_data_dir dir;
    /** Bus width. */
    uint8_t buswidth;
    /** Number of bytes. */
    size_t nbytes;
    /** Buffer. */
    union {
      /** Buffer to read into. */
      void *in;
      /** Data to write. */
      const void *out;
    } buf;
  } data;
};

/** @brief Command phase, for PBL_SPI_MEM_OP(). */
#define PBL_SPI_MEM_OP_CMD(_opcode, _buswidth) {.opcode = (_opcode), .buswidth = (_buswidth)}
/** @brief Address phase, for PBL_SPI_MEM_OP(). */
#define PBL_SPI_MEM_OP_ADDR(_nbytes, _val, _buswidth) \
  {.nbytes = (_nbytes), .buswidth = (_buswidth), .val = (_val)}
/** @brief No address phase. */
#define PBL_SPI_MEM_OP_NO_ADDR \
  {                            \
  }
/** @brief Dummy phase, for PBL_SPI_MEM_OP(). */
#define PBL_SPI_MEM_OP_DUMMY(_nbytes, _buswidth) {.nbytes = (_nbytes), .buswidth = (_buswidth)}
/** @brief No dummy phase. */
#define PBL_SPI_MEM_OP_NO_DUMMY \
  {                             \
  }
/** @brief Data phase reading from the memory, for PBL_SPI_MEM_OP(). */
#define PBL_SPI_MEM_OP_DATA_IN(_nbytes, _buf, _buswidth) \
  {.dir = PBL_SPI_MEM_DATA_IN, .buswidth = (_buswidth), .nbytes = (_nbytes), .buf.in = (_buf)}
/** @brief Data phase writing to the memory, for PBL_SPI_MEM_OP(). */
#define PBL_SPI_MEM_OP_DATA_OUT(_nbytes, _buf, _buswidth) \
  {.dir = PBL_SPI_MEM_DATA_OUT, .buswidth = (_buswidth), .nbytes = (_nbytes), .buf.out = (_buf)}
/** @brief No data phase. */
#define PBL_SPI_MEM_OP_NO_DATA \
  {                            \
  }

/** @brief Build an operation from its phases. */
#define PBL_SPI_MEM_OP(_cmd, _addr, _dummy, _data)              \
  (struct pbl_spi_mem_op) {                                     \
    .cmd = _cmd, .addr = _addr, .dummy = _dummy, .data = _data, \
  }

/** @brief A memory-mapped read window, see pbl_spi_mem_dirmap_create(). */
struct pbl_spi_mem_dirmap {
  /** Read operation; its address value and data length are ignored. */
  struct pbl_spi_mem_op op;
  /** First memory address of the window. */
  uint32_t offset;
  /** Size of the window, which the controller may reduce. */
  uint32_t length;
  /** Set when the controller maps the window, cleared when reads fall back to operations. */
  bool mapped;
};

/** @brief Memory commands that put it in and out of its lowest power state. */
struct pbl_spi_mem_pm {
  /** Command entering the low power state. */
  struct pbl_spi_mem_op enter;
  /** Time the memory takes to enter it, in microseconds. */
  uint16_t enter_us;
  /** Command leaving the low power state. */
  struct pbl_spi_mem_op exit;
  /** Time the memory takes to leave it, in microseconds. */
  uint16_t exit_us;
};

/**
 * @brief Memory commands that suspend and resume a long operation, see pbl_spi_mem_poll_status().
 */
struct pbl_spi_mem_suspend {
  /** Command suspending the operation. */
  struct pbl_spi_mem_op suspend;
  /** Command resuming it. */
  struct pbl_spi_mem_op resume;
  /** Longest time the memory takes to suspend, in microseconds. */
  uint16_t suspend_us;
  /** Shortest time between a resume and the next suspend, in microseconds. */
  uint16_t resume_us;
};

/** @brief Function run while the memory is powered down, see pbl_spi_mem_power_down(). */
typedef void (*pbl_spi_mem_pm_fn_t)(void *arg);

/** @brief Controller driver operations. */
struct pbl_spi_mem_ops {
  /** Optional. Bring the controller up. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_spi_mem *mem);
  /** Check whether the controller can run @p op. */
  bool (*supports_op)(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op);
  /** Optional. Reduce the data of @p op to what one run carries. Returns 0 or a negative errno. */
  int (*adjust_op_size)(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op);
  /** Run a supported operation. Returns 0 or a negative errno. */
  int (*exec_op)(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op);
  /**
   * Optional, required with @ref PBL_SPI_MEM_XIP. Run @p op, then @p status until its first data
   * byte masked with @p mask equals @p match, letting nothing else reach the memory in between.
   * With @p susp, the controller may suspend the operation meanwhile to serve interrupts. Returns
   * 0 or a negative errno.
   */
  int (*poll_status)(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op,
                     const struct pbl_spi_mem_op *status, uint8_t mask, uint8_t match,
                     const struct pbl_spi_mem_suspend *susp);
  /**
   * Optional. Set up a memory-mapped read window for @p desc, reducing @c desc->length to what
   * the controller maps. Returns 0, or @c -ENOTSUP to fall back to operations.
   */
  int (*dirmap_create)(const struct pbl_spi_mem *mem, struct pbl_spi_mem_dirmap *desc);
  /** With @c dirmap_create. Read through the window. Returns 0 or a negative errno. */
  int (*dirmap_read)(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                     uint32_t offs, void *buf, size_t len);
  /** Optional, with @c dirmap_create. Drop cached copies of a range of the window. */
  void (*dirmap_invalidate)(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                            uint32_t offs, size_t len);
  /**
   * Optional. Run @p pm->enter, if @p pm is not NULL, power the controller down, call @p fn, then
   * undo it all, with interrupts disabled. With @ref PBL_SPI_MEM_XIP the memory is unreadable in
   * between: nothing but @p fn may run from it, and @p pm must be in RAM.
   */
  void (*power_down)(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_pm *pm,
                     pbl_spi_mem_pm_fn_t fn, void *arg);
};

/**
 * @brief The CPU executes from the memory on this controller.
 *
 * Its memory driver adopts the state the boot code left instead of resetting the memory, and runs
 * the operations that leave the memory busy through pbl_spi_mem_poll_status().
 */
#define PBL_SPI_MEM_XIP PBL_BIT(0)

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_mem_state {
  bool polling;
};

extern const struct pbl_device_class pbl_spi_mem_class;
int pbl_spi_mem_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief A SPI memory controller. */
struct pbl_spi_mem {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_spi_mem_ops *ops;
  /** Runtime state. */
  struct pbl_spi_mem_state *state;
  /** Flags, @c PBL_SPI_MEM_*. */
  uint8_t flags;
};

/**
 * @brief Define the class state of controller @p sym, for the @c PBL_SPI_MEM_*_DEFINE() macro of
 * a driver.
 *
 * @param sym Symbol of the controller instance.
 */
#define PBL_SPI_MEM_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);       \
  static struct pbl_spi_mem_state sym##_spi_mem_state

/**
 * @brief Initializer for the struct pbl_spi_mem of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_SPI_MEM_STATE_DEFINE().
 * @param _name Name.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _ops Driver operations.
 * @param _flags Flags, @c PBL_SPI_MEM_*.
 */
#define PBL_SPI_MEM_INIT(sym, _name, _deps, _ops, _flags)                          \
  {                                                                                \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_spi_mem_dev_init, nullptr, _deps, \
                                 &pbl_spi_mem_class),                              \
    .ops = (_ops),                                                                 \
    .state = &sym##_spi_mem_state,                                                 \
    .flags = (_flags),                                                             \
  }

/**
 * @brief Check whether the CPU executes from the memory on a controller.
 *
 * @param mem Controller.
 * @return True with @ref PBL_SPI_MEM_XIP.
 */
static inline bool pbl_spi_mem_is_xip(const struct pbl_spi_mem *mem) {
  return (mem->flags & PBL_SPI_MEM_XIP) != 0U;
}

/**
 * @brief Check whether the controller can run an operation.
 *
 * @param mem Controller.
 * @param op Operation.
 * @return True if pbl_spi_mem_exec_op() accepts it.
 */
bool pbl_spi_mem_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op);

/**
 * @brief Reduce the data of an operation to what one run carries.
 *
 * @param mem Controller.
 * @param op Operation, whose @c data.nbytes may be reduced.
 * @return 0 or a negative errno.
 */
int pbl_spi_mem_adjust_op_size(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op);

/**
 * @brief Run an operation.
 *
 * @param mem Controller.
 * @param op Operation.
 * @return 0, @c -ENOTSUP if the controller cannot run it, or another negative errno.
 */
int pbl_spi_mem_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op);

/**
 * @brief Run an operation, then poll a status register until it matches.
 *
 * Nothing else reaches the memory until the status matches, so this is how operations that leave
 * the memory busy (programs, erases, register writes) are run on a @ref PBL_SPI_MEM_XIP
 * controller.
 *
 * @param mem Controller.
 * @param op Operation.
 * @param status Operation reading the status into its first data byte.
 * @param mask Status bits to check.
 * @param match Value of the masked status bits to wait for.
 * @param susp Commands suspending and resuming @p op, or NULL. A controller that would otherwise
 * hold off interrupts until the status matches suspends @p op with them to let interrupts run.
 * @return 0, @c -ENOTSUP if the controller cannot do it, leaving the polling to the caller, or
 * another negative errno.
 */
int pbl_spi_mem_poll_status(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op,
                            const struct pbl_spi_mem_op *status, uint8_t mask, uint8_t match,
                            const struct pbl_spi_mem_suspend *susp);

/**
 * @brief Create a memory-mapped read window.
 *
 * The controller may map less than asked: reads beyond @c desc->length after the call are up to
 * the caller. Controllers without a window carry the reads out as operations, through the whole
 * requested range.
 *
 * @param mem Controller.
 * @param[in,out] desc Window: read operation and requested range in, mapped range out.
 * @return 0, @c -ENOTSUP if the controller cannot run the read operation, or another negative
 * errno.
 */
int pbl_spi_mem_dirmap_create(const struct pbl_spi_mem *mem, struct pbl_spi_mem_dirmap *desc);

/**
 * @brief Read through a memory-mapped read window.
 *
 * @param mem Controller.
 * @param desc Window, from pbl_spi_mem_dirmap_create().
 * @param offs Memory address.
 * @param[out] buf Buffer.
 * @param len Number of bytes.
 * @return 0, @c -EINVAL for a range outside the window, or another negative errno.
 */
int pbl_spi_mem_dirmap_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                            uint32_t offs, void *buf, size_t len);

/**
 * @brief Drop cached copies of a range of a window, after programming or erasing it.
 *
 * @param mem Controller.
 * @param desc Window, from pbl_spi_mem_dirmap_create().
 * @param offs Memory address.
 * @param len Number of bytes.
 */
void pbl_spi_mem_dirmap_invalidate(const struct pbl_spi_mem *mem,
                                   const struct pbl_spi_mem_dirmap *desc, uint32_t offs,
                                   size_t len);

/**
 * @brief Power the memory and the controller down while a function runs, with interrupts
 * disabled.
 *
 * @param mem Controller.
 * @param pm Memory low power commands, or NULL to leave the memory as is. Must be in RAM with
 * @ref PBL_SPI_MEM_XIP.
 * @param fn Function to run, from RAM with @ref PBL_SPI_MEM_XIP.
 * @param arg Argument of @p fn.
 */
void pbl_spi_mem_power_down(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_pm *pm,
                            pbl_spi_mem_pm_fn_t fn, void *arg);

/**
 * @brief Poll for completion instead of sleeping, e.g. from a fault handler.
 *
 * @param mem Controller.
 * @param polling True to poll.
 */
void pbl_spi_mem_set_polling(const struct pbl_spi_mem *mem, bool polling);

/**
 * @brief Check whether the controller polls for completion, for drivers.
 *
 * @param mem Controller.
 * @return True if polling.
 */
static inline bool pbl_spi_mem_is_polling(const struct pbl_spi_mem *mem) {
  return mem->state->polling;
}

/** @} */
