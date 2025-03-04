#include "spi_definitions.h"
#include "drivers/spi.h"
#include "drivers/spi_dma.h"

#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "system/passert.h"
#include "util/math.h"
#include "util/units.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

/* XXX: this really needs to be refactored to allow mutual exclusion between two SPISlavePorts on a single SPIBus (and other things) */

static void prv_spi_bus_deinit(const SPIBus *bus) {
  bus->state->initialized = false;
}

void prv_spi_bus_init(const SPIBus *bus) {
  if (bus->state->initialized) {
    return;
  }
  // copy the speed over to the transient state since the slave port can change it
  bus->state->spi_clock_speed_hz = bus->spi_clock_speed_hz;
  bus->state->initialized = true;
}

static void prv_spi_slave_init(const SPISlavePort *slave, bool is_reinit) {
  SPIBus *bus = slave->spi_bus;

  nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(
    bus->spi_sclk, bus->spi_mosi, bus->spi_miso, NRF_SPIM_PIN_NOT_CONNECTED);
  config.frequency = bus->state->spi_clock_speed_hz;
  config.mode = (slave->spi_cpol == SpiCPol_Low  && slave->spi_cpha == SpiCPha_1Edge) ? NRF_SPIM_MODE_0 :
                (slave->spi_cpol == SpiCPol_Low  && slave->spi_cpha == SpiCPha_2Edge) ? NRF_SPIM_MODE_1 :
                (slave->spi_cpol == SpiCPol_High && slave->spi_cpha == SpiCPha_1Edge) ? NRF_SPIM_MODE_2 :
                                                                                        NRF_SPIM_MODE_3;
  config.bit_order = slave->spi_first_bit == SpiFirstBit_MSB ? NRF_SPIM_BIT_ORDER_MSB_FIRST : NRF_SPIM_BIT_ORDER_LSB_FIRST;

  nrfx_err_t rv;
  if (is_reinit) {
    nrfx_spim_uninit(&bus->spi);
    rv = nrfx_spim_init(&bus->spi, &config, NULL /* always in blocking mode! */, NULL);
  } else {
    rv = nrfx_spim_init(&bus->spi, &config, NULL /* always in blocking mode! */, NULL);
  }
  PBL_ASSERTN(rv == NRFX_SUCCESS);
}

static void prv_spi_slave_deinit(const SPISlavePort *slave) {
  spi_ll_slave_acquire(slave);
  SPIBus *bus = slave->spi_bus;
  nrfx_spim_uninit(&bus->spi);
  spi_ll_slave_release(slave);
}

//
//! High level slave port interface
//! This part of the API can be used for fairly straightforward SPI interactions
//! The assertion and deassertion of the SCS line is automatic
//

void spi_slave_port_deinit(const SPISlavePort *slave) {
  // don't deinitialize twice
  if (!slave->slave_state->initialized) {
    return;
  }
  prv_spi_slave_deinit(slave);
  prv_spi_bus_deinit(slave->spi_bus);
  slave->slave_state->initialized = false;
}

void spi_slave_port_init(const SPISlavePort *slave) {
  // don't initialize twice
  if (slave->slave_state->initialized) {
    return;
  }
  slave->slave_state->initialized = true;
  slave->slave_state->acquired = false;
  slave->slave_state->scs_selected = false;
  prv_spi_bus_init(slave->spi_bus);

  // SCS
  gpio_output_init(&slave->spi_scs, GPIO_OType_PP, slave->spi_bus->spi_sclk_speed);
  gpio_output_set(&slave->spi_scs, false);  // SCS not asserted (high)

  // Set up an SPI
  prv_spi_slave_deinit(slave);

  prv_spi_slave_init(slave, false /* initial init */);
}

static void prv_spi_acquire_helper(const SPISlavePort *slave) {
  spi_ll_slave_acquire(slave);
  spi_ll_slave_scs_assert(slave);
}

static void prv_spi_release_helper(const SPISlavePort *slave) {
  spi_ll_slave_scs_deassert(slave);
  spi_ll_slave_release(slave);
}

uint8_t spi_slave_read_write(const SPISlavePort *slave, uint8_t out) {
  prv_spi_acquire_helper(slave);
  uint8_t ret = spi_ll_slave_read_write(slave, out);
  prv_spi_release_helper(slave);
  return ret;
}

void spi_slave_write(const SPISlavePort *slave, uint8_t out) {
  prv_spi_acquire_helper(slave);
  spi_ll_slave_write(slave, out);
  prv_spi_release_helper(slave);
}

void spi_slave_burst_read(const SPISlavePort *slave, void *in, size_t len) {
  prv_spi_acquire_helper(slave);
  spi_ll_slave_burst_read(slave, in, len);
  prv_spi_release_helper(slave);
}

void spi_slave_burst_write(const SPISlavePort *slave, const void *out, size_t len) {
  prv_spi_acquire_helper(slave);
  spi_ll_slave_burst_write(slave, out, len);
  prv_spi_release_helper(slave);
}

void spi_slave_burst_read_write(const SPISlavePort *slave, const void *out, void *in, size_t len) {
  prv_spi_acquire_helper(slave);
  spi_ll_slave_burst_read_write(slave, out, in, len);
  prv_spi_release_helper(slave);
}

void spi_slave_burst_read_write_scatter(const SPISlavePort *slave,
                             const SPIScatterGather *sc_info,
                             size_t num_sg) {
  prv_spi_acquire_helper(slave);
  spi_ll_slave_burst_read_write_scatter(slave, sc_info, num_sg);
  prv_spi_release_helper(slave);
}

void spi_slave_set_frequency(const SPISlavePort *slave, uint32_t frequency_hz) {
  slave->spi_bus->state->spi_clock_speed_hz = frequency_hz;
  prv_spi_slave_init(slave, true /* is_reinit */);
}

void spi_slave_wait_until_idle_blocking(const SPISlavePort *slave) {
  // "always has been!"
}

#if 0
uint32_t spi_get_dma_base_address(const SPISlavePort *slave) {
  return (uint32_t)&(slave->spi_bus->spi->DR);
}
#endif

//
//! Low level slave port interface
//! This part of the API can be used for slightly more complex SPI operations
//! (such as piecemeal reads or writes).  Assertion and deassertion of SCS
//! is up to the caller.  Asserts in the code will help to ensure that the
//! API is used correctly.
//

void spi_ll_slave_acquire(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired == false);
  slave->slave_state->acquired = true;
  spi_ll_slave_spi_enable(slave);
}

void spi_ll_slave_release(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  spi_ll_slave_spi_disable(slave);
  slave->slave_state->acquired = false;
}

void spi_ll_slave_spi_enable(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
}

void spi_ll_slave_spi_disable(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
}

void spi_ll_slave_scs_assert(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->scs_selected == false);
  slave->slave_state->scs_selected = true;
  gpio_output_set(&slave->spi_scs, true);  // SCS asserted (low)
}

void spi_ll_slave_scs_deassert(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->scs_selected);
  slave->slave_state->scs_selected = false;
  gpio_output_set(&slave->spi_scs, false);  // SCS not asserted (high)
}

uint8_t spi_ll_slave_read_write(const SPISlavePort *slave, uint8_t out) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->scs_selected);
  
  uint8_t in;
  nrfx_spim_xfer_desc_t xfer;
  xfer.p_tx_buffer = &out;
  xfer.tx_length = 1;
  xfer.p_rx_buffer = &in;
  xfer.rx_length = 1;
  
  nrfx_err_t rv = nrfx_spim_xfer(&slave->spi_bus->spi, &xfer, 0);
  PBL_ASSERTN(rv == NRFX_SUCCESS);
  
  return in;
}

void spi_ll_slave_write(const SPISlavePort *slave, uint8_t out) {
  (void) spi_ll_slave_read_write(slave, out);
}

void spi_ll_slave_burst_read(const SPISlavePort *slave, void *in, size_t len) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->scs_selected);
  uint8_t *cptr = in;
  while (len--) {
    *(cptr++) = spi_ll_slave_read_write(slave, 0); // useless write-data
  }
}

void spi_ll_slave_burst_write(const SPISlavePort *slave, const void *out, size_t len) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);

  nrfx_spim_xfer_desc_t xfer;
  xfer.p_tx_buffer = out;
  xfer.tx_length = len;
  xfer.p_rx_buffer = NULL;
  xfer.rx_length = 0;
  
  nrfx_err_t rv = nrfx_spim_xfer(&slave->spi_bus->spi, &xfer, 0);
  PBL_ASSERTN(rv == NRFX_SUCCESS);
}

void spi_ll_slave_burst_read_write(const SPISlavePort *slave,
                                   const void *out,
                                   void *in,
                                   size_t len) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);


  nrfx_spim_xfer_desc_t xfer;
  xfer.p_tx_buffer = out;
  xfer.tx_length = len;
  xfer.p_rx_buffer = in;
  xfer.rx_length = len;
  
  nrfx_err_t rv = nrfx_spim_xfer(&slave->spi_bus->spi, &xfer, 0);
  PBL_ASSERTN(rv == NRFX_SUCCESS);
}

void spi_ll_slave_burst_read_write_scatter(const SPISlavePort *slave,
                                           const SPIScatterGather *sc_info,
                                           size_t num_sg) {
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  for (size_t elem = 0; elem < num_sg; ++elem) {
    const SPIScatterGather *sg = &sc_info[elem];
    spi_ll_slave_burst_read_write(slave, sg->sg_out, sg->sg_in, sg->sg_len);
  }
}

#if 0
/* SPI DMA is not supported on nRF5 yet */

static bool prv_dma_irq_handler(DMARequest *request, void *context) {
  const SPISlavePort *slave = context;
  PBL_ASSERTN(slave);
  bool is_done = false;
  switch (slave->slave_state->dma_state) {
    case SPISlavePortDMAState_Read:
    case SPISlavePortDMAState_Write:
    case SPISlavePortDMAState_ReadWriteOneInterrupt:
      slave->slave_state->dma_state = SPISlavePortDMAState_Idle;
      is_done = true;
      break;
    case SPISlavePortDMAState_ReadWrite:
      slave->slave_state->dma_state = SPISlavePortDMAState_ReadWriteOneInterrupt;
      break;
    case SPISlavePortDMAState_Idle:
    default:
      WTF;
  }
  SPIDMACompleteHandler handler = slave->slave_state->dma_complete_handler;
  if (is_done && handler) {
    return handler(slave, slave->slave_state->dma_complete_context);
  }
  return false;
}

void spi_ll_slave_read_dma_start(const SPISlavePort *slave, void *in, size_t len,
                                 SPIDMACompleteHandler handler, void *context) {
  PBL_ASSERTN(slave->rx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->dma_state == SPISlavePortDMAState_Idle);
  slave->slave_state->dma_state = SPISlavePortDMAState_Read;
  slave->slave_state->dma_complete_handler = handler;
  slave->slave_state->dma_complete_context = context;
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqRx, true);
  dma_request_start_direct(slave->rx_dma, in, (void *)&slave->spi_bus->spi->DR, len,
                           prv_dma_irq_handler, (void *)slave);
}

void spi_ll_slave_read_dma_stop(const SPISlavePort *slave) {
  if (slave->slave_state->dma_state != SPISlavePortDMAState_Read) {
    return;
  }
  dma_request_stop(slave->rx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqRx, false);
  slave->slave_state->dma_complete_handler = NULL;
  slave->slave_state->dma_complete_context = NULL;
}

void spi_ll_slave_write_dma_start(const SPISlavePort *slave, const void *out, size_t len,
                                  SPIDMACompleteHandler handler, void *context) {
  PBL_ASSERTN(slave->tx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->dma_state == SPISlavePortDMAState_Idle);
  slave->slave_state->dma_state = SPISlavePortDMAState_Write;
  slave->slave_state->dma_complete_handler = handler;
  slave->slave_state->dma_complete_context = context;
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqTx, true);
  dma_request_start_direct(slave->tx_dma, (void *)&slave->spi_bus->spi->DR, out, len,
                           prv_dma_irq_handler, (void *)slave);
}

void spi_ll_slave_write_dma_stop(const SPISlavePort *slave) {
  if (slave->slave_state->dma_state != SPISlavePortDMAState_Write) {
    return;
  }
  dma_request_stop(slave->tx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqTx, false);
  slave->slave_state->dma_complete_handler = NULL;
  slave->slave_state->dma_complete_context = NULL;
}

void spi_ll_slave_read_write_dma_start(const SPISlavePort *slave, const void *out, void *in,
                                       size_t len, SPIDMACompleteHandler handler, void *context) {
  PBL_ASSERTN(slave->rx_dma && slave->tx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  PBL_ASSERTN(slave->slave_state->dma_state == SPISlavePortDMAState_Idle);
  slave->slave_state->dma_complete_handler = handler;
  slave->slave_state->dma_complete_context = context;

  if (out) {
    dma_request_set_memory_increment_disabled(slave->tx_dma, false);
  } else {
    dma_request_set_memory_increment_disabled(slave->tx_dma, true);
    static const uint8_t s_zero = 0;
    out = &s_zero;
  }

  if (in) {
    slave->slave_state->dma_state = SPISlavePortDMAState_ReadWrite;
    // start the read DMA
    prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqRx, true);
    dma_request_start_direct(slave->rx_dma, in, (void *)&slave->spi_bus->spi->DR, len,
                             prv_dma_irq_handler, (void *)slave);
  } else {
    slave->slave_state->dma_state = SPISlavePortDMAState_ReadWriteOneInterrupt;
  }
  // start the write DMA
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqTx, true);
  dma_request_start_direct(slave->tx_dma, (void *)&slave->spi_bus->spi->DR, out, len,
                           prv_dma_irq_handler, (void *)slave);
}

void spi_ll_slave_read_write_dma_stop(const SPISlavePort *slave) {
  if ((slave->slave_state->dma_state != SPISlavePortDMAState_ReadWrite) &&
      (slave->slave_state->dma_state != SPISlavePortDMAState_ReadWriteOneInterrupt)) {
    return;
  }
  PBL_ASSERTN(slave->tx_dma && slave->rx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  dma_request_stop(slave->rx_dma);
  dma_request_stop(slave->tx_dma);
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqRx, false);
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqTx, false);
  slave->slave_state->dma_complete_handler = NULL;
  slave->slave_state->dma_complete_context = NULL;
}

bool spi_ll_slave_dma_in_progress(const SPISlavePort *slave) {
  PBL_ASSERTN(slave->tx_dma || slave->rx_dma);
  PBL_ASSERTN(slave->slave_state->initialized);
  PBL_ASSERTN(slave->slave_state->acquired);
  return (slave->rx_dma && dma_request_in_progress(slave->rx_dma)) ||
         (slave->tx_dma && dma_request_in_progress(slave->tx_dma));
}

void spi_ll_slave_set_tx_dma(const SPISlavePort *slave, bool enable) {
  PBL_ASSERTN(slave->slave_state->initialized);
  spi_ll_slave_acquire(slave);
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqTx, enable);
  spi_ll_slave_release(slave);
}

void spi_ll_slave_set_rx_dma(const SPISlavePort *slave, bool enable) {
  PBL_ASSERTN(slave->slave_state->initialized);
  spi_ll_slave_acquire(slave);
  prv_spi_dma_cmd(slave->spi_bus, SpiI2sDma_ReqRx, enable);
  spi_ll_slave_release(slave);
}


void spi_ll_slave_drive_clock(const SPISlavePort *slave, bool enable) {
  const AfConfig *spi_sclk = &slave->spi_bus->spi_sclk;

  if (enable) {
    OutputConfig clk_as_gpio = {
      .gpio = spi_sclk->gpio,
      .gpio_pin = spi_sclk->gpio_pin,
      .active_high = true,
    };
    gpio_output_init(&clk_as_gpio, GPIO_OType_PP, GPIO_Speed_50MHz);
    gpio_output_set(&clk_as_gpio, false);
  } else {
    prv_configure_spi_sclk(spi_sclk, slave->spi_bus->spi_sclk_speed);
  }
}

void spi_ll_slave_clear_errors(const SPISlavePort *slave) {
  // First, empty the RX FIFO by reading the data. If in TX-only mode, it's possible that
  // received data (0x00s) will be left in the RX FIFO.

  // NOTE: Obviously, do not call this function with transfer in progress.
  while (slave->spi_bus->spi->SR & SPI_SR_RXNE) {
    (void)slave->spi_bus->spi->DR;
  }

  // If the FIFO overflowed, the OVR error will be flagged. Clear the error.
  (void)slave->spi_bus->spi->DR;
  (void)slave->spi_bus->spi->SR;
}

#endif
