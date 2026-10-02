/*-
 * Copyright (c) 2000, 2001 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#pragma once

#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_i18n_mo MO file format
 * @ingroup services_i18n
 * @brief Layout of gettext MO files, used for language packs.
 *
 * Offsets are in bytes from the start of the file:
 *
 * @verbatim
 *           byte
 *                +------------------------------------------+
 *             0  | magic number = 0x950412de                |
 *                |                                          |
 *             4  | file format revision = 0                 |
 *                |                                          |
 *             8  | number of strings                        |  == N
 *                |                                          |
 *            12  | offset of table with original strings    |  == O
 *                |                                          |
 *            16  | offset of table with translation strings |  == T
 *                |                                          |
 *            20  | size of hashing table                    |  == S
 *                |                                          |
 *            24  | offset of hashing table                  |  == H
 *                |                                          |
 *                .                                          .
 *                .    (possibly more entries later)         .
 *                .                                          .
 *                |                                          |
 *             O  | length & offset 0th string  ----------------.
 *         O + 8  | length & offset 1st string  ------------------.
 *                 ...                                    ...   | |
 *   O + ((N-1)*8)| length & offset (N-1)th string           |  | |
 *                |                                          |  | |
 *             T  | length & offset 0th translation  ---------------.
 *         T + 8  | length & offset 1st translation  -----------------.
 *                 ...                                    ...   | | | |
 *   T + ((N-1)*8)| length & offset (N-1)th translation      |  | | | |
 *                |                                          |  | | | |
 *             H  | start hash table                         |  | | | |
 *                 ...                                    ...   | | | |
 *     H + S * 4  | end hash table                           |  | | | |
 *                |                                          |  | | | |
 *                | NUL terminated 0th string  <----------------' | | |
 *                |                                          |    | | |
 *                | NUL terminated 1st string  <------------------' | |
 *                |                                          |      | |
 *                 ...                                    ...       | |
 *                |                                          |      | |
 *                | NUL terminated 0th translation  <---------------' |
 *                |                                          |        |
 *                | NUL terminated 1st translation  <-----------------'
 *                |                                          |
 *                 ...                                    ...
 *                |                                          |
 *                +------------------------------------------+
 * @endverbatim
 * @{
 */

/** @brief Magic number at the start of an MO file. */
#define MO_MAGIC 0x950412de
/**
 * @brief Get the major part of an MO file revision.
 *
 * @param r Revision.
 */
#define MO_GET_REV_MAJOR(r) (((r) >> 16) & 0xFFFF)
/**
 * @brief Get the minor part of an MO file revision.
 *
 * @param r Revision.
 */
#define MO_GET_REV_MINOR(r) ((r) & 0xFFFF)
/**
 * @brief Build an MO file revision.
 *
 * @param maj Major part.
 * @param min Minor part.
 */
#define MO_MAKE_REV(maj, min) (((maj) << 16) | (min))

/** @brief Header property holding the ISO locale of the language. */
#define LANG_PROP_NAME "Language: "

/** @brief MO file header. */
typedef struct PBL_PACKED {
  /** Magic number, @ref MO_MAGIC; determines the endianness. */
  uint32_t mo_magic;
  /** File format revision, 0. */
  uint32_t mo_revision;
  /** Number of strings (N). */
  uint32_t mo_nstring;
  /** Offset of the original strings table (O). */
  uint32_t mo_otable;
  /** Offset of the translated strings table (T). */
  uint32_t mo_ttable;
  /** Number of entries in the hash table (S). */
  uint32_t mo_hsize;
  /** Offset of the hash table (H). */
  uint32_t mo_hoffset;
} MoHeader;

/** @brief Entry of the original or translated strings table. */
typedef struct PBL_PACKED {
  /** String length without terminator; the string occupies len + 1 bytes. */
  uint32_t len;
  /** Offset of the NUL-terminated string. */
  uint32_t off;
} MoEntry;

/** @brief Loaded MO file. */
typedef struct {
  /** File header. */
  MoHeader hdr;
  /** Language of the file. Currently unused. */
  char *mo_lang;
  /** Hash table (H), loaded into RAM. */
  uint32_t *mo_htable;
} Mo;

/** @brief Handle to a loaded MO file. */
typedef struct {
  /** Size of the file in bytes. */
  size_t len;
  /** Loaded file. */
  Mo mo;
} MoHandle;

/** @} */
