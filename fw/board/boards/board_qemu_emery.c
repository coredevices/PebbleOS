/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient/qemu.h>
#include <pbl/drivers/backlight/qemu.h>
#include <pbl/drivers/imu/qemu.h>
#include <pbl/drivers/ppg/qemu.h>
#include <pbl/drivers/temperature/qemu.h>
#include <pbl/drivers/vibe/qemu.h>
#include <pbl/kernel/irq.h>
#include <pbl/services/hrm/hrm_ppg.h>

#include <board/board.h>

// UART device for debug serial
#include <pbl/drivers/mic/qemu/mic_definitions.h>
#include <pbl/drivers/speaker/qemu/audio.h>
#include <pbl/drivers/uart/qemu.h>

static UARTDeviceState s_dbg_uart_state = {};

static struct UARTDevice DBG_UART_DEVICE = {
  .state = &s_dbg_uart_state,
  .base_addr = QEMU_UART2_BASE,
  .irqn = UART2_IRQn,
};

UARTDevice *const DBG_UART = (UARTDevice *)&DBG_UART_DEVICE;

// QEMU control protocol UART (UART1)
static UARTDeviceState s_qemu_uart_state = {};

static struct UARTDevice QEMU_UART_DEVICE = {
  .state = &s_qemu_uart_state,
  .base_addr = QEMU_UART1_BASE,
  .irqn = UART1_IRQn,
};

UARTDevice *const QEMU_UART = (UARTDevice *)&QEMU_UART_DEVICE;

#ifdef CONFIG_BT_HCI_UART
static UARTDeviceState s_bt_hci_uart_state = {};

static struct UARTDevice BT_HCI_UART_DEVICE = {
  .state = &s_bt_hci_uart_state,
  .base_addr = QEMU_UART3_BASE,
  .irqn = UART3_IRQn,
};

UARTDevice *const BT_HCI_UART = (UARTDevice *)&BT_HCI_UART_DEVICE;
#endif

// Display device - QEMU framebuffer at DISPLAY_BASE + 0x1000
static QemuDisplayDevice s_display = {
  .base_addr = QEMU_DISPLAY_BASE,
  .fb_addr = QEMU_DISPLAY_FB_BASE,
  .width = 200,
  .height = 228,
  .bpp = 8,
  .irqn = DISPLAY_IRQn,
};

DisplayDevice *const DISPLAY = &s_display;

const BoardConfigPower BOARD_CONFIG_POWER = {
  .low_power_threshold = 2U,
  .battery_capacity_hours = 168U,
};

const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 100,
  .ambient_light_dark_threshold = 150,
  .ambient_k_delta_threshold = 25,
  .backlight_default_color = BACKLIGHT_COLOR_WHITE,
};

const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK] = {"Back", PBL_INPUT_KEY_BACK},
    [BUTTON_ID_UP] = {"Up", PBL_INPUT_KEY_UP},
    [BUTTON_ID_SELECT] = {"Select", PBL_INPUT_KEY_SELECT},
    [BUTTON_ID_DOWN] = {"Down", PBL_INPUT_KEY_DOWN},
  },
};

// Audio device
static struct AudioState s_audio_state;
static struct AudioDevice AUDIO_DEVICE = {
  .state = &s_audio_state,
  .base_addr = QEMU_AUDIO_BASE,
  .irqn = AUDIO_IRQn,
};
AudioDevice *const AUDIO = (AudioDevice *)&AUDIO_DEVICE;

// Microphone (QEMU stub — feeds silence on a timer)
static MicDeviceState s_mic_state;
static MicDevice MIC_DEVICE = {
  .state = &s_mic_state,
  .channels = 1,
};
MicDevice *const MIC = &MIC_DEVICE;

const struct pbl_ppg *const PPG = QEMU_PPG;

const struct hrm_ppg_config HRM_PPG_CONFIG = {
  .roles = {
    [HRM_PPG_ROLE_GREEN] =
        {
          .slot = 0,
          .rx_mask = 0x1,
          .agc = true,
          .agc_target = HRM_PPG_AGC_GENERAL,
          .initial_ua = 20000,
          .initial_gain_ohm = 100000,
          .min_ua = 10000,
          .max_ua = 150000,
          .max_gain_ohm = 1000000,
        },
    [HRM_PPG_ROLE_RED] =
        {
          .slot = 1,
          .rx_mask = 0x1,
          .agc = true,
          .agc_led = 1,
          .agc_target = HRM_PPG_AGC_SPO2,
          .initial_ua = 20000,
          .initial_gain_ohm = 25000,
          .min_ua = 20000,
          .max_ua = 100000,
          .max_gain_ohm = 1000000,
        },
    [HRM_PPG_ROLE_IR] =
        {
          .slot = 2,
          .rx_mask = 0x1,
          .agc = true,
          .agc_target = HRM_PPG_AGC_SPO2,
          .initial_ua = 20000,
          .initial_gain_ohm = 25000,
          .min_ua = 20000,
          .max_ua = 100000,
          .max_gain_ohm = 1000000,
        },
    [HRM_PPG_ROLE_AMBIENT] = {.slot = 3, .rx_mask = 0x1},
  },
};

PBL_IRQ_CONNECT(UART2, 5, uart_irq_handler, DBG_UART, 0);
PBL_IRQ_CONNECT(UART1, 6, uart_irq_handler, QEMU_UART, 0);
#ifdef CONFIG_BT_HCI_UART
PBL_IRQ_CONNECT(UART3, 6, uart_irq_handler, BT_HCI_UART, 0);
#endif
PBL_IRQ_CONNECT(AUDIO, 5, qemu_audio_irq_handler, AUDIO, 0);

void board_early_init(void) {
}

void board_init(void) {
}

const struct pbl_accel *const ACCEL = QEMU_ACCEL;

const struct pbl_als *const ALS = QEMU_ALS;

const struct pbl_backlight *const BACKLIGHT = QEMU_BACKLIGHT;

const struct pbl_vibe *const VIBE = QEMU_VIBE;

const struct pbl_temp *const DIE_TEMP = QEMU_TEMP;
