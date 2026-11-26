/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/nrf54l.h>
#include <pbl/drivers/spi/nrf54l.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/nrf54l_spim.h>

// EasyDMA only reaches RAM
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xe0000000U) == 0x20000000U)

static const struct pbl_spi_nrf54l *prv_spi(const struct pbl_spi_bus *bus) {
  return PBL_CONTAINER_OF(bus, const struct pbl_spi_nrf54l, bus);
}

static inline uint32_t prv_getreg(const struct pbl_spi_nrf54l *spi, uint32_t offset) {
  return pbl_readl(spi->base + offset);
}

static inline void prv_putreg(const struct pbl_spi_nrf54l *spi, uint32_t offset, uint32_t value) {
  pbl_writel(value, spi->base + offset);
}

static void prv_event_clear(const struct pbl_spi_nrf54l *spi, uint32_t offset) {
  prv_putreg(spi, offset, 0U);
  (void)prv_getreg(spi, offset);
}

static uint32_t prv_psel(const struct pbl_gpio *gpio) {
  return pbl_gpio_is_connected(gpio) ? pbl_gpio_nrf54l_psel(gpio) : SPIM_PSEL_DISCONNECTED;
}

static int prv_pin_init(const struct pbl_gpio *gpio, uint32_t flags) {
  if (!pbl_gpio_is_connected(gpio)) {
    return 0;
  }

  if (pbl_device_init(&gpio->port->dev) != 0) {
    return -ENODEV;
  }

  return pbl_gpio_configure(gpio, flags);
}

static int prv_init(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_nrf54l *spi = prv_spi(bus);
  int ret;

  if (!pbl_gpio_is_connected(&spi->sck)) {
    return -EINVAL;
  }

  // SCK needs its input buffer connected
  ret = prv_pin_init(&spi->sck, PBL_GPIO_OUTPUT_LOW | PBL_GPIO_INPUT);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pin_init(&spi->mosi, PBL_GPIO_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pin_init(&spi->miso, PBL_GPIO_INPUT);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(spi, NRF54L_SPIM_PSEL_SCK_OFFSET, prv_psel(&spi->sck));
  prv_putreg(spi, NRF54L_SPIM_PSEL_MOSI_OFFSET, prv_psel(&spi->mosi));
  prv_putreg(spi, NRF54L_SPIM_PSEL_MISO_OFFSET, prv_psel(&spi->miso));
  prv_putreg(spi, NRF54L_SPIM_PSEL_DCX_OFFSET, SPIM_PSEL_DISCONNECTED);
  prv_putreg(spi, NRF54L_SPIM_PSEL_CSN_OFFSET, SPIM_PSEL_DISCONNECTED);

  pbl_irq_enable(spi->irq);

  return 0;
}

static int prv_configure(const struct pbl_spi_bus *bus, const struct pbl_spi_dev *dev) {
  const struct pbl_spi_nrf54l *spi = prv_spi(bus);
  const uint32_t hz = PBL_MIN(dev->max_hz, spi->max_hz);
  uint32_t divisor;
  uint32_t config = 0U;

  if (hz == 0U) {
    return -EINVAL;
  }

  // The divisor is even, and rounds up so that SCK stays at or below the requested frequency
  divisor = PBL_DIV_ROUND_UP(spi->clock_hz, hz);
  divisor += divisor & 1U;
  if (divisor > SPIM_PRESCALER_DIVISOR_MASK) {
    return -EINVAL;
  }

  if ((dev->mode & PBL_SPI_LSB_FIRST) != 0U) {
    config |= SPIM_CONFIG_ORDER_LSBFIRST;
  }

  if ((dev->mode & PBL_SPI_CPHA) != 0U) {
    config |= SPIM_CONFIG_CPHA_TRAILING;
  }

  if ((dev->mode & PBL_SPI_CPOL) != 0U) {
    config |= SPIM_CONFIG_CPOL_ACTIVELOW;
  }

  // The SCK idle level is driven by the GPIO while the SPIM is disabled
  (void)pbl_gpio_set_raw(&spi->sck, (dev->mode & PBL_SPI_CPOL) != 0U);

  prv_putreg(spi, NRF54L_SPIM_PRESCALER_OFFSET, divisor);
  prv_putreg(spi, NRF54L_SPIM_CONFIG_OFFSET, config);

  return 0;
}

static void prv_enable(const struct pbl_spi_bus *bus) {
  prv_putreg(prv_spi(bus), NRF54L_SPIM_ENABLE_OFFSET, SPIM_ENABLE_ENABLED);
}

static void prv_disable(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_nrf54l *spi = prv_spi(bus);

  prv_putreg(spi, NRF54L_SPIM_INTENCLR_OFFSET, SPIM_INT_END);
  prv_putreg(spi, NRF54L_SPIM_ENABLE_OFFSET, SPIM_ENABLE_DISABLED);
  prv_event_clear(spi, NRF54L_SPIM_EVENTS_END_OFFSET);
  pbl_irq_clear_pending(spi->irq);
}

static int prv_start(const struct pbl_spi_bus *bus, const struct pbl_spi_xfer *xfer) {
  const struct pbl_spi_nrf54l *spi = prv_spi(bus);

  if (xfer->len > SPIM_MAXCNT_MASK || (xfer->tx_buf != nullptr && !IS_IN_RAM(xfer->tx_buf)) ||
      (xfer->rx_buf != nullptr && !IS_IN_RAM(xfer->rx_buf))) {
    return -EINVAL;
  }

  prv_putreg(spi, NRF54L_SPIM_DMA_TX_PTR_OFFSET, (uintptr_t)xfer->tx_buf);
  prv_putreg(spi, NRF54L_SPIM_DMA_TX_MAXCNT_OFFSET, (xfer->tx_buf != nullptr) ? xfer->len : 0U);
  prv_putreg(spi, NRF54L_SPIM_DMA_RX_PTR_OFFSET, (uintptr_t)xfer->rx_buf);
  prv_putreg(spi, NRF54L_SPIM_DMA_RX_MAXCNT_OFFSET, (xfer->rx_buf != nullptr) ? xfer->len : 0U);

  prv_event_clear(spi, NRF54L_SPIM_EVENTS_END_OFFSET);
  prv_putreg(spi, NRF54L_SPIM_INTENSET_OFFSET, SPIM_INT_END);
  prv_putreg(spi, NRF54L_SPIM_TASKS_START_OFFSET, 1U);

  return 0;
}

void pbl_spi_nrf54l_irq_handler(const struct pbl_spi_nrf54l *spi) {
  if (prv_getreg(spi, NRF54L_SPIM_EVENTS_END_OFFSET) == 0U) {
    return;
  }

  prv_event_clear(spi, NRF54L_SPIM_EVENTS_END_OFFSET);
  prv_putreg(spi, NRF54L_SPIM_INTENCLR_OFFSET, SPIM_INT_END);

  pbl_spi_bus_complete(&spi->bus, 0);
}

const struct pbl_spi_bus_ops pbl_spi_nrf54l_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
};

#include PBL_DT_INSTANCES(nordic_nrf54l15_spim)
