/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <system/status_codes.h>

/**
 * @defgroup drivers_flash Flash
 * @ingroup drivers
 * @brief External flash access.
 *
 * Thread-safe API on top of a part-specific low-level driver (see @ref drivers_flash_flash_impl).
 * Reads and writes block; an in-progress erase is suspended while they run. Writes only clear
 * bits, so the target range must be erased first. Erases work on subsectors and sectors, whose
 * sizes depend on the part.
 *
 * @code{.c}
 * static void prv_erased(void *context, status_t result) {
 *   // Runs on a timer task: keep it short
 * }
 *
 * flash_erase_subsector_blocking(addr);
 * flash_write_bytes(data, addr, sizeof(data));
 * flash_read_bytes(buf, addr, sizeof(buf));
 *
 * flash_erase_sector(other_addr, prv_erased, NULL);
 * @endcode
 * @{
 */

/** @brief Expected ID of a 32 Mbit part. */
static const uint32_t EXPECTED_SPI_FLASH_ID_32MBIT = 0x20bb16;
/** @brief Expected ID of a 64 Mbit part. */
static const uint32_t EXPECTED_SPI_FLASH_ID_64MBIT = 0x20bb17;

/** @brief Security (OTP) registers of the flash part. */
typedef struct FlashSecurityRegisters {
  /** Base address of each security register. */
  const uint32_t *sec_regs;
  /** Number of security registers. */
  uint8_t num_sec_regs;
  /** Size of each security register in bytes. */
  uint16_t sec_reg_size;
} FlashSecurityRegisters;

/** @brief Initialize the flash driver and the flash part. */
void flash_init(void);

/**
 * @brief Stop flash activity.
 *
 * Waits for an in-progress erase to finish. Does nothing before flash_init().
 */
void flash_stop(void);

/**
 * @brief Read from flash.
 *
 * No range checking is done.
 *
 * @param[out] buffer Buffer receiving the data.
 * @param start_addr Flash address of the first byte.
 * @param buffer_size Number of bytes to read.
 */
void flash_read_bytes(uint8_t *buffer, uint32_t start_addr, uint32_t buffer_size);

/**
 * @brief Write to flash.
 *
 * Handles unaligned addresses and writes spanning several pages. Asserts on failure.
 *
 * @param buffer Data to write.
 * @param start_addr Flash address of the first byte.
 * @param buffer_size Number of bytes to write.
 */
void flash_write_bytes(const uint8_t *buffer, uint32_t start_addr, uint32_t buffer_size);

/**
 * @brief Flash operation completion callback.
 *
 * @param context User context.
 * @param result S_SUCCESS, S_NO_ACTION_REQUIRED if the area was already erased, or an error.
 */
typedef void (*FlashOperationCompleteCb)(void *context, status_t result);

/**
 * @brief Erase the subsector containing an address, asynchronously.
 *
 * @p on_complete is called once the erase finishes, succeeded or not, from a timer task or
 * directly from this function. It must return quickly.
 *
 * @param subsector_addr Address within the subsector.
 * @param on_complete Completion callback.
 * @param context User context passed to @p on_complete.
 */
void flash_erase_subsector(uint32_t subsector_addr, FlashOperationCompleteCb on_complete,
                           void *context);

/**
 * @brief Erase the sector containing an address, asynchronously.
 *
 * @p on_complete is called once the erase finishes, succeeded or not, from a timer task or
 * directly from this function. It must return quickly.
 *
 * @param sector_addr Address within the sector.
 * @param on_complete Completion callback.
 * @param context User context passed to @p on_complete.
 */
void flash_erase_sector(uint32_t sector_addr, FlashOperationCompleteCb on_complete, void *context);

/**
 * @brief Erase the subsector containing an address.
 *
 * Blocks until done and asserts on failure.
 *
 * @param subsector_addr Address within the subsector.
 */
void flash_erase_subsector_blocking(uint32_t subsector_addr);

/**
 * @brief Erase the sector containing an address.
 *
 * Blocks until done, which takes 100 ms or more, and asserts on failure.
 *
 * @param sector_addr Address within the sector.
 */
void flash_erase_sector_blocking(uint32_t sector_addr);

/**
 * @brief Check whether the sector containing an address is erased.
 *
 * @param sector_addr Address within the sector.
 * @return true if erased.
 */
bool flash_sector_is_erased(uint32_t sector_addr);

/**
 * @brief Check whether the subsector containing an address is erased.
 *
 * @param sector_addr Address within the subsector.
 * @return true if erased.
 */
bool flash_subsector_is_erased(uint32_t sector_addr);

/**
 * @brief Erase the entire flash.
 *
 * Blocks for up to a minute: make sure the watchdog does not fire.
 */
void flash_erase_bulk(void);

/**
 * @brief Erase a range of flash asynchronously, using as few erase operations as possible.
 *
 * Erases at least [@p max_start, @p min_end) and at most [@p min_start, @p max_end), using
 * sector erases where possible and subsector erases elsewhere.
 *
 * @param min_start Lowest address that may be erased, subsector aligned.
 * @param max_start Highest address the erase may start at.
 * @param min_end Lowest address the erase may end at (exclusive).
 * @param max_end Highest address the erase may end at (exclusive), subsector aligned.
 * @param on_complete Callback run once the whole range is erased or an erase failed.
 * @param context User context passed to @p on_complete.
 */
void flash_erase_optimal_range(uint32_t min_start, uint32_t max_start, uint32_t min_end,
                               uint32_t max_end, FlashOperationCompleteCb on_complete,
                               void *context);

/**
 * @brief Let the flash enter deep sleep between commands.
 *
 * @param enable true to enable.
 */
void flash_sleep_when_idle(bool enable);

/**
 * @brief Check whether flash_sleep_when_idle() is in effect.
 *
 * @return true if enabled.
 */
bool flash_get_sleep_when_idle(void);

/**
 * @brief Check whether flash_init() has run.
 *
 * @return true if initialized.
 */
bool flash_is_initialized(void);

/**
 * @brief Put the flash in deep power-down before entering stop mode.
 *
 * Takes no locks; call only with interrupts disabled. The part draws about 100 uA in standby
 * and 10 uA in deep power-down, which only matters while the MCU is in stop mode.
 */
void flash_power_down_for_stop_mode(void);

/**
 * @brief Wake the flash after stop mode.
 *
 * Counterpart of flash_power_down_for_stop_mode(), with the same constraints.
 */
void flash_power_up_after_stop_mode(void);

/** @brief Flash read mode. */
typedef enum {
  /** Asynchronous reads. */
  FLASH_MODE_ASYNC = 0,
  /** Synchronous burst reads. */
  FLASH_MODE_SYNC_BURST,

  /** Number of modes. */
  FLASH_MODE_NUM_MODES
} FlashModeType;

/**
 * @brief Switch the read mode.
 *
 * @param mode New mode; burst mode is used only if the part supports it.
 */
void flash_switch_mode(FlashModeType mode);

/**
 * @brief Get the base address of the sector containing an address.
 *
 * @param flash_addr Flash address.
 * @return Sector base address.
 */
uint32_t flash_get_sector_base_address(uint32_t flash_addr);

/**
 * @brief Get the base address of the subsector containing an address.
 *
 * @param flash_addr Flash address.
 * @return Subsector base address.
 */
uint32_t flash_get_subsector_base_address(uint32_t flash_addr);

/** @brief Enable write protection, if the part requires it to be enabled explicitly. */
void flash_enable_write_protection(void);

/**
 * @brief Write-protect the recovery firmware region, or remove all protection.
 *
 * @param do_protect true to protect the region, false to unprotect the whole flash.
 */
void flash_prf_set_protection(bool do_protect);

/**
 * @brief Compute the CRC-32 of a flash region.
 *
 * @param flash_addr Start address.
 * @param length Length in bytes.
 * @return pbl_crc32() of the region.
 */
uint32_t flash_crc32(uint32_t flash_addr, uint32_t length);

/**
 * @brief Compute the legacy checksum of a flash region.
 *
 * @param flash_addr Start address.
 * @param length Length in bytes.
 * @return pbl_crc32_legacy() of the region.
 */
uint32_t flash_crc32_legacy(uint32_t flash_addr, uint32_t length);

/**
 * @brief Take a reference keeping the flash peripheral powered.
 *
 * Call before any flash access, including memory-mapped reads. Release with flash_release_many().
 */
void flash_use(void);

/**
 * @brief Drop several references taken with flash_use().
 *
 * The peripheral is powered down when the count reaches zero.
 *
 * @param num_locks Number of references to drop, usually 1.
 */
void flash_release_many(uint32_t num_locks);

/**
 * @brief Read a byte from a security register.
 *
 * @param addr Security register address.
 * @param[out] val Byte read.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_read_security_register(uint32_t addr, uint8_t *val);

/**
 * @brief Check whether a security register is locked.
 *
 * @param addr Security register address.
 * @param[out] locked true if locked.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_security_register_is_locked(uint32_t addr, bool *locked);

/**
 * @brief Erase a security register.
 *
 * @param addr Security register address.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_erase_security_register(uint32_t addr);

/**
 * @brief Write a byte to a security register.
 *
 * @param addr Security register address.
 * @param val Byte to write.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_write_security_register(uint32_t addr, uint8_t val);

/**
 * @brief Get the security register layout.
 *
 * @return Security register information.
 */
const FlashSecurityRegisters *flash_security_registers_info(void);

#ifdef CONFIG_RECOVERY_FW
/**
 * @brief Permanently lock the security registers.
 *
 * Only available in the recovery firmware.
 *
 * @warning One-time operation that cannot be undone.
 *
 * @param addr Security register address.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_lock_security_register(uint32_t addr);
#endif // CONFIG_RECOVERY_FW

/** @} */
