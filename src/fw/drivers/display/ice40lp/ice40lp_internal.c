/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/display/ice40lp/ice40lp_internal.h"

#include "board/board.h"
#include "debug/power_tracking.h"
#include "drivers/display/ice40lp/ice40lp_definitions.h"
#include "drivers/periph_config.h"
#include "drivers/gpio.h"
#include "drivers/spi.h"
#include "drivers/exti.h"
#include "drivers/pmic.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/delay.h"
#include "util/size.h"
#include "util/sle.h"
#include "kernel/util/sleep.h"
#include "util/units.h"

#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <string.h>

bool display_busy(void) {
  return gpio_input_read(&ICE40LP->busy);
}

void display_spi_begin_transaction(void) {
  spi_ll_slave_acquire(ICE40LP->spi_port);
  spi_ll_slave_scs_assert(ICE40LP->spi_port);
  power_tracking_start(PowerSystemMcuSpi6);
}

void display_spi_end_transaction(void) {
  spi_ll_slave_scs_deassert(ICE40LP->spi_port);
  spi_ll_slave_release(ICE40LP->spi_port);
  power_tracking_stop(PowerSystemMcuSpi6);
}

// Temporary code to support prv_do_display_update() logic that attempts to use the
// bootloader error display
void display_spi_configure_default(void) {
  spi_slave_set_frequency(ICE40LP->spi_port, ICE40LP->base_spi_frequency);
}

void display_start() {
  periph_config_acquire_lock();

  gpio_output_init(&ICE40LP->creset, GPIO_OType_OD, GPIO_Speed_25MHz);
  gpio_input_init(&ICE40LP->cdone);
  gpio_input_init(&ICE40LP->busy);

  periph_config_release_lock();
}

static bool prv_spin_until_creset_is(bool level) {
  int timeout_us = 500 * 1000;
  InputConfig creset_input = {
    .gpio = ICE40LP->creset.gpio,
    .gpio_pin = ICE40LP->creset.gpio_pin,
  };
  while (timeout_us > 0) {
    if (gpio_input_read(&creset_input) == level) return true;
    delay_us(100);
    timeout_us -= 100;
  }
  return false;
}

static bool prv_wait_programmed(void) {
  // The datasheet lists the typical NVCM configuration time as 56 ms.
  // Something is wrong if it takes more than twice that time.
  int timeout = 100 * 10;  // * 100 microseconds
  while (!gpio_input_read(&ICE40LP->cdone)) {
    if (timeout-- == 0) {
      PBL_LOG(LOG_LEVEL_ERROR, "FPGA CDONE timeout expired!");
      return false;
    }
    delay_us(100);
  }
  return true;
}

static bool prv_try_program(const uint8_t *fpga_bitstream,
                            uint32_t bitstream_size) {
  display_spi_configure_default();
  spi_ll_slave_acquire(ICE40LP->spi_port);
  spi_ll_slave_scs_assert(ICE40LP->spi_port);

  gpio_output_set(&ICE40LP->creset, false); // CRESET LOW

#if !defined(TARGET_QEMU)
  // Wait until we succeed in pulling CRESET down against the external pull-up
  // and other external circuitry which is fighting against us.
  PBL_ASSERT(prv_spin_until_creset_is(false), "CRESET not low during reset");

  // CRESET needs to be low for 200 ns to actually reset the FPGA.
  delay_us(10);
#endif

  gpio_output_set(&ICE40LP->creset, true); // CRESET -> HIGH

#if !defined(TARGET_QEMU)
  PBL_ASSERT(!gpio_input_read(&ICE40LP->cdone), "CDONE not low after reset");

  // Wait until CRESET goes high again. It's open-drain (and someone with
  // tweezers might be grounding it) so it may take some time.
  PBL_ASSERT(prv_spin_until_creset_is(true), "CRESET not high after reset");

  // iCE40 Programming and Configuration manual specifies that the iCE40 needs
  // 800 µs for "housekeeping" after reset is released before it is ready to
  // receive its configuration.
  delay_us(1000);
#endif


  SLEDecodeContext sle_ctx;
  sle_decode_init(&sle_ctx, fpga_bitstream);
  uint8_t byte;
  while (sle_decode(&sle_ctx, &byte)) {
    spi_ll_slave_write(ICE40LP->spi_port, byte);
  }

  // Set SCS high so that we don't process any of these clocks as commands.
  spi_ll_slave_scs_deassert(ICE40LP->spi_port);

  static const uint8_t spi_zeros[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

  // 49+ SCLK cycles to tell FPGA we're done configuration.
  spi_ll_slave_burst_write(ICE40LP->spi_port, spi_zeros, ARRAY_LENGTH(spi_zeros));

  spi_ll_slave_release(ICE40LP->spi_port);

  // PBL-19516
#if !defined(TARGET_QEMU)
  prv_wait_programmed();
  if (!gpio_input_read(&ICE40LP->cdone)) {
    PBL_LOG(LOG_LEVEL_ERROR, "CDONE not high after programming");
    return false;
  }
#endif
  return true;
}

void display_program(const uint8_t *fpga_bitstream, uint32_t bitstream_size) {
  periph_config_acquire_lock();

  int attempt = 1;
  while (1) {
    if (prv_try_program(fpga_bitstream, bitstream_size)) {
      break;
    }
    if (attempt++ >= 3) {
      PBL_CROAK("Too many failed FPGA programming attempts");
    }
  }

  spi_slave_set_frequency(ICE40LP->spi_port, ICE40LP->fast_spi_frequency);

  periph_config_release_lock();
}

bool display_switch_to_bootloader_mode(void) {
  // Reset the FPGA and wait for it to program itself via NVCM.
  // NVCM configuration is initiated by pulling CRESET high while SCS is high.
  periph_config_acquire_lock();
  // SCS will already be high here.

  // CRESET needs to be low for at least 200 ns
  gpio_output_set(&ICE40LP->creset, false);
  delay_us(1000);
  gpio_output_set(&ICE40LP->creset, true);
  bool success = prv_wait_programmed();
  if (success) {
    display_spi_configure_default();
  }
  periph_config_release_lock();
  return success;
}

void display_power_enable(void) {
  // The display requires us to wait 1ms between each power rail coming up. The PMIC
  // initialization brings up the 3.2V rail (VLCD on the display, LD02 on the PMIC) for us, but
  // we still need to wait before turning on the subsequent rails.
  psleep(2);

  if (ICE40LP->use_6v6_rail) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Enabling 6v6 (Display VDDC)");
    set_6V6_power_state(true);

    psleep(2);
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Enabling 4v5 (Display VDDP)");
  set_4V5_power_state(true);
}

void display_power_disable(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Disabling 4v5 (Display VDDP)");
  set_4V5_power_state(false);

  psleep(2);

  if (ICE40LP->use_6v6_rail) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Disabling 6v6 (Display VDDC)");
    set_6V6_power_state(false);

    psleep(2);
  }
}

//!
//! Starts a frame.
//!
void display_start_frame(void) {
  // The iCE40UL framebuffer FPGA (S4) configuration requires a short delay
  // after asserting SCS before it is ready for a command.
  delay_us(5);

  spi_ll_slave_write(ICE40LP->spi_port, CMD_FRAME_BEGIN);

  // Make sure command has been transferred.
  spi_slave_wait_until_idle_blocking(ICE40LP->spi_port);
}
