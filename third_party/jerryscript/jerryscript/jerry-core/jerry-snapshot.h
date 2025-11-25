/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_SNAPSHOT_H
#define JERRY_SNAPSHOT_H

#include "ecma-globals.h"

/**
 * Snapshot header
 */
typedef struct
{
  /* The size of this structure is recommended to be divisible by
   * JMEM_ALIGNMENT. Otherwise some bytes after the header are wasted. */
  uint32_t version; /**< version number */
  uint32_t lit_table_offset; /**< offset of the literal table */
  uint32_t lit_table_size; /**< size of literal table */
  uint32_t is_run_global; /**< flag, indicating whether the snapshot
                            *   was saved as 'Global scope'-mode code (true)
                            *   or as eval-mode code (false) */
} jerry_snapshot_header_t;

/**
 * Jerry snapshot format version
 */
#define JERRY_SNAPSHOT_VERSION (6u)

#endif /* !JERRY_SNAPSHOT_H */
