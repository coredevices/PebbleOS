/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdint.h>

//
//! High level slave port interface
//! This part of the API can be used for fairly straightforward SPI interactions
//! The assertion and deassertion of the SCS line is automatic
//

//! Scatter Gather tx / rx information
typedef struct SPIScatterGather {
    size_t sg_len; // number of bytes to tx and/or rx
    const void *sg_out; //! may be NULL (0 padding sent)
    void *sg_in; //! may be NULL (nothing saved)
} SPIScatterGather;

typedef bool (*SPIDMACompleteHandler)(const SPISlavePort *slave, void *context);

//! Write byte to slave port and return the corresponding received byte
uint8_t spi_slave_read_write(const SPISlavePort *slave, uint8_t out);

//! Write single data byte to the given slave port
void spi_slave_write(const SPISlavePort *slave, uint8_t out);

//! Read a burst of bytes from given slave port (asserts SCS)
//! 0 bytes are sent to the slave port to prompt the incoming bytes
void spi_slave_burst_read(const SPISlavePort *slave, void *in, size_t len);

//! Write a burst of bytes to the given slave port (asserts SCS)
//! No data is received or waited for
void spi_slave_burst_write(const SPISlavePort *slave, const void *out, size_t len);

//! Transmit and Receive data bytes to and from the SPI Slave Port
//! If out is NULL then 0s are transmitted, if in is NULL incoming data is not saved
void spi_slave_burst_read_write(const SPISlavePort *slave, const void *out, void *in, size_t len);

//! Transmit and Receive data bytes to and from the SPI Slave Port (scatter gather)
void spi_slave_burst_read_write_scatter(const SPISlavePort *slave,
                                       const SPIScatterGather *sc_info,
                                       size_t num_sg);

//! Set (or change) the clock frequency for the given SPI Slave Port (Hz)
void spi_slave_set_frequency(const SPISlavePort *slave, uint32_t frequency_hz);

//! Wait until the SPI Slave is idle
void spi_slave_wait_until_idle_blocking(const SPISlavePort *slave);

//! Gets the peripheral data register address for setting up DMA
uint32_t spi_get_dma_base_address(const SPISlavePort *slave);

//
//! Low level slave port interface
//! This part of the API can be used for slightly more complex SPI operations
//! (such as piecemeal reads or writes).  Assertion and deassertion of SCS
//! is up to the caller.  Asserts in the code will help to ensure that the
//! API is used correctly.
//

//! The general use case for the _ll_ functions:
//!
//! acquire the Slave Port (start peripheral clock and enable SPI)
//!   spi_ll_slave_acquire(SLAVE);
//!   spi_ll_slave_scs_assert(SLAVE);
//! perform (multiple) reads and writes as required
//!   spi_ll_XXXX(SLAVE, ...);
//!   spi_ll_XXXX(SLAVE, ...);
//!   spi_ll_XXXX(SLAVE, ...);
//! release the Slave Port (stop peripheral clock and disable SPI)
//!   spi_ll_slave_scs_deassert(SLAVE);
//!   spi_ll_slave_release(SLAVE);
//!
//! Using the _ll_ routines it is also possible to perform slightly
//! odd SPI transactions such as transmitting while SCS is not asserted
//! or starting/stopping the clock while the SCS is asserted.
//! These routines can also be used to adjust for timing requirements
//! that some devices have.

//! Acquire the SPI device for use by spi_ll_XXXX functions
//! All spi_ll_XXXX have asserts to check for acquisition
//! Note: does not guarantee exclusivity for right now
//! but could easily do one day if we ever share a SPI
//! bus between multiple Slave Ports
void spi_ll_slave_acquire(const SPISlavePort *slave);

//! Release the SPI Slave Port
void spi_ll_slave_release(const SPISlavePort *slave);

//! Assert the SCS for the given SPI Slave Port
void spi_ll_slave_scs_assert(const SPISlavePort *slave);

//! Deassert the SCS for the given SPI slave port
void spi_ll_slave_scs_deassert(const SPISlavePort *slave);

//! Write byte to slave port and return the corresponding received byte
//! It is up to the caller to ensure SCS is asserted correctly
uint8_t spi_ll_slave_read_write(const SPISlavePort *slave, uint8_t out);

//! Write single data byte to the given slave port
//! It is up to the caller to ensure SCS is asserted correctly
void spi_ll_slave_write(const SPISlavePort *slave, uint8_t out);

//! Read a burst of bytes from given slave port
//! 0 bytes are sent to the slave port to prompt the incoming bytes
//! It is up to the caller to ensure SCS is asserted correctly
void spi_ll_slave_burst_read(const SPISlavePort *slave, void *in, size_t len);

//! Write a burst of bytes to the given slave port
//! No data is received or waited for
//! It is up to the caller to ensure SCS is asserted correctly
void spi_ll_slave_burst_write(const SPISlavePort *slave, const void *out, size_t len);

//! Transmit and Receive data bytes to and from the SPI Slave Port
//! If out is NULL then 0s are transmitted, if in is NULL incoming data is not saved
//! It is up to the caller to ensure SCS is asserted correctly
void spi_ll_slave_burst_read_write(const SPISlavePort *slave,
                                   const void *out,
                                   void *in,
                                   size_t len);

//! Transmit and Receive data bytes to and from the SPI Slave Port (scatter gather)
//! It is up to the caller to ensure SCS is asserted correctly
void spi_ll_slave_burst_read_write_scatter(const SPISlavePort *slave,
                                           const SPIScatterGather *sc_info,
                                           size_t num_sg);

//! Reads data from the given slave port via DMA
void spi_ll_slave_read_dma_start(const SPISlavePort *slave, void *in, size_t len,
                                 SPIDMACompleteHandler handler, void *context);

//! Stops the read DMA on the given slave port
void spi_ll_slave_read_dma_stop(const SPISlavePort *slave);

//! Write data to the given slave port via DMA
void spi_ll_slave_write_dma_start(const SPISlavePort *slave, const void *out, size_t len,
                                  SPIDMACompleteHandler handler, void *context);

//! Stops the write DMA on the given slave port
void spi_ll_slave_write_dma_stop(const SPISlavePort *slave);

//! Sends and receives data via DMA on the given slave port. If out is NULL, 0s are transmitted. If
//! in is NULL, incoming data is not saved.
void spi_ll_slave_read_write_dma_start(const SPISlavePort *slave, const void *out, void *in,
                                       size_t len, SPIDMACompleteHandler handler, void *context);

//! Stops the read + write DMA on the given slave port
void spi_ll_slave_read_write_dma_stop(const SPISlavePort *slave);

//! Checks whether a DMA operation is in progress on the given slave port
bool spi_ll_slave_dma_in_progress(const SPISlavePort *slave);

//! The state of the SPI clock lines between transactions is unspecified. Some devices
//! expect the clock to be in a certain state. This routine will drive the line low when
//! enable is true and reconfigure it as a SPI CLK line when false
void spi_ll_slave_drive_clock(const SPISlavePort *slave, bool enable);

//! Clears any errors which may be set
void spi_ll_slave_clear_errors(const SPISlavePort *slave);

//! Initialize a single SPI device instance. Must be called before first use
void spi_slave_port_init(SPISlavePort *device);

//! Deinitializes the SPI device
void spi_slave_port_deinit(const SPISlavePort *slave);

// REVISIT:
// This prototype is used by the roll-your-own SPI drivers.
// It (and the definition) can go away once the new driver
// API is adopted universally.
uint16_t spi_find_prescaler(uint32_t bus_frequency, SpiPeriphClock periph_clock);
