/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/services/new_timer/new_timer.h>

/**
 * @defgroup drivers_flash Flash
 * @ingroup drivers
 * @brief NOR flash class.
 *
 * A flash is a struct pbl_flash device. Addresses are absolute: they start at the device's
 * @c base, which is where the flash is memory-mapped on SoCs that map it. Writes only clear bits,
 * so the target range must be erased first. Erases work on whole subsectors: the class erases a
 * range with as few sector and subsector erases as it can, skipping the blank ones.
 *
 * @code{.c}
 * static void prv_erased(int status, void *arg) {
 *   // Runs on the timer task: keep it short
 * }
 *
 * ret = pbl_flash_erase(FLASH, addr, len);
 * if (ret != 0) {
 *   return ret;
 * }
 *
 * ret = pbl_flash_write(FLASH, addr, data, sizeof(data));
 * if (ret != 0) {
 *   return ret;
 * }
 *
 * ret = pbl_flash_erase_async(FLASH, other_addr, other_len, prv_erased, nullptr);
 * @endcode
 *
 * Calls are serialized per device and must not be issued from ISRs. A read or write issued while
 * an erase is in progress suspends the erase, if the driver can, and the class resumes it shortly
 * after. Only one erase runs at a time: starting another one blocks until the first is done.
 *
 * Calls return 0 or a negative errno: @c -EINVAL for a range outside the flash or an unaligned
 * erase, @c -EACCES for a write or erase in the protected range and @c -ENODEV if the flash is
 * not ready. A hardware failure while reading, writing or erasing is fatal.
 *
 * After pbl_flash_coredump_init() the class runs without OS services: no locking, no timers, no
 * sleeping, and erases are synchronous. That is not reversible.
 * @{
 */

struct pbl_flash;

/**
 * @brief Completion callback of pbl_flash_erase_async().
 *
 * Runs from the timer task, or from the caller if there is nothing to erase, with no lock held:
 * it may start another erase.
 *
 * @param status 0 or a negative errno.
 * @param arg Callback argument.
 */
typedef void (*pbl_flash_erase_cb_t)(int status, void *arg);

/** @brief Flash geometry. */
struct pbl_flash_geometry {
  /** Size, in bytes. */
  uint32_t size;
  /** Program page size, in bytes. */
  uint32_t page_size;
  /** Large erase unit, in bytes. */
  uint32_t sector_size;
  /** Small erase unit, in bytes. */
  uint32_t subsector_size;
  /** Typical sector erase time, to pace erase polling. */
  uint16_t sector_erase_ms;
  /** Typical subsector erase time, to pace erase polling. */
  uint16_t subsector_erase_ms;
};

/** @brief Organization of the user one-time programmable (OTP) area. */
struct pbl_flash_otp_info {
  /** Size of each region, in bytes. */
  uint32_t region_len;
  /** Number of regions, 0 without OTP. */
  uint8_t n_regions;
};

/** @brief A named range of the OTP area. */
struct pbl_flash_otp_cell {
  /** Name. */
  const char *name;
  /** Offset in the OTP area. */
  uint32_t offset;
  /** Size, in bytes. */
  uint32_t len;
};

/** @brief Named ranges of the OTP area, as laid out by the board. */
struct pbl_flash_otp_layout {
  /** Cells. */
  const struct pbl_flash_otp_cell *cells;
  /** Number of cells. */
  uint8_t num_cells;
};

/** @brief Initializer for a struct pbl_flash_otp_cell. */
#define PBL_FLASH_OTP_CELL(_name, _offset, _len) \
  {.name = (_name), .offset = (_offset), .len = (_len)}

/** @brief Function run while the flash is powered down, see pbl_flash_power_down(). */
typedef void (*pbl_flash_pm_fn_t)(void *arg);

/**
 * @brief Flash driver operations.
 *
 * Called with the device lock held, or without any OS service in coredump mode, which drivers
 * check with pbl_flash_in_coredump(): they must then poll instead of blocking.
 */
struct pbl_flash_ops {
  /**
   * Optional. Bring the hardware up. Also called by pbl_flash_coredump_init(), possibly after a
   * previous initialization. Returns 0 or a negative errno.
   */
  int (*init)(const struct pbl_flash *flash);
  /** Read @p len bytes. Returns 0 or a negative errno. */
  int (*read)(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len);
  /** Program @p len bytes, returning once they are programmed. Returns 0 or a negative errno. */
  int (*write)(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len);
  /**
   * Start erasing the sector or subsector of @p size bytes at @p addr. Returns 0, 1 if the erase
   * completed before returning, or a negative errno. Without @c erase_status, returns once the
   * erase is done.
   */
  int (*erase)(const struct pbl_flash *flash, uint32_t addr, uint32_t size);
  /**
   * Optional. Returns 0 once the erase is done, @c -EBUSY while it is in progress, @c -EAGAIN
   * while it is suspended, or another negative errno if it failed.
   */
  int (*erase_status)(const struct pbl_flash *flash);
  /**
   * Optional, with @c erase_status. Suspend the erase in progress. Returns 0, 1 if the erase had
   * already completed, or a negative errno.
   */
  int (*erase_suspend)(const struct pbl_flash *flash);
  /** With @c erase_suspend. Resume the suspended erase. Returns 0 or a negative errno. */
  int (*erase_resume)(const struct pbl_flash *flash);
  /** Optional. Power down while @p fn runs, with interrupts disabled. */
  void (*power_down)(const struct pbl_flash *flash, pbl_flash_pm_fn_t fn, void *arg);
  /** Optional. Return the hardware to the state the boot code expects, before a reset. */
  void (*stop)(const struct pbl_flash *flash);
  /** With OTP. Read @p len bytes at @p offs of OTP region @p region. */
  int (*otp_read)(const struct pbl_flash *flash, uint8_t region, uint32_t offs, void *buf,
                  size_t len);
  /** With OTP. Program @p len bytes at @p offs of OTP region @p region. */
  int (*otp_write)(const struct pbl_flash *flash, uint8_t region, uint32_t offs, const void *buf,
                   size_t len);
  /** With OTP. Check whether OTP region @p region is locked. */
  int (*otp_is_locked)(const struct pbl_flash *flash, uint8_t region, bool *locked);
  /** Optional. Lock OTP region @p region forever. */
  int (*otp_lock)(const struct pbl_flash *flash, uint8_t region);
  /** Optional. Erase OTP region @p region. */
  int (*otp_erase)(const struct pbl_flash *flash, uint8_t region);
};

/** @cond INTERNAL_HIDDEN */
struct pbl_flash_state {
  struct pbl_mutex lock;
  struct pbl_sem erase_sem;
  bool coredump;
  bool stopped;
  struct {
    bool enabled;
    uint32_t start;
    uint32_t end;
  } protect;
  struct {
    bool in_progress;
    bool suspended;
    bool done;
    uint8_t retries;
    uint32_t next;
    uint32_t end;
    uint32_t unit;
    uint32_t expected_ms;
    struct pbl_thread *thread;
    pbl_flash_erase_cb_t cb;
    void *arg;
  } erase;
  TimerID poll_timer;
  TimerID resume_timer;
};

extern const struct pbl_device_class pbl_flash_class;
int pbl_flash_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief A flash. */
struct pbl_flash {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_flash_ops *ops;
  /** Runtime state. */
  struct pbl_flash_state *state;
  /** Address of the first byte. */
  uint32_t base;
  /** Geometry. */
  const struct pbl_flash_geometry *geometry;
  /** OTP organization, or NULL. */
  const struct pbl_flash_otp_info *otp;
  /** OTP cells, or NULL. */
  const struct pbl_flash_otp_layout *otp_layout;
};

/** @brief The board's storage flash. */
extern const struct pbl_flash *const FLASH;

/**
 * @brief Define the class state of flash @p sym, for the @c PBL_FLASH_*_DEFINE() macro of a
 * driver.
 *
 * @param sym Symbol of the flash instance.
 */
#define PBL_FLASH_STATE_DEFINE(sym)                   \
  PBL_DEVICE_STATE_DEFINE(sym);                       \
  static struct pbl_flash_state sym##_flash_state = { \
    .lock = PBL_MUTEX_INITIALIZER,                    \
    .erase_sem = PBL_SEM_INITIALIZER(1, 1),           \
  }

/**
 * @brief Initializer for the struct pbl_flash of flash @p sym.
 *
 * @param sym Symbol of the flash instance, with its state defined by PBL_FLASH_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, e.g. the memory controller, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _ops Driver operations.
 * @param _base Address of the first byte.
 * @param _geometry Geometry.
 * @param _otp OTP organization, or NULL.
 * @param _otp_layout OTP cells, or NULL.
 */
#define PBL_FLASH_INIT(sym, _name, _parent, _deps, _ops, _base, _geometry, _otp, _otp_layout)    \
  {                                                                                              \
    .dev =                                                                                       \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_flash_dev_init, _parent, _deps, &pbl_flash_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_flash_state,                                                                 \
    .base = (_base),                                                                             \
    .geometry = (_geometry),                                                                     \
    .otp = (_otp),                                                                               \
    .otp_layout = (_otp_layout),                                                                 \
  }

/**
 * @brief Check whether the flash runs in coredump mode, for drivers.
 *
 * @param flash Flash.
 * @return True after pbl_flash_coredump_init().
 */
static inline bool pbl_flash_in_coredump(const struct pbl_flash *flash) {
  return flash->state->coredump;
}

/**
 * @brief Read data.
 *
 * @param flash Flash.
 * @param addr Address.
 * @param[out] buf Buffer.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
int pbl_flash_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len);

/**
 * @brief Program data into an erased range.
 *
 * @param flash Flash.
 * @param addr Address.
 * @param buf Data.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
int pbl_flash_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len);

/**
 * @brief Erase a range.
 *
 * Feeds the caller's task watchdog while it waits, unless a single erase unit takes unreasonably
 * long.
 *
 * @param flash Flash.
 * @param addr Address, aligned to a subsector.
 * @param len Number of bytes, rounded up to a subsector.
 * @return 0 or a negative errno.
 */
int pbl_flash_erase(const struct pbl_flash *flash, uint32_t addr, size_t len);

/**
 * @brief Erase a range, returning once the erase has started.
 *
 * @param flash Flash.
 * @param addr Address, aligned to a subsector.
 * @param len Number of bytes, rounded up to a subsector.
 * @param cb Completion callback.
 * @param arg Callback argument.
 * @return 0 if @p cb will be called, or a negative errno.
 */
int pbl_flash_erase_async(const struct pbl_flash *flash, uint32_t addr, size_t len,
                          pbl_flash_erase_cb_t cb, void *arg);

/**
 * @brief Check whether a range is erased.
 *
 * @param flash Flash.
 * @param addr Address.
 * @param len Number of bytes.
 * @return True if the flash is ready and every byte reads as erased.
 */
bool pbl_flash_is_erased(const struct pbl_flash *flash, uint32_t addr, size_t len);

/**
 * @brief Refuse writes and erases in a range, replacing the previous one.
 *
 * @param flash Flash.
 * @param addr Address.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
int pbl_flash_protect(const struct pbl_flash *flash, uint32_t addr, size_t len);

/**
 * @brief Allow writes and erases everywhere.
 *
 * @param flash Flash.
 * @return 0 or a negative errno.
 */
int pbl_flash_unprotect(const struct pbl_flash *flash);

/**
 * @brief Wait for the erase in progress, if any, and return the hardware to the state the boot
 * code expects, before a reset.
 *
 * The flash then refuses every call with @c -ENODEV, until pbl_flash_coredump_init().
 *
 * @param flash Flash.
 */
void pbl_flash_stop(const struct pbl_flash *flash);

/**
 * @brief Re-initialize the flash for use from a fault handler.
 *
 * @param flash Flash.
 * @return 0 or a negative errno.
 */
int pbl_flash_coredump_init(const struct pbl_flash *flash);

/**
 * @brief Power the flash down while a function runs, with interrupts disabled.
 *
 * The flash stays powered while an erase is in progress, but @p fn always runs. When the CPU
 * executes from the flash, @p fn must run from RAM.
 *
 * @param flash Flash.
 * @param fn Function to run.
 * @param arg Argument of @p fn.
 */
void pbl_flash_power_down(const struct pbl_flash *flash, pbl_flash_pm_fn_t fn, void *arg);

/**
 * @brief CRC-32 of a range.
 *
 * @param flash Flash.
 * @param addr Address.
 * @param len Number of bytes.
 * @return CRC-32.
 */
uint32_t pbl_flash_crc32(const struct pbl_flash *flash, uint32_t addr, size_t len);

/**
 * @brief Legacy checksum of a range, as used by firmware images and resources.
 *
 * @param flash Flash.
 * @param addr Address.
 * @param len Number of bytes.
 * @return Checksum.
 */
uint32_t pbl_flash_crc32_legacy(const struct pbl_flash *flash, uint32_t addr, size_t len);

/**
 * @name One-time programmable area
 *
 * The user OTP area of drivers that select @c FLASH_OTP: @c n_regions regions of
 * @c region_len bytes, region @c i starting at offset @c i * @c region_len. OTP bits are
 * programmed like the main array, from erased (1) to 0, and a locked region can no longer be
 * programmed or erased. Locking is irreversible: only the recovery firmware builds it, and it is
 * never done implicitly. The board names the ranges it stores data in as cells.
 * @{
 */

/**
 * @brief Get the OTP organization.
 *
 * @param flash Flash.
 * @param[out] info Organization.
 * @return 0, or @c -ENOTSUP without OTP.
 */
int pbl_flash_otp_get_info(const struct pbl_flash *flash, struct pbl_flash_otp_info *info);

/**
 * @brief Read from the OTP area.
 *
 * @param flash Flash.
 * @param offs Offset in the OTP area.
 * @param[out] buf Buffer.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
int pbl_flash_otp_read(const struct pbl_flash *flash, uint32_t offs, void *buf, size_t len);

/**
 * @brief Program the OTP area.
 *
 * @param flash Flash.
 * @param offs Offset in the OTP area.
 * @param buf Data.
 * @param len Number of bytes.
 * @return 0, @c -EROFS if a region in the range is locked, or another negative errno.
 */
int pbl_flash_otp_write(const struct pbl_flash *flash, uint32_t offs, const void *buf, size_t len);

/**
 * @brief Check whether an OTP region is locked.
 *
 * @param flash Flash.
 * @param region Region.
 * @param[out] locked Lock state.
 * @return 0 or a negative errno.
 */
int pbl_flash_otp_is_locked(const struct pbl_flash *flash, uint8_t region, bool *locked);

/**
 * @brief Lock an OTP region forever.
 *
 * @param flash Flash.
 * @param region Region.
 * @return 0, @c -ENOTSUP if the flash cannot lock it, or another negative errno.
 */
int pbl_flash_otp_lock(const struct pbl_flash *flash, uint8_t region);

/**
 * @brief Erase an OTP region.
 *
 * @param flash Flash.
 * @param region Region.
 * @return 0, @c -ENOTSUP if the flash cannot erase it, @c -EROFS if the region is locked,
 * or another negative errno.
 */
int pbl_flash_otp_erase(const struct pbl_flash *flash, uint8_t region);

/**
 * @brief Find an OTP cell by name.
 *
 * @param flash Flash.
 * @param name Name.
 * @return Cell, or NULL if the board has no such cell.
 */
const struct pbl_flash_otp_cell *pbl_flash_otp_cell_get(const struct pbl_flash *flash,
                                                        const char *name);

/** @} */

/** @} */
