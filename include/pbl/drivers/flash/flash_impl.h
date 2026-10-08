/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/flash.h>

#include <system/status_codes.h>

/**
 * @defgroup drivers_flash_flash_impl Low-level flash driver
 * @ingroup drivers_flash
 * @brief Interface implemented by each flash part driver.
 *
 * Used by the flash API and by the core dump flash driver. Implementations do not rely on OS
 * services, except where noted.
 *
 * Unless otherwise specified, functions are not reentrant: do not call one while another runs
 * in a different thread, nor from within a flash_impl callback.
 * @{
 */

/** @brief Flash address. */
typedef uint32_t FlashAddress;

/**
 * @brief Initialize the driver and bring the part to a state ready to accept commands.
 *
 * @param coredump_mode Do not rely on any OS service, because a core dump is in progress.
 *                      Operations may be slower.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_init(bool coredump_mode);

/**
 * @brief Enable or disable synchronous burst mode, if supported.
 *
 * Burst mode is disabled by flash_impl_init(). The result is undefined if another operation is
 * in progress.
 *
 * @param enable true to enable burst mode.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_set_burst_mode(bool enable);

/**
 * @brief Get the base address of the sector containing an address.
 *
 * Reentrant.
 *
 * @param addr Flash address.
 * @return Sector base address.
 */
FlashAddress flash_impl_get_sector_base_address(FlashAddress addr);

/**
 * @brief Get the base address of the subsector containing an address.
 *
 * Reentrant.
 *
 * @param addr Flash address.
 * @return Subsector base address.
 */
FlashAddress flash_impl_get_subsector_base_address(FlashAddress addr);

/**
 * @brief Get the flash capacity.
 *
 * @return Capacity in bytes.
 */
size_t flash_impl_get_capacity(void);

/**
 * @brief Enter a low-power state.
 *
 * Operations may fail until flash_impl_exit_low_power_mode() is called. Idempotent.
 *
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_enter_low_power_mode(void);

/**
 * @brief Leave the low-power state.
 *
 * May take a while. Idempotent.
 *
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_exit_low_power_mode(void);

/**
 * @brief Read data.
 *
 * The result is undefined if a write or erase is in progress.
 *
 * @param[out] buffer Buffer receiving the data.
 * @param addr Flash address.
 * @param len Number of bytes to read.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_read_sync(void *buffer, FlashAddress addr, size_t len);

/** @brief Enable write protection, if the part requires it to be enabled explicitly. */
void flash_impl_enable_write_protection(void);

/**
 * @brief Write-protect a range of sectors.
 *
 * Only one range may be protected at a time. The result is undefined if a write or erase is in
 * progress.
 *
 * @param start_sector Address of the first protected sector.
 * @param end_sector Address of the last protected sector.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_write_protect(FlashAddress start_sector, FlashAddress end_sector);

/**
 * @brief Remove write protection.
 *
 * The result is undefined if a write or erase is in progress.
 *
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_unprotect(void);

/**
 * @brief Start writing up to a page.
 *
 * Starts a single program operation with as much data as the part accepts at once; writing a
 * whole buffer may take several calls:
 *
 * @code{.c}
 * while (len) {
 *   int written = flash_impl_write_page_begin(buffer, addr, len);
 *   if (written < 0) {
 *     // Handle error
 *   }
 *   status_t status;
 *   while ((status = flash_impl_get_write_status()) == E_BUSY) {
 *     continue;
 *   }
 *   if (status != S_SUCCESS) {
 *     // Handle error
 *   }
 *   buffer += written;
 *   addr += written;
 *   len -= written;
 * }
 * @endcode
 *
 * The result is undefined if a read or erase is in progress. It is an error to call this while
 * a write is in progress or suspended.
 *
 * @param buffer Data to write.
 * @param addr Flash address.
 * @param len Number of bytes available in @p buffer.
 * @return Number of bytes that will be written if the write completes, or a negative
 *         StatusCode if the write could not be started.
 */
int flash_impl_write_page_begin(const void *buffer, FlashAddress addr, size_t len);

/**
 * @brief Poll the status of a page write.
 *
 * @retval S_SUCCESS The write succeeded.
 * @retval E_ERROR The write failed.
 * @retval E_BUSY The write is in progress.
 * @retval E_AGAIN The write is suspended.
 */
status_t flash_impl_get_write_status(void);

/**
 * @brief Suspend an in-progress write so reads and erases are permitted.
 *
 * @param addr Address passed to the flash_impl_write_page_begin() call that started the write.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_write_suspend(FlashAddress addr);

/**
 * @brief Resume a suspended write.
 *
 * The result is undefined if a read or write is in progress.
 *
 * @param addr Address passed to flash_impl_write_suspend().
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_write_resume(FlashAddress addr);

/**
 * @brief Start erasing the subsector containing an address.
 *
 * The result is undefined if a read or write is in progress. It is an error to call this while
 * an erase is suspended.
 *
 * @param subsector_addr Address within the subsector.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_erase_subsector_begin(FlashAddress subsector_addr);

/**
 * @brief Start erasing the sector containing an address.
 *
 * The result is undefined if a read or write is in progress. It is an error to call this while
 * an erase is suspended.
 *
 * @param sector_addr Address within the sector.
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_erase_sector_begin(FlashAddress sector_addr);

/**
 * @brief Start erasing the entire flash.
 *
 * The result is undefined if a read or write is in progress. It is an error to call this while
 * an erase is suspended.
 *
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_erase_bulk_begin(void);

/**
 * @brief Poll the status of an erase.
 *
 * @retval S_SUCCESS The erase succeeded.
 * @retval E_ERROR The erase failed.
 * @retval E_BUSY The erase is in progress.
 * @retval E_AGAIN The erase is suspended.
 */
status_t flash_impl_get_erase_status(void);

/**
 * @brief Get the typical subsector erase duration.
 *
 * Reentrant.
 *
 * @return Duration in milliseconds.
 */
uint32_t flash_impl_get_typical_subsector_erase_duration_ms(void);

/**
 * @brief Get the typical sector erase duration.
 *
 * Reentrant.
 *
 * @return Duration in milliseconds.
 */
uint32_t flash_impl_get_typical_sector_erase_duration_ms(void);

/**
 * @brief Suspend an in-progress erase so reads and writes are permitted.
 *
 * @param addr Address passed to the flash_impl_erase_subsector_begin() or
 *             flash_impl_erase_sector_begin() call that started the erase.
 * @retval S_SUCCESS The erase is suspended.
 * @retval S_NO_ACTION_REQUIRED No erase was in progress.
 * @return Otherwise an error.
 */
status_t flash_impl_erase_suspend(FlashAddress addr);

/**
 * @brief Resume a suspended erase.
 *
 * The result is undefined if a read or write is in progress.
 *
 * @param addr Address passed to flash_impl_erase_suspend().
 * @return S_SUCCESS or an error.
 */
status_t flash_impl_erase_resume(FlashAddress addr);

/**
 * @brief Check whether the subsector containing an address is blank (all ones).
 *
 * Hardware accelerated where possible. Must not be called while any read, write or erase is in
 * progress or suspended, cannot be suspended, and no other operation may start until it
 * returns.
 *
 * @warning A subsector whose erase was interrupted may read as blank while not being fully
 *          erased; writing it may then fail or lose data.
 *
 * @param addr Address within the subsector.
 * @retval S_TRUE Blank.
 * @retval S_FALSE At least one bit is programmed.
 * @retval E_BUSY Another operation is in progress.
 */
status_t flash_impl_blank_check_subsector(FlashAddress addr);

/**
 * @brief Check whether the sector containing an address is blank (all ones).
 *
 * Hardware accelerated where possible. Must not be called while any read, write or erase is in
 * progress or suspended, cannot be suspended, and no other operation may start until it
 * returns.
 *
 * @warning A sector whose erase was interrupted may read as blank while not being fully
 *          erased; writing it may then fail or lose data.
 *
 * @param addr Address within the sector.
 * @retval S_TRUE Blank.
 * @retval S_FALSE At least one bit is programmed.
 * @retval E_BUSY Another operation is in progress.
 */
status_t flash_impl_blank_check_sector(FlashAddress addr);

/** @brief Take a reference keeping the flash peripheral powered. */
void flash_impl_use(void);
/** @brief Drop one reference taken with flash_impl_use(). */
void flash_impl_release(void);
/**
 * @brief Drop several references taken with flash_impl_use().
 *
 * @param num_locks Number of references to drop.
 */
void flash_impl_release_many(uint32_t num_locks);

/**
 * @brief Read a byte from a security register.
 *
 * @param addr Security register address.
 * @param[out] val Byte read.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_impl_read_security_register(uint32_t addr, uint8_t *val);

/**
 * @brief Check whether a security register is locked.
 *
 * @param address Security register address.
 * @param[out] locked true if locked.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p address is not in a security register, or
 *         another error.
 */
status_t flash_impl_security_register_is_locked(uint32_t address, bool *locked);

/**
 * @brief Erase a security register.
 *
 * @param addr Security register address.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_impl_erase_security_register(uint32_t addr);

/**
 * @brief Write a byte to a security register.
 *
 * @param addr Security register address.
 * @param val Byte to write.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p addr is not in a security register, or another
 *         error.
 */
status_t flash_impl_write_security_register(uint32_t addr, uint8_t val);

/**
 * @brief Get the security register layout.
 *
 * @return Security register information.
 */
const FlashSecurityRegisters *flash_impl_security_registers_info(void);

#ifdef CONFIG_RECOVERY_FW
/**
 * @brief Permanently lock the security registers.
 *
 * @warning One-time operation that cannot be undone.
 *
 * @param address Security register address.
 * @return S_SUCCESS, E_INVALID_ARGUMENT if @p address is not in a security register, or
 *         another error.
 */
status_t flash_impl_lock_security_register(uint32_t address);
#endif // CONFIG_RECOVERY_FW

/** @} */
