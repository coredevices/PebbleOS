/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup loader Loader
 * @ingroup subsys
 * @brief Loader for position-independent images with absolute relocations.
 *
 * An image is linked at address 0 and stored as its file part (code and data) followed, at any
 * offset of the source, by a relocation table. Each table entry is the little-endian 32-bit
 * offset of a 32-bit slot in the file part; the slot holds an image offset, and the loader adds
 * the address the image was loaded at to it. The memory past the file part, up to the memory
 * size, is zero-filled.
 *
 * Loading happens in two steps, so the caller can check the copied bytes (e.g. a checksum)
 * before relocations change them:
 *
 * @code{.c}
 * ret = pbl_loader_copy(&img, &src, dst, dst_size);
 * if (ret < 0) {
 *   return ret;
 * }
 * // verify dst here
 * ret = pbl_loader_relocate(&img, &src, dst, dst_size, nullptr, nullptr);
 * @endcode
 *
 * Slots and table entries do not need to be aligned. Every entry and slot value is checked
 * against the image, so a malformed image cannot write outside of it.
 *
 * @{
 */

/**
 * @brief Read bytes from an image source.
 *
 * @param ctx Source context.
 * @param offset Offset in the source.
 * @param[out] buf Destination buffer.
 * @param len Number of bytes to read.
 * @return 0 if exactly @p len bytes were read, a negative errno otherwise.
 */
typedef int (*pbl_loader_read_t)(void *ctx, size_t offset, void *buf, size_t len);

/**
 * @brief Translate an address in the loaded image to the address the image code sees.
 *
 * @param ctx Translation context.
 * @param addr Address in the destination buffer.
 * @return Address the image code uses for @p addr.
 */
typedef uint32_t (*pbl_loader_address_t)(void *ctx, const void *addr);

/** @brief Image source. */
struct pbl_loader_source {
  /** Read callback. */
  pbl_loader_read_t read;
  /** Context passed to @ref pbl_loader_source.read. */
  void *ctx;
};

/** @brief Image layout. */
struct pbl_loader_image {
  /** Size of the file part, stored at source offset 0. */
  size_t file_size;
  /** Size of the image in memory, file part plus zero-fill. */
  size_t mem_size;
  /** Size of the header at the start of the file part, which relocations must not target. */
  size_t header_size;
  /** Source offset of the relocation table. */
  size_t reloc_offset;
  /** Number of relocation table entries. */
  size_t reloc_count;
};

/**
 * @brief Copy the file part of an image and zero-fill the rest of its memory.
 *
 * @param img Image layout.
 * @param src Image source.
 * @param dst Destination buffer.
 * @param dst_size Size of @p dst.
 * @retval 0 Success.
 * @retval -EINVAL The layout is inconsistent.
 * @retval -ENOMEM The image does not fit in @p dst.
 * @return Other negative errno values from the source.
 */
int pbl_loader_copy(const struct pbl_loader_image *img, const struct pbl_loader_source *src,
                    void *dst, size_t dst_size);

/**
 * @brief Apply the relocations of an image copied by pbl_loader_copy().
 *
 * The part of @p dst past the file part is used to read the relocation table, and is zeroed
 * again afterwards, so @p dst_size must hold the file part plus the table.
 *
 * @param img Image layout.
 * @param src Image source.
 * @param dst Destination buffer.
 * @param dst_size Size of @p dst.
 * @param address Address translation, or @c nullptr to relocate to @p dst itself.
 * @param ctx Context passed to @p address.
 * @retval 0 Success.
 * @retval -EINVAL The layout is inconsistent, or an entry or slot points outside the image.
 * @retval -ENOMEM The relocation table does not fit in @p dst.
 * @return Other negative errno values from the source.
 */
int pbl_loader_relocate(const struct pbl_loader_image *img, const struct pbl_loader_source *src,
                        void *dst, size_t dst_size, pbl_loader_address_t address, void *ctx);

/** @} */
