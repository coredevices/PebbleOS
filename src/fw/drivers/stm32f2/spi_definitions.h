/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/spi.h"

#include "board/board.h"

//! Generic(ish) definitions of how we wish a particular SPI to be configured
//! (Initially based on ST configuration and registers)
//! board.h and board_xxxx.h will use these definitions to configure each SPI
//! spi.c will use these definitions to program the device

// REVISIT: We may like to split the definition and control of the SCS
// signal out of the main spi driver and into a separate driver so
// that if we ever share an SPI and use multiple SCS bits to select
// the destination we can control them individually.  As it stands now
// we have exactly one.

//! SPI transmission modes (unidirectional/bidirectional etc)
typedef enum SpiDirection {
  SpiDirection_2LinesFullDuplex = 0x0000,
  SpiDirection_2LinesRxOnly = 0x0400,
  SpiDirection_1LineRx = 0x8000,
  SpiDirection_1LineTx = 0xC000
} SpiDirection;

//! SPI Clock Polarity
typedef enum SpiCPol {
  SpiCPol_Low = 0x0,
  SpiCPol_High = 0x2
} SpiCPol;

//! SPI Clock Phase
typedef enum SpiCPha {
  SpiCPha_1Edge = 0x0,
  SpiCPha_2Edge = 0x1
} SpiCPha;

//! SPI MSB / LSB First Bit Transmission
typedef enum SpiFirstBit {
  SpiFirstBit_MSB = 0x0000,
  SpiFirstBit_LSB = 0x0080
} SpiFirstBit;

//! SPI / I2S Flags
typedef enum SpiI2sFlag {
  SpiI2sFlag_RXNE = 0x0001,
  SpiI2sFlag_TXE = 0x0002,
  I2sFlag_CHSIDE = 0x0004,
  I2sFlag_UDR = 0x0008,
  SpiFlag_CRCERR = 0x0010,
  SpiFlag_MODF = 0x0020,
  SpiI2sFlag_OVR = 0x0040,
  SpiI2sFlag_BSY = 0x0080,
  SpiI2sFlag_TIFRFE = 0x0100
} SpiI2sFlag;

typedef enum SpiAPB {
  SpiAPB_1,
  SpiAPB_2
} SpiAPB;

typedef struct SPIBusState {
  uint32_t spi_clock_speed_hz; // can be changed by slave port
  uint32_t spi_clock_periph; // mapped to SPI peripheral
  uint32_t spi_clock_periph_speed;
  SpiAPB spi_apb;
  bool initialized;
} SPIBusState;

//! An SPI Bus specifies an SPI Instance and the I/O pins
//! used for the CLK, MOSI and MISO pins
//! The communication specific parameters (direction and
//! phase etc) and the pin to use for slave select are set per
//! SPISlavePort
//! REVISIT: There is currently no arbitration between possible
//! slave ports on the same bus - since for now all of our
//! SPI devices are point-to-point
typedef const struct SPIBus {
  SPIBusState *state;
  SPI_TypeDef *const spi;
  AfConfig spi_sclk;
  AfConfig spi_miso;
  AfConfig spi_mosi;
  uint16_t spi_sclk_speed;
//  uint32_t spi_clock_ctrl;
  uint32_t spi_clock_speed_hz;
} SPIBus;

typedef enum SPISlavePortDMAState {
  SPISlavePortDMAState_Idle,
  SPISlavePortDMAState_Read,
  SPISlavePortDMAState_Write,
  SPISlavePortDMAState_ReadWrite,
  SPISlavePortDMAState_ReadWriteOneInterrupt,
} SPISlavePortDMAState;

typedef struct SPISlavePortState {
  bool initialized;
  bool acquired;
  bool scs_selected;
  SPIDMACompleteHandler dma_complete_handler;
  void *dma_complete_context;
  SPISlavePortDMAState dma_state;
} SPISlavePortState;

typedef const struct SPISlavePort {
  SPISlavePortState *slave_state;
  SPIBus *spi_bus;
  OutputConfig spi_scs;
  SpiDirection spi_direction;
  SpiCPol spi_cpol;
  SpiCPha spi_cpha;
  SpiFirstBit spi_first_bit;
  DMARequest *rx_dma;
  DMARequest *tx_dma;
} SPISlavePort;
