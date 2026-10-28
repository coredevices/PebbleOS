/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/spi/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_spim.h>

// EasyDMA only reaches data RAM
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xe0000000U) == 0x20000000U)

// nRF52840 anomaly 195: SPIM3 keeps drawing current once disabled unless this is written
#define SPIM3_ANOMALY_195_ADDR (NRF52_SPIM3_BASE + 0x0004U)

static const struct {
  uintptr_t base;
  pbl_irq_t irqn;
} s_instances[] = {
  {NRF52_SPIM0_BASE, PBL_IRQN(SPI0_SPIM0_SPIS0_TWI0_TWIM0_TWIS0)},
  {NRF52_SPIM1_BASE, PBL_IRQN(SPI1_SPIM1_SPIS1_TWI1_TWIM1_TWIS1)},
  {NRF52_SPIM2_BASE, PBL_IRQN(SPI2_SPIM2_SPIS2)},
  {NRF52_SPIM3_BASE, PBL_IRQN(SPIM3)},
};

static const struct {
  uint32_t hz;
  uint32_t frequency;
} s_frequencies[] = {
  {32000000U, SPIM_FREQUENCY_M32}, {16000000U, SPIM_FREQUENCY_M16}, {8000000U, SPIM_FREQUENCY_M8},
  {4000000U, SPIM_FREQUENCY_M4},   {2000000U, SPIM_FREQUENCY_M2},   {1000000U, SPIM_FREQUENCY_M1},
  {500000U, SPIM_FREQUENCY_K500},  {250000U, SPIM_FREQUENCY_K250},  {125000U, SPIM_FREQUENCY_K125},
};

static const struct pbl_spi_nrf5 *prv_spi(const struct pbl_spi_bus *bus) {
  return container_of(bus, const struct pbl_spi_nrf5, bus);
}

static inline uint32_t prv_getreg(const struct pbl_spi_nrf5 *spi, uint32_t offset) {
  return pbl_readl(s_instances[spi->index].base + offset);
}

static inline void prv_putreg(const struct pbl_spi_nrf5 *spi, uint32_t offset, uint32_t value) {
  pbl_writel(value, s_instances[spi->index].base + offset);
}

static void prv_event_clear(const struct pbl_spi_nrf5 *spi, uint32_t offset) {
  prv_putreg(spi, offset, 0U);
  (void)prv_getreg(spi, offset);
}

static uint32_t prv_psel(const struct pbl_gpio *gpio) {
  return pbl_gpio_is_connected(gpio) ? pbl_gpio_nrf5_pin(gpio) : SPIM_PSEL_DISCONNECTED;
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
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);
  int ret;

  if (spi->index >= ARRAY_LENGTH(s_instances) || !pbl_gpio_is_connected(&spi->sck)) {
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

  prv_putreg(spi, NRF52_SPIM_PSEL_SCK_OFFSET, prv_psel(&spi->sck));
  prv_putreg(spi, NRF52_SPIM_PSEL_MOSI_OFFSET, prv_psel(&spi->mosi));
  prv_putreg(spi, NRF52_SPIM_PSEL_MISO_OFFSET, prv_psel(&spi->miso));

  pbl_irq_enable(s_instances[spi->index].irqn);

  return 0;
}

static int prv_configure(const struct pbl_spi_bus *bus, const struct pbl_spi_dev *dev) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);
  uint32_t config = 0U;
  size_t i;

  // Only SPIM3 runs above 8 MHz
  for (i = (spi->index == 3U) ? 0U : 2U; i < ARRAY_LENGTH(s_frequencies); i++) {
    if (s_frequencies[i].hz <= dev->max_hz) {
      break;
    }
  }

  if (i == ARRAY_LENGTH(s_frequencies)) {
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

  prv_putreg(spi, NRF52_SPIM_FREQUENCY_OFFSET, s_frequencies[i].frequency);
  prv_putreg(spi, NRF52_SPIM_CONFIG_OFFSET, config);

  return 0;
}

static void prv_enable(const struct pbl_spi_bus *bus) {
  prv_putreg(prv_spi(bus), NRF52_SPIM_ENABLE_OFFSET, SPIM_ENABLE_ENABLED);
}

static void prv_disable(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);

  prv_putreg(spi, NRF52_SPIM_INTENCLR_OFFSET, SPIM_INT_END);
  prv_putreg(spi, NRF52_SPIM_ENABLE_OFFSET, SPIM_ENABLE_DISABLED);
  if (spi->index == 3U) {
    pbl_writel(1U, SPIM3_ANOMALY_195_ADDR);
  }
  prv_event_clear(spi, NRF52_SPIM_EVENTS_END_OFFSET);
  pbl_irq_clear_pending(s_instances[spi->index].irqn);
}

static int prv_start(const struct pbl_spi_bus *bus, const struct pbl_spi_xfer *xfer) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);

  if (xfer->len > SPIM_MAXCNT_MASK || (xfer->tx_buf != nullptr && !IS_IN_RAM(xfer->tx_buf)) ||
      (xfer->rx_buf != nullptr && !IS_IN_RAM(xfer->rx_buf))) {
    return -EINVAL;
  }

  prv_putreg(spi, NRF52_SPIM_TXD_PTR_OFFSET, (uintptr_t)xfer->tx_buf);
  prv_putreg(spi, NRF52_SPIM_TXD_MAXCNT_OFFSET, (xfer->tx_buf != nullptr) ? xfer->len : 0U);
  prv_putreg(spi, NRF52_SPIM_RXD_PTR_OFFSET, (uintptr_t)xfer->rx_buf);
  prv_putreg(spi, NRF52_SPIM_RXD_MAXCNT_OFFSET, (xfer->rx_buf != nullptr) ? xfer->len : 0U);

  prv_event_clear(spi, NRF52_SPIM_EVENTS_END_OFFSET);
  prv_putreg(spi, NRF52_SPIM_INTENSET_OFFSET, SPIM_INT_END);
  prv_putreg(spi, NRF52_SPIM_TASKS_START_OFFSET, 1U);

  return 0;
}

void pbl_spi_nrf5_irq_handler(const struct pbl_spi_nrf5 *spi) {
  if (prv_getreg(spi, NRF52_SPIM_EVENTS_END_OFFSET) == 0U) {
    return;
  }

  prv_event_clear(spi, NRF52_SPIM_EVENTS_END_OFFSET);
  prv_putreg(spi, NRF52_SPIM_INTENCLR_OFFSET, SPIM_INT_END);

  pbl_spi_bus_complete(&spi->bus, 0);
}

const struct pbl_spi_bus_ops pbl_spi_nrf5_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
};
