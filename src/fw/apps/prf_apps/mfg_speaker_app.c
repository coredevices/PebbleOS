/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "board/board.h"
#include "drivers/i2c.h"
#include "drivers/i2c_definitions.h"
#include "drivers/i2c_hal.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "util/size.h"

#define DA7212_CIF_CTRL              0x1D
#define DA7212_DAI_CLK_MODE          0x28
#define DA7212_DAI_CTRL              0x29
#define DA7212_DIG_ROUTING_DAC       0x2A
#define DA7212_DAC_FILTERS5          0x40
#define DA7212_DAC_R_GAIN            0x46
#define DA7212_LINE_GAIN             0x4A
#define DA7212_MIXOUT_R_SELECT       0x4C
#define DA7212_SYSTEM_MODES_OUTPUT   0x51
#define DA7212_DAC_R_CTRL            0x6A
#define DA7212_LINE_CTRL             0x6D
#define DA7212_MIXOUT_R_CTRL         0x6F
#define DA7212_TONE_GEN_CFG1         0xB4
#define DA7212_TONE_GEN_CYCLES       0xB6
#define DA7212_TONE_GEN_ON_PER       0xBB
#define DA7212_SYSTEM_ACTIVE         0xFD

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;
} AppData;

static void da7212_register_write(uint8_t reg, uint8_t value) {
  uint8_t data[2] = {reg, value};
  bool ret;

  i2c_use(I2C_DA7212);

  ret = i2c_write_block(I2C_DA7212, 2, data);
  PBL_ASSERTN(ret);

  i2c_release(I2C_DA7212);
}

static void prv_da7212_play_tone(void) {
  // CIF_CTRL: soft reset
  da7212_register_write(DA7212_CIF_CTRL, 0x80);
  
  psleep(10);

  // SYSTEM_ACTIVE: wake-up
  da7212_register_write(DA7212_SYSTEM_ACTIVE, 0x01);

  // Soft mute
  da7212_register_write(DA7212_DAC_FILTERS5, 0x80);

  // DAI: enable (16-bit)
  da7212_register_write(DA7212_DAI_CTRL, 0x80);
  // DAI: enable master mode, BCLK=64
  da7212_register_write(DA7212_DAI_CLK_MODE, 0x81);

  // DAC_R/L source: DAI_R/L
  da7212_register_write(DA7212_DIG_ROUTING_DAC, 0x32);

  // DAC_R: 0dB gain
  da7212_register_write(DA7212_DAC_R_GAIN, 0x6f);
  // DAC_R: enable
  da7212_register_write(DA7212_DAC_R_CTRL, 0x80);

  // MIXOUT_R: input from DAC_R
  da7212_register_write(DA7212_MIXOUT_R_SELECT, 0x08);
  // MIXOUT_R: enable + amplifier
  da7212_register_write(DA7212_MIXOUT_R_CTRL, 0x88);

  // Line: 0dB gain
  da7212_register_write(DA7212_LINE_GAIN, 0x30);
  // Line: enable amplifier, gain ramping, drive output
  da7212_register_write(DA7212_LINE_CTRL, 0xa8);

  // Tone generator: infinite cycles
  da7212_register_write(DA7212_TONE_GEN_CYCLES, 0x07);
  // Tone generator: 200ms pulse
  da7212_register_write(DA7212_TONE_GEN_ON_PER, 0x14);
  // Tone generator: start
  da7212_register_write(DA7212_TONE_GEN_CFG1, 0x80);

  // System modes output: enable DAC_R, Line
  da7212_register_write(DA7212_SYSTEM_MODES_OUTPUT, 0x89);

  // Soft mute off
  da7212_register_write(DA7212_DAC_FILTERS5, 0x00);
}

static void prv_da7212_idle(void) {
  da7212_register_write(DA7212_SYSTEM_ACTIVE, 0x00);
}

static void prv_timer_callback(void *cb_data) {
  (void)prv_da7212_idle();
  app_window_stack_pop(true /* animated */);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_overrides_back_button(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "SPEAKER TEST");
  layer_add_child(&window->layer, &title->layer);

  app_window_stack_push(window, true /* Animated */);

  prv_da7212_play_tone();

  app_timer_register(5000, prv_timer_callback, NULL);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd *mfg_speaker_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common.main_func = &s_main,
      // UUID: 27047635-68f1-4ece-9ca7-52dd8e22d1dd
      .common.uuid = {0x27, 0x04, 0x76, 0x35, 0x68, 0xf1, 0x4e, 0xce, 0x9c, 0xa7, 0x52, 0xdd, 0x8e,
                      0x22, 0xd1, 0xdd},
      .name = "MfgSpeaker",
  };
  return (const PebbleProcessMd *)&s_app_info;
}
