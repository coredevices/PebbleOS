/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Note: DMA isn't explicitly supported via this API.
//! These functions exist to allow the caller to manager their own
//! DMA and enable it without accessing the SPI configuration directly

//! Enable/Disable TX DMA for the given device
void spi_ll_slave_set_tx_dma(const SPISlavePort *slave, bool enable);

//! Enable/Disable RX DMA for the given device
void spi_ll_slave_set_rx_dma(const SPISlavePort *slave, bool enable);

//! Enable the SPI device
void spi_ll_slave_spi_enable(const SPISlavePort *slave);

//! Disable the SPI device
void spi_ll_slave_spi_disable(const SPISlavePort *slave);
