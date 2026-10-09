/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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

const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons =
      {
        [BUTTON_ID_BACK] = {"Back", NULL, 0, GPIO_PuPd_NOPULL, true, PBL_INPUT_KEY_BACK},
        [BUTTON_ID_UP] = {"Up", NULL, 1, GPIO_PuPd_UP, false, PBL_INPUT_KEY_UP},
        [BUTTON_ID_SELECT] = {"Select", NULL, 2, GPIO_PuPd_UP, false, PBL_INPUT_KEY_SELECT},
        [BUTTON_ID_DOWN] = {"Down", NULL, 3, GPIO_PuPd_UP, false, PBL_INPUT_KEY_DOWN},
      },
  .timer = NULL,
  .timer_irqn = TIMER0_IRQn,
};

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
