/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/flash/qspi_flash_definitions.h>
#include <pbl/drivers/flash/qspi_flash_part_definitions.h>

#include <system/status_codes.h>

/**
 * @defgroup drivers_flash_qspi_flash QSPI flash
 * @ingroup drivers_flash
 * @brief Generic QSPI NOR flash driver, used by the part drivers to implement
 *        @ref drivers_flash_flash_impl.
 * @{
 */

/**
 * @brief Initialize a QSPI flash device.
 *
 * Resets the part and configures the controller for its read and write modes.
 *
 * @param dev Device.
 * @param part Part description.
 * @param coredump_mode Do not rely on OS services, because a core dump is in progress.
 */
void qspi_flash_init(QSPIFlash *dev, QSPIFlashPart *part, bool coredump_mode);

/**
 * @brief Check whether the device was initialized in core dump mode.
 *
 * @param dev Device.
 * @return true in core dump mode.
 */
bool qspi_flash_is_in_coredump_mode(QSPIFlash *dev);

/**
 * @brief Check the JEDEC ID against the part description.
 *
 * @param dev Device.
 * @return true if it matches.
 */
bool qspi_flash_check_whoami(QSPIFlash *dev);

/**
 * @brief Check whether an erase has completed.
 *
 * @param dev Device.
 * @return Erase status, as flash_impl_get_erase_status().
 */
status_t qspi_flash_is_erase_complete(QSPIFlash *dev);

/**
 * @brief Start an erase.
 *
 * @param dev Device.
 * @param addr Address of the sector or subsector.
 * @param is_subsector Erase a subsector instead of a sector.
 * @return S_SUCCESS or an error.
 */
status_t qspi_flash_erase_begin(QSPIFlash *dev, uint32_t addr, bool is_subsector);

/**
 * @brief Suspend an in-progress erase.
 *
 * @param dev Device.
 * @param addr Address of the erase.
 * @return As flash_impl_erase_suspend().
 */
status_t qspi_flash_erase_suspend(QSPIFlash *dev, uint32_t addr);

/**
 * @brief Resume a suspended erase.
 *
 * @param dev Device.
 * @param addr Address of the erase.
 */
void qspi_flash_erase_resume(QSPIFlash *dev, uint32_t addr);

/**
 * @brief Read data, blocking.
 *
 * @param dev Device.
 * @param addr Flash address.
 * @param[out] buffer Buffer receiving the data.
 * @param length Number of bytes to read.
 */
void qspi_flash_read_blocking(QSPIFlash *dev, uint32_t addr, void *buffer, uint32_t length);

/**
 * @brief Start writing up to a page.
 *
 * @param dev Device.
 * @param buffer Data to write.
 * @param addr Flash address.
 * @param length Number of bytes available in @p buffer.
 * @return As flash_impl_write_page_begin().
 */
int qspi_flash_write_page_begin(QSPIFlash *dev, const void *buffer, uint32_t addr, uint32_t length);

/**
 * @brief Poll the status of a page write.
 *
 * @param dev Device.
 * @return As flash_impl_get_write_status().
 */
status_t qspi_flash_get_write_status(QSPIFlash *dev);

/**
 * @brief Enter or leave deep power-down.
 *
 * @param dev Device.
 * @param active true to enter deep power-down.
 */
void qspi_flash_set_lower_power_mode(QSPIFlash *dev, bool active);

/**
 * @brief Check whether a sector or subsector is blank.
 *
 * @param dev Device.
 * @param addr Address within the sector or subsector.
 * @param is_subsector Check a subsector instead of a sector.
 * @return As flash_impl_blank_check_sector().
 */
status_t qspi_flash_blank_check(QSPIFlash *dev, uint32_t addr, bool is_subsector);

/**
 * @brief Enable write and erase protection.
 *
 * Uses the @c write_protection_enable instruction and checks the result of
 * @c read_protection_status against @c block_lock.protection_enabled_mask.
 *
 * @param dev Device.
 * @return S_SUCCESS, S_NO_ACTION_REQUIRED if unsupported, or an error.
 */
status_t qspi_flash_write_protection_enable(QSPIFlash *dev);

/**
 * @brief Lock a sector against writes and erases.
 *
 * Uses the @c block_lock instruction, with @c block_lock.lock_data if
 * @c block_lock.has_lock_data, and checks @c block_lock_status against
 * @c block_lock.locked_check.
 *
 * @param dev Device.
 * @param addr Sector address.
 * @return S_SUCCESS or an error.
 */
status_t qspi_flash_lock_sector(QSPIFlash *dev, uint32_t addr);

/**
 * @brief Unlock all sectors, with the @c block_unlock_all instruction.
 *
 * @param dev Device.
 * @return S_SUCCESS or an error.
 */
status_t qspi_flash_unlock_all(QSPIFlash *dev);

/**
 * @brief Read a byte from a security register.
 *
 * @param dev Device.
 * @param addr Security register address.
 * @param[out] val Byte read.
 * @return As flash_impl_read_security_register().
 */
status_t qspi_flash_read_security_register(QSPIFlash *dev, uint32_t addr, uint8_t *val);

/**
 * @brief Check whether a security register is locked.
 *
 * @param dev Device.
 * @param address Security register address.
 * @param[out] locked true if locked.
 * @return As flash_impl_security_register_is_locked().
 */
status_t qspi_flash_security_register_is_locked(QSPIFlash *dev, uint32_t address, bool *locked);

/**
 * @brief Erase a security register.
 *
 * @param dev Device.
 * @param addr Security register address.
 * @return As flash_impl_erase_security_register().
 */
status_t qspi_flash_erase_security_register(QSPIFlash *dev, uint32_t addr);

/**
 * @brief Write a byte to a security register.
 *
 * @param dev Device.
 * @param addr Security register address.
 * @param val Byte to write.
 * @return As flash_impl_write_security_register().
 */
status_t qspi_flash_write_security_register(QSPIFlash *dev, uint32_t addr, uint8_t val);

/**
 * @brief Get the security register layout.
 *
 * @param dev Device.
 * @return Security register information.
 */
const FlashSecurityRegisters *qspi_flash_security_registers_info(QSPIFlash *dev);

#ifdef CONFIG_RECOVERY_FW
/**
 * @brief Permanently lock the security registers.
 *
 * @warning One-time operation that cannot be undone.
 *
 * @param dev Device.
 * @param address Security register address.
 * @return As flash_impl_lock_security_register().
 */
status_t qspi_flash_lock_security_register(QSPIFlash *dev, uint32_t address);
#endif // CONFIG_RECOVERY_FW

/** @} */
