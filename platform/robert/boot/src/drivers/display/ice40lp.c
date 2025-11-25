/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ice40lp.h"

#include "board/board.h"
#include "drivers/dbgserial.h"
#include "drivers/display/ice40lp_definitions.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/delay.h"

#include "stm32f7xx.h"
#include "misc.h"

#include <string.h>



static void prv_spi_init(void) {
  // Configure the GPIO (SCLK, MOSI - no MISO since the SPI is TX-only)
  gpio_af_init(&ICE40LP->spi.clk, GPIO_OType_PP, GPIO_Speed_25MHz, GPIO_PuPd_NOPULL);
  gpio_af_init(&ICE40LP->spi.mosi, GPIO_OType_PP, GPIO_Speed_25MHz, GPIO_PuPd_NOPULL);

  // Reset the SPI peripheral and enable the clock
  RCC_APB2PeriphResetCmd(ICE40LP->spi.rcc_bit, ENABLE);
  RCC_APB2PeriphResetCmd(ICE40LP->spi.rcc_bit, DISABLE);
  periph_config_enable(ICE40LP->spi.periph, ICE40LP->spi.rcc_bit);

  // Configure CR1 first:
  //  * TX-only mode (BIDIMODE | BIDIOE)
  //  * software control NSS pin (SSM | SSI)
  //  * master mode (MSTR)
  //  * clock polarity high / 2nd edge (CPOL | CPHA)
  ICE40LP->spi.periph->CR1 = SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | SPI_CR1_SSM | SPI_CR1_SSI |
                             SPI_CR1_MSTR | SPI_CR1_CPOL | SPI_CR1_CPHA;

  // Configure CR2:
  //  * 8-bit data size (DS[4:0] == 0b0111)
  //  * 1/4 RX threshold (for 8-bit transfers)
  ICE40LP->spi.periph->CR2 = SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_FRXTH;

  // enable the SPI
  ICE40LP->spi.periph->CR1 |= SPI_CR1_SPE;
}

static void prv_spi_write(const uint8_t *data, uint32_t len) {
  for (uint32_t i = 0; i < len; ++i) {
    // Wait until we can transmit.
    while (!(ICE40LP->spi.periph->SR & SPI_SR_TXE)) continue;

    // Write a byte. STM32F7 needs to access as 8 bits in order to actually do 8 bits.
    *(volatile uint8_t*)&ICE40LP->spi.periph->DR = data[i];
  }

  // Wait until the TX FIFO is empty plus an extra little bit for the shift-register.
  while (((ICE40LP->spi.periph->SR & SPI_SR_FTLVL) >> __builtin_ctz(SPI_SR_FTLVL)) > 0) continue;
  delay_us(10);
}

bool display_busy(void) {
  return gpio_input_read(&ICE40LP->busy);
}

void display_start(void) {
  // Configure SCS before CRESET and before configuring the SPI so that we don't end up with the
  // FPGA in the "SPI Master Configuration Interface" on bigboards which don't have NVCM. If we end
  // up in this mode, the FPGA will drive the clock and put the SPI peripheral in a bad state.
  gpio_output_init(&ICE40LP->spi.scs, GPIO_OType_PP, GPIO_Speed_25MHz);
  gpio_output_set(&ICE40LP->spi.scs, false);
  gpio_input_init(&ICE40LP->cdone);
  gpio_input_init(&ICE40LP->busy);
  gpio_output_init(&ICE40LP->creset, GPIO_OType_OD, GPIO_Speed_25MHz);

  prv_spi_init();
}

bool display_program(const uint8_t *fpga_bitstream, uint32_t bitstream_size) {
  InputConfig creset_input = {
    .gpio = ICE40LP->creset.gpio,
    .gpio_pin = ICE40LP->creset.gpio_pin,
  };

  delay_ms(1);

  gpio_output_set(&ICE40LP->spi.scs, true);  // SCS asserted (low)
  gpio_output_set(&ICE40LP->creset, false); // CRESET LOW

  delay_ms(1);

  if (gpio_input_read(&creset_input)) {
    dbgserial_putstr("CRESET not low during reset");
    return false;
  }

  gpio_output_set(&ICE40LP->creset, true); // CRESET -> HIGH

  delay_ms(1);

  if (gpio_input_read(&ICE40LP->cdone)) {
    dbgserial_putstr("CDONE not low after reset");
    return false;
  }

  if (!gpio_input_read(&creset_input)) {
    dbgserial_putstr("CRESET not high after reset");
    return false;
  }

  delay_ms(1);

  // Program the FPGA
  prv_spi_write(fpga_bitstream, bitstream_size);

  // Set SCS high so that we don't process any of these clocks as commands.
  gpio_output_set(&ICE40LP->spi.scs, false);  // SCS not asserted (high)

  // 49+ SCLK cycles to tell FPGA we're done configuration.
  static const uint8_t spi_zeros[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  prv_spi_write(spi_zeros, sizeof(spi_zeros));

  if (!gpio_input_read(&ICE40LP->cdone)) {
    dbgserial_putstr("CDONE not high after programming");
    return false;
  }
  return true;
}

void display_power_enable(void) {
  // The display requires us to wait 1ms between each power rail coming up. The PMIC
  // initialization brings up the 3.2V rail (VLCD on the display, LD02 on the PMIC) for us, but
  // we still need to wait before turning on the subsequent rails.
  delay_ms(2);

  if (ICE40LP->use_6v6_rail) {
    dbgserial_putstr("Enabling 6v6 (Display VDDC)");
    set_6V6_power_state(true);

    delay_ms(2);
  }

  dbgserial_putstr("Enabling 4v5 (Display VDDP)");
  set_4V5_power_state(true);
}

void display_power_disable(void) {
  dbgserial_putstr("Disabling 4v5 (Display VDDP)");
  set_4V5_power_state(false);

  delay_ms(2);

  if (ICE40LP->use_6v6_rail) {
    dbgserial_putstr("Disabling 6v6 (Display VDDC)");
    set_6V6_power_state(false);

    delay_ms(2);
  }
}

void display_write_cmd(uint8_t cmd, uint8_t *arg, uint32_t arg_len) {
  gpio_output_set(&ICE40LP->spi.scs, true);  // SCS asserted (low)
  delay_us(100);

  prv_spi_write(&cmd, sizeof(cmd));
  if (arg_len) {
    prv_spi_write(arg, arg_len);
  }

  gpio_output_set(&ICE40LP->spi.scs, false);  // SCS not asserted (high)
}
