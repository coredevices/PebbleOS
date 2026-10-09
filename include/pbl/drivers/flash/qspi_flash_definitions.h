/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "qspi_flash_part_definitions.h"

#include <pbl/drivers/qspi_definitions.h>

#include <board/board.h>

/**
 * @defgroup drivers_flash_qspi_flash_definitions QSPI flash device
 * @ingroup drivers_flash
 * @brief Board description of a QSPI flash device.
 * @{
 */

/** @brief QSPI flash state, owned by the driver. */
typedef struct QSPIFlashState {
  /** Part description. */
  QSPIFlashPart *part;
  /** Running in core dump mode, without OS services. */
  bool coredump_mode;
  /** Double data rate fast read is in use. */
  bool fast_read_ddr_enabled;
} QSPIFlashState;

/** @brief Read instruction used by the controller. */
typedef enum QSPIFlashReadMode {
  /** Fast read (1-1-1). */
  QSPI_FLASH_READ_FASTREAD,
  /** Dual output read (1-1-2). */
  QSPI_FLASH_READ_READ2O,
  /** Dual I/O read (1-2-2). */
  QSPI_FLASH_READ_READ2IO,
  /** Quad output read (1-1-4). */
  QSPI_FLASH_READ_READ4O,
  /** Quad I/O read (1-4-4). */
  QSPI_FLASH_READ_READ4IO,
} QSPIFlashReadMode;

/** @brief Program instruction used by the controller. */
typedef enum QSPIFlashWriteMode {
  /** Page program (1-1-1). */
  QSPI_FLASH_WRITE_PP,
  /** Dual input page program (1-1-2). */
  QSPI_FLASH_WRITE_PP2O,
  /** Quad input page program (1-1-4). */
  QSPI_FLASH_WRITE_PP4O,
  /** Quad I/O page program (1-4-4). */
  QSPI_FLASH_WRITE_PP4IO,
} QSPIFlashWriteMode;

/** @brief QSPI flash device board description. */
typedef const struct QSPIFlash {
  /** Driver state. */
  QSPIFlashState *state;
  /** QSPI port the flash is attached to. */
  QSPIPort *qspi;
  /** Use double data rate fast read by default. */
  bool default_fast_read_ddr_enabled;
  /** Read mode. */
  QSPIFlashReadMode read_mode;
  /** Write mode. */
  QSPIFlashWriteMode write_mode;
} QSPIFlash;

/** @} */
