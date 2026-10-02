/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup drivers_mpu MPU
 * @ingroup drivers
 * @brief Memory Protection Unit regions.
 *
 * Backends exist for ARMv7-M and ARMv8-M. Regions are described by their real base and size;
 * the backend derives the hardware encoding.
 *
 * @code{.c}
 * const MpuRegion region = {
 *   .region_num = 3,
 *   .enabled = true,
 *   .base_address = (uintptr_t)buf,
 *   .size = sizeof(buf),
 *   .cache_policy = MpuCachePolicy_WriteBackWriteAllocate,
 *   .permissions = MpuPermissions_PrivRW,
 * };
 *
 * mpu_set_region(&region);
 * @endcode
 * @{
 */

/** @brief Cache policy of a region. */
typedef enum MpuCachePolicy {
  /** Not cacheable. */
  MpuCachePolicy_NotCacheable,
  /** Write-through. */
  MpuCachePolicy_WriteThrough,
  /** Write-back, write-allocate. */
  MpuCachePolicy_WriteBackWriteAllocate,
  /** Write-back, no write-allocate. */
  MpuCachePolicy_WriteBackNoWriteAllocate,
} MpuCachePolicy;

/**
 * @brief Access permissions of a region.
 *
 * The backends map these to the hardware AP encoding (3 bits on ARMv7-M, 2 bits on ARMv8-M).
 * The two-bit ARMv8-M field cannot express two of them precisely:
 * - @ref MpuPermissions_NoAccess degrades to @ref MpuPermissions_PrivRO, which still allows
 *   privileged reads. Use PSPLIM, not this, for thread stack overflow detection.
 * - @ref MpuPermissions_PrivRW_UserRO is aliased to @ref MpuPermissions_PrivRW_UserRW, so
 *   unprivileged code gains write access.
 */
typedef enum MpuPermissions {
  /** No access. */
  MpuPermissions_NoAccess,
  /** Privileged read/write, no unprivileged access. */
  MpuPermissions_PrivRW,
  /** Privileged read/write, unprivileged read-only. */
  MpuPermissions_PrivRW_UserRO,
  /** Privileged and unprivileged read/write. */
  MpuPermissions_PrivRW_UserRW,
  /** Privileged read-only, no unprivileged access. */
  MpuPermissions_PrivRO,
  /** Privileged and unprivileged read-only. */
  MpuPermissions_PrivRO_UserRO,
  /** Number of permission sets. */
  MpuPermissionsCount,
} MpuPermissions;

/**
 * @brief MPU region, described by its real base and size.
 *
 * The ARMv7-M backend rounds the region up to a power-of-two block and computes the subregion
 * mask; the ARMv8-M backend programs the limit register directly.
 */
typedef struct MpuRegion {
  /** Region number, 0 to 15. */
  uint8_t region_num : 4;
  /** Region is enabled. */
  bool enabled : 1;
  /**
   * Allow instruction fetch (XN=0).
   *
   * Defaults to non-executable. Flash, app RAM, worker RAM and any code relocated to RAM must
   * opt in.
   */
  bool executable : 1;
  /** Base address, 32-byte aligned. */
  uintptr_t base_address;
  /** Size in bytes. */
  uint32_t size;
  /** Cache policy. */
  MpuCachePolicy cache_policy;
  /** Access permissions. */
  MpuPermissions permissions;
} MpuRegion;

/**
 * @brief Enable the MPU.
 *
 * The default memory map stays enabled as a background region for privileged code.
 */
void mpu_enable(void);

/** @brief Disable the MPU. */
void mpu_disable(void);

/**
 * @brief Program a region.
 *
 * @param region Region to program, in slot @ref MpuRegion::region_num.
 */
void mpu_set_region(const MpuRegion *region);

/**
 * @brief Read back a programmed region.
 *
 * @param region_num Region number.
 * @return Region as decoded from the hardware registers.
 */
MpuRegion mpu_get_region(int region_num);

/**
 * @brief Compute the register values for a region without programming it.
 *
 * @param region Region to encode.
 * @param[out] base_address_reg Value for the RBAR register.
 * @param[out] attributes_reg Value for the RASR (ARMv7-M) or RLAR (ARMv8-M) register.
 */
void mpu_get_register_settings(const MpuRegion *region, uint32_t *base_address_reg,
                               uint32_t *attributes_reg);

/**
 * @brief Check whether an address may be cached.
 *
 * Only SRAM is considered cacheable, and only while the data cache is enabled.
 *
 * @param addr Address to check.
 * @return true if @p addr is cacheable.
 */
bool mpu_memory_is_cachable(const void *addr);

/**
 * @brief Copy a privileged read/write region, optionally granting unprivileged access.
 *
 * Used to switch app and worker RAM regions on task switches.
 *
 * @param[out] copy Destination region.
 * @param from Source region; must be @ref MpuPermissions_PrivRW.
 * @param allow_user_access Grant unprivileged read/write access in @p copy.
 */
void mpu_init_region_from_region(MpuRegion *copy, const MpuRegion *from, bool allow_user_access);

/** @} */
