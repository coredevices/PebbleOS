/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pbl/kernel/mutex.h"
#include "pbl/kernel/sem.h"
#include "pbl/services/new_timer/new_timer.h"

struct pbl_flash_device;

typedef void (*pbl_flash_erase_cb_t)(void *ctx, int status);

struct pbl_flash_geometry {
  uint32_t size;
  uint32_t page_size;
  //! Large and small erase units.
  uint32_t sector_size;
  uint32_t subsector_size;
  //! Typical erase durations, used to pace polling of asynchronous erases.
  uint16_t sector_erase_ms;
  uint16_t subsector_erase_ms;
};

//! One-time-programmable "security registers" of NOR parts that have them.
struct pbl_flash_sec_regs {
  const uint32_t *addrs;
  uint8_t count;
  uint16_t size;
};

//! Driver interface. Called with the device lock held (or from coredump/idle
//! context without any OS services); drivers must not block on OS primitives
//! when @ref pbl_flash_device_state::coredump is set.
struct pbl_flash_ops {
  int (*init)(const struct pbl_flash_device *dev);
  int (*read)(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len);
  int (*write)(const struct pbl_flash_device *dev, uint32_t addr, const void *buf, size_t len);
  //! Erase @p size bytes (sector_size or subsector_size) at @p addr. When
  //! erase_status is NULL the erase completes before returning.
  int (*erase_begin)(const struct pbl_flash_device *dev, uint32_t addr, size_t size);
  //! @return 0 done, -EBUSY in progress, -EAGAIN suspended, other errors failed.
  int (*erase_status)(const struct pbl_flash_device *dev);
  //! @return 0 suspended, 1 if the erase had already completed. Optional.
  int (*erase_suspend)(const struct pbl_flash_device *dev);
  int (*erase_resume)(const struct pbl_flash_device *dev);
  //! Called with interrupts disabled around MCU stop mode. Optional.
  void (*power_down)(const struct pbl_flash_device *dev);
  void (*power_up)(const struct pbl_flash_device *dev);
  int (*sec_reg_read)(const struct pbl_flash_device *dev, uint32_t addr, uint8_t *val);
  int (*sec_reg_write)(const struct pbl_flash_device *dev, uint32_t addr, uint8_t val);
  int (*sec_reg_erase)(const struct pbl_flash_device *dev, uint32_t addr);
  int (*sec_reg_is_locked)(const struct pbl_flash_device *dev, uint32_t addr, bool *locked);
  int (*sec_reg_lock)(const struct pbl_flash_device *dev, uint32_t addr);
};

struct pbl_flash_device_state {
  struct pbl_mutex lock;
  struct pbl_sem erase_sem;
  bool initialized;
  bool coredump;
  struct {
    bool enabled;
    uint32_t start;
    uint32_t end;
  } protect;
  struct {
    bool in_progress;
    bool suspended;
    //! The hardware finished before a suspend was attempted.
    bool done;
    uint32_t next;
    uint32_t end;
    uint32_t unit;
    uint32_t expected_ms;
    uint8_t retries;
    pbl_flash_erase_cb_t cb;
    void *ctx;
  } erase;
  TimerID poll_timer;
  TimerID resume_timer;
};

struct pbl_flash_device {
  struct pbl_flash_device_state *state;
  const struct pbl_flash_ops *ops;
  //! Address of the first byte, as seen by the flash API.
  uint32_t base;
  const struct pbl_flash_geometry *geometry;
  //! NULL when the part has no security registers.
  const struct pbl_flash_sec_regs *sec_regs;
};

//! The board's storage flash.
extern const struct pbl_flash_device *const FLASH;

int pbl_flash_init(const struct pbl_flash_device *dev);

//! Re-initialise the device for use from a fault handler: no locking, no
//! sleeping, no timers. Not reversible.
int pbl_flash_coredump_init(const struct pbl_flash_device *dev);

//! Wait for an in-progress erase to finish. Called before reset.
void pbl_flash_stop(const struct pbl_flash_device *dev);

//! Reads and writes are thread safe and may be issued while an erase is in
//! progress. Write and erase assert on hardware failure.
int pbl_flash_read(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len);
int pbl_flash_write(const struct pbl_flash_device *dev, uint32_t addr, const void *buf, size_t len);

//! Erase [addr, addr + len). @p addr must be subsector aligned; @p len is
//! rounded up to a subsector. Sector erases are used wherever the range allows.
int pbl_flash_erase(const struct pbl_flash_device *dev, uint32_t addr, size_t len);

//! Like @ref pbl_flash_erase, completing through @p cb from an arbitrary task.
//! The callback may also run before this function returns. Blocks while
//! another erase is ongoing.
int pbl_flash_erase_async(const struct pbl_flash_device *dev, uint32_t addr, size_t len,
                          pbl_flash_erase_cb_t cb, void *ctx);

bool pbl_flash_is_erased(const struct pbl_flash_device *dev, uint32_t addr, size_t len);

//! Refuse writes and erases in [addr, addr + len). Only one range at a time.
int pbl_flash_protect(const struct pbl_flash_device *dev, uint32_t addr, size_t len);
int pbl_flash_unprotect(const struct pbl_flash_device *dev);

//! Around MCU stop mode, with interrupts disabled.
void pbl_flash_power_down(const struct pbl_flash_device *dev);
void pbl_flash_power_up(const struct pbl_flash_device *dev);

uint32_t pbl_flash_crc32(const struct pbl_flash_device *dev, uint32_t addr, size_t len);
uint32_t pbl_flash_legacy_checksum(const struct pbl_flash_device *dev, uint32_t addr, size_t len);

//! Security registers. -ENOTSUP when the part has none.
int pbl_flash_sec_reg_read(const struct pbl_flash_device *dev, uint32_t addr, uint8_t *val);
int pbl_flash_sec_reg_write(const struct pbl_flash_device *dev, uint32_t addr, uint8_t val);
int pbl_flash_sec_reg_erase(const struct pbl_flash_device *dev, uint32_t addr);
int pbl_flash_sec_reg_is_locked(const struct pbl_flash_device *dev, uint32_t addr, bool *locked);
#ifdef CONFIG_RECOVERY_FW
//! Permanently locks the register. One-time operation.
int pbl_flash_sec_reg_lock(const struct pbl_flash_device *dev, uint32_t addr);
#endif
