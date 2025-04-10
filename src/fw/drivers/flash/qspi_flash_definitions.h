/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>

#include "board/board.h"
#include "drivers/qspi.h"
#include "qspi_flash_part_definitions.h"

typedef struct QSPIFlashState {
  QSPIFlashPart *part;
  bool coredump_mode;
  bool fast_read_ddr_enabled;
} QSPIFlashState;

typedef enum QSPIFlashReadMode {
  QSPI_FLASH_READ_FASTREAD,
  QSPI_FLASH_READ_READ2O,
  QSPI_FLASH_READ_READ2IO,
  QSPI_FLASH_READ_READ4O,
  QSPI_FLASH_READ_READ4IO,
} QSPIFlashReadMode;

typedef enum QSPIFlashWriteMode {
  QSPI_FLASH_WRITE_PP,
  QSPI_FLASH_WRITE_PP2O,
  QSPI_FLASH_WRITE_PP4O,
  QSPI_FLASH_WRITE_PP4IO,
} QSPIFlashWriteMode;

typedef const struct QSPIFlash {
  QSPIFlashState *state;
  QSPIPort *qspi;
  bool default_fast_read_ddr_enabled;
  QSPIFlashReadMode read_mode;
  QSPIFlashWriteMode write_mode;
  OutputConfig reset_gpio;
} QSPIFlash;
