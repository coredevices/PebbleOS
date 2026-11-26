/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/spi/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_spim.h>

// EasyDMA only reaches data RAM
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xe0000000U) == 0x20000000U)

// nRF52840 anomaly 195: SPIM3 keeps drawing current once disabled unless this is written
#define SPIM3_ANOMALY_195_OFFSET 0x0004U

static const struct {
  uint32_t hz;
  uint32_t frequency;
} s_frequencies[] = {
  {32000000U, SPIM_FREQUENCY_M32}, {16000000U, SPIM_FREQUENCY_M16}, {8000000U, SPIM_FREQUENCY_M8},
  {4000000U, SPIM_FREQUENCY_M4},   {2000000U, SPIM_FREQUENCY_M2},   {1000000U, SPIM_FREQUENCY_M1},
  {500000U, SPIM_FREQUENCY_K500},  {250000U, SPIM_FREQUENCY_K250},  {125000U, SPIM_FREQUENCY_K125},
};

static const struct pbl_spi_nrf5 *prv_spi(const struct pbl_spi_bus *bus) {
  return PBL_CONTAINER_OF(bus, const struct pbl_spi_nrf5, bus);
}

static inline uint32_t prv_getreg(const struct pbl_spi_nrf5 *spi, uint32_t offset) {
  return pbl_readl(spi->base + offset);
}

static inline void prv_putreg(const struct pbl_spi_nrf5 *spi, uint32_t offset, uint32_t value) {
  pbl_writel(value, spi->base + offset);
}

static void prv_event_clear(const struct pbl_spi_nrf5 *spi, uint32_t offset) {
  prv_putreg(spi, offset, 0U);
  (void)prv_getreg(spi, offset);
}

static uint32_t prv_psel(const struct pbl_gpio *gpio) {
  return pbl_gpio_is_connected(gpio) ? pbl_gpio_nrf5_pin(gpio) : SPIM_PSEL_DISCONNECTED;
}

static int prv_port_init(const struct pbl_gpio *gpio) {
  if (pbl_gpio_is_connected(gpio) && pbl_device_init(&gpio->port->dev) != 0) {
    return -ENODEV;
  }

  return 0;
}

// Drives the lines for a transfer, or releases them to the level the board's pulls give them
static void prv_pins_set(const struct pbl_spi_bus *bus, bool active) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);
  const struct pbl_gpio *pins[] = {&spi->sck, &spi->mosi, &spi->miso};
  uint32_t flags[] = {PBL_GPIO_DISCONNECTED, PBL_GPIO_DISCONNECTED, PBL_GPIO_DISCONNECTED};

  if (active) {
    // SCK idles at CPOL, and needs its input buffer connected
    flags[0] =
        ((bus->state->dev->mode & PBL_SPI_CPOL) != 0U) ? PBL_GPIO_OUTPUT_HIGH : PBL_GPIO_OUTPUT_LOW;
    flags[0] |= PBL_GPIO_INPUT;
    flags[1] = PBL_GPIO_OUTPUT_LOW;
    flags[2] = PBL_GPIO_INPUT;
  }

  for (size_t i = 0U; i < PBL_ARRAY_SIZE(pins); i++) {
    if (pbl_gpio_is_connected(pins[i])) {
      (void)pbl_gpio_configure(pins[i], flags[i]);
    }
  }
}

static int prv_init(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);

  if (!pbl_gpio_is_connected(&spi->sck)) {
    return -EINVAL;
  }

  if (prv_port_init(&spi->sck) != 0 || prv_port_init(&spi->mosi) != 0 ||
      prv_port_init(&spi->miso) != 0) {
    return -ENODEV;
  }

  prv_pins_set(bus, false);

  prv_putreg(spi, NRF52_SPIM_PSEL_SCK_OFFSET, prv_psel(&spi->sck));
  prv_putreg(spi, NRF52_SPIM_PSEL_MOSI_OFFSET, prv_psel(&spi->mosi));
  prv_putreg(spi, NRF52_SPIM_PSEL_MISO_OFFSET, prv_psel(&spi->miso));

  pbl_irq_enable(spi->irq);

  return 0;
}

static int prv_configure(const struct pbl_spi_bus *bus, const struct pbl_spi_dev *dev) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);
  uint32_t config = 0U;
  size_t i;

  // Only SPIM3 runs above 8 MHz
  for (i = (spi->spim3) ? 0U : 2U; i < PBL_ARRAY_SIZE(s_frequencies); i++) {
    if (s_frequencies[i].hz <= dev->max_hz) {
      break;
    }
  }

  if (i == PBL_ARRAY_SIZE(s_frequencies)) {
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

  prv_putreg(spi, NRF52_SPIM_FREQUENCY_OFFSET, s_frequencies[i].frequency);
  prv_putreg(spi, NRF52_SPIM_CONFIG_OFFSET, config);

  return 0;
}

// Called before the chip select is asserted; SCK is driven at CPOL before the SPIM takes it
static void prv_enable(const struct pbl_spi_bus *bus) {
  prv_pins_set(bus, true);
  prv_putreg(prv_spi(bus), NRF52_SPIM_ENABLE_OFFSET, SPIM_ENABLE_ENABLED);
}

static void prv_disable(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_nrf5 *spi = prv_spi(bus);

  prv_putreg(spi, NRF52_SPIM_INTENCLR_OFFSET, SPIM_INT_END);
  prv_putreg(spi, NRF52_SPIM_ENABLE_OFFSET, SPIM_ENABLE_DISABLED);
  if (spi->spim3) {
    prv_putreg(spi, SPIM3_ANOMALY_195_OFFSET, 1U);
  }
  prv_event_clear(spi, NRF52_SPIM_EVENTS_END_OFFSET);
  pbl_irq_clear_pending(spi->irq);

  // Called once the chip select is released
  prv_pins_set(bus, false);
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

#include PBL_DT_INSTANCES(nordic_nrf52832_spim)
