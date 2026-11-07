/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/section.h>

/**
 * @defgroup kernel_mpu Memory protection
 * @ingroup kernel
 * @brief Memory protection regions.
 *
 * A region describes a range of memory: who may access it and what kind of memory it is. The
 * kernel owns the protection hardware, so callers never see its slots or register encodings.
 *
 * Static regions apply to every thread and stay until reset. Thread regions are switched in with
 * a thread, see @ref pbl_thread_attr::regions.
 *
 * Privileged code can access anything no region covers, through the default memory map.
 * Unprivileged code can only access what a region grants. Static regions must not overlap.
 *
 * @code{.c}
 * static const struct pbl_mpu_region s_banks = {
 *   .name = "banks",
 *   .base = BANKS_BASE,
 *   .size = BANKS_SIZE,
 *   .attr = PBL_MPU_P_RO_U_RO | PBL_MPU_NORMAL_WT,
 * };
 *
 * PBL_ASSERTN(pbl_mpu_region_add(&s_banks) == 0);
 * @endcode
 * @{
 */

/** Places a variable where unprivileged code can read it but not write to it. */
#define KERNEL_READONLY_DATA PBL_SECTION(".kernel_unpriv_ro_bss")

/**
 * @brief Assign a @ref KERNEL_READONLY_DATA variable. Privileged code only.
 *
 * @param var Variable.
 * @param val Value.
 */
#define KERNEL_READONLY_DATA_SET(var, val)              \
  do {                                                  \
    typeof(var) val_ = (val);                           \
    pbl_mpu_user_ro_write(&(var), &val_, sizeof(val_)); \
  } while (0)

/** No access. Below a stack, this makes a guard. */
#define PBL_MPU_NO_ACCESS 0U
/** Privileged read/write, no unprivileged access. */
#define PBL_MPU_P_RW 1U
/** Privileged read/write, unprivileged read-only. */
#define PBL_MPU_P_RW_U_RO 2U
/** Privileged and unprivileged read/write. */
#define PBL_MPU_P_RW_U_RW 3U
/** Privileged read-only, no unprivileged access. */
#define PBL_MPU_P_RO 4U
/** Privileged and unprivileged read-only. */
#define PBL_MPU_P_RO_U_RO 5U
/** Mask of the access attribute. */
#define PBL_MPU_ACCESS_MSK 7U

/** Normal memory, not cached. */
#define PBL_MPU_NORMAL_NC (0U << 3)
/** Normal memory, write-through cached. */
#define PBL_MPU_NORMAL_WT (1U << 3)
/** Normal memory, write-back cached. */
#define PBL_MPU_NORMAL_WB (2U << 3)
/** Device memory: accesses are not merged, reordered or cached. */
#define PBL_MPU_DEVICE (3U << 3)
/** Mask of the memory type attribute. */
#define PBL_MPU_TYPE_MSK (3U << 3)

/** Instructions may be fetched from the region. */
#define PBL_MPU_EXEC (1U << 5)
/** Coherent with other bus masters. Some cores do not cache shareable memory at all. */
#define PBL_MPU_SHAREABLE (1U << 6)

/**
 * @brief Memory region.
 *
 * The base and size must be multiples of 32 bytes. Not every range can be protected on every
 * architecture: ARMv7-M needs it to line up with eighths of a power-of-two block.
 */
struct pbl_mpu_region {
  /** Name shown by pbl_mpu_regions_print(); optional. */
  const char *name;
  /** Lowest address. */
  uintptr_t base;
  /** Size in bytes. */
  size_t size;
  /** Access, memory type and flags: PBL_MPU_*. */
  uint32_t attr;
};

/**
 * @brief Add a static region, for every thread and until reset.
 *
 * @param region Region to add, which must stay valid.
 * @return 0 on success.
 * @retval -ENOSPC No slot is left.
 * @retval -EINVAL The region overlaps another static region.
 */
int pbl_mpu_region_add(const struct pbl_mpu_region *region);

/**
 * @brief Write to the @ref PBL_MPU_P_RW_U_RO region from privileged code.
 *
 * ARMv8-M cannot let only privileged code write to a region, so the region is lifted for the
 * duration of the copy, with interrupts masked. Elsewhere this is a plain copy.
 *
 * @param dst Destination.
 * @param src Source.
 * @param n Number of bytes.
 */
void pbl_mpu_user_ro_write(void *dst, const void *src, size_t n);

/**
 * @brief Regions the SoC declares, with @c CONFIG_SOC_HAS_MPU_REGIONS.
 *
 * The kernel adds them first thing on boot, before pbl_soc_early_init().
 */
extern const struct pbl_mpu_region pbl_soc_mpu_regions[];
/** @brief Number of entries in @ref pbl_soc_mpu_regions. */
extern const size_t pbl_soc_num_mpu_regions;

/**
 * @brief Check whether a stack guard covers any of a memory range.
 *
 * Usable from fault handlers.
 *
 * @param start Lowest address of the range.
 * @param end End of the range, exclusive.
 * @return true if the range overlaps the ISR stack guard or one of the running thread.
 */
bool pbl_mpu_region_is_stack_guard(uintptr_t start, uintptr_t end);

/**
 * @brief Get the memory of the running firmware image, to check pointers against.
 *
 * Not always what the MPU protects: an architecture may have to round its region up.
 *
 * @return Firmware image region.
 */
const struct pbl_mpu_region *pbl_mpu_region_firmware(void);

/**
 * @brief Check whether a region contains an address.
 *
 * @param region Region.
 * @param ptr Address.
 * @return true if @p ptr is inside @p region.
 */
bool pbl_mpu_region_contains(const struct pbl_mpu_region *region, const void *ptr);

/**
 * @brief Check whether a region contains a whole buffer.
 *
 * @param region Region.
 * @param buf Buffer.
 * @param length Buffer size in bytes.
 * @return true if all of @p buf is inside @p region.
 */
bool pbl_mpu_region_contains_buffer(const struct pbl_mpu_region *region, const void *buf,
                                    size_t length);

/**
 * @brief Check whether a region contains a whole NUL-terminated string.
 *
 * Never reads outside of the region.
 *
 * @param region Region.
 * @param str String.
 * @param max_length Longest accepted string, without the terminator.
 * @return true if @p str and its terminator are inside @p region.
 */
bool pbl_mpu_region_contains_cstring(const struct pbl_mpu_region *region, const char *str,
                                     size_t max_length);

/**
 * @brief Print every programmed region, one line at a time.
 *
 * Usable from fault handlers.
 *
 * @param print Called with each line.
 * @param ctx Passed to @p print.
 */
void pbl_mpu_regions_print(void (*print)(void *ctx, const char *line), void *ctx);

/** @} */
