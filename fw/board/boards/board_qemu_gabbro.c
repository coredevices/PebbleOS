/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient/qemu.h>
#include <pbl/drivers/backlight/qemu.h>
#include <pbl/drivers/gpio/qemu.h>
#include <pbl/drivers/imu/qemu.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/temperature/qemu.h>
#include <pbl/drivers/vibe/qemu.h>
#include <pbl/kernel/irq.h>

#include <board/board.h>

// UART device for debug serial
#include <pbl/drivers/mic/qemu/mic_definitions.h>
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

// Display device - QEMU framebuffer
static QemuDisplayDevice s_display = {
  .base_addr = QEMU_DISPLAY_BASE,
  .fb_addr = QEMU_DISPLAY_FB_BASE,
  .width = 260,
  .height = 260,
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
};

PBL_GPIO_KEYS_DEFINE(BUTTONS, "buttons",
                     PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(QEMU_GPIO, 0, 0), PBL_INPUT_KEY_BACK, 0),
                     PBL_GPIO_KEYS_KEY("Up", PBL_GPIO(QEMU_GPIO, 1, 0), PBL_INPUT_KEY_UP, 0),
                     PBL_GPIO_KEYS_KEY("Select", PBL_GPIO(QEMU_GPIO, 2, 0), PBL_INPUT_KEY_SELECT,
                                       0),
                     PBL_GPIO_KEYS_KEY("Down", PBL_GPIO(QEMU_GPIO, 3, 0), PBL_INPUT_KEY_DOWN, 0));

// Microphone (QEMU stub — feeds silence on a timer)
static MicDeviceState s_mic_state;
static MicDevice MIC_DEVICE = {
  .state = &s_mic_state,
  .channels = 1,
};
MicDevice *const MIC = &MIC_DEVICE;

PBL_IRQ_CONNECT(UART2, 5, uart_irq_handler, DBG_UART, 0);
PBL_IRQ_CONNECT(UART1, 6, uart_irq_handler, QEMU_UART, 0);
#ifdef CONFIG_BT_HCI_UART
PBL_IRQ_CONNECT(UART3, 6, uart_irq_handler, BT_HCI_UART, 0);
#endif

void board_early_init(void) {
}

void board_init(void) {
}

const struct pbl_accel *const ACCEL = QEMU_ACCEL;

const struct pbl_als *const ALS = QEMU_ALS;

const struct pbl_backlight *const BACKLIGHT = QEMU_BACKLIGHT;

const struct pbl_vibe *const VIBE = QEMU_VIBE;

const struct pbl_temp *const DIE_TEMP = QEMU_TEMP;
