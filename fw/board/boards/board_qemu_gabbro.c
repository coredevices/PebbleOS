/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient/qemu.h>
#include <pbl/drivers/backlight/qemu.h>
#include <pbl/drivers/display/qemu.h>
#include <pbl/drivers/flash/qemu.h>
#include <pbl/drivers/gpio/qemu.h>
#include <pbl/drivers/imu/qemu.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/temperature/qemu.h>
#include <pbl/drivers/touch/qemu.h>
#include <pbl/drivers/vibe/qemu.h>
#include <pbl/kernel/irq.h>

#include <board/board.h>

// UART device for debug serial
#include <pbl/drivers/mic/qemu/mic_definitions.h>
#include <pbl/drivers/uart/qemu.h>

PBL_UART_QEMU_DEFINE(s_uart2, "uart2", QEMU_UART2_BASE, PBL_IRQN(UART2), 115200);
const struct pbl_uart *const DBG_UART = &s_uart2.uart;

// QEMU control protocol UART (UART1)
PBL_UART_QEMU_DEFINE(s_uart1, "uart1", QEMU_UART1_BASE, PBL_IRQN(UART1), 115200);
const struct pbl_uart *const QEMU_UART = &s_uart1.uart;

#ifdef CONFIG_BT_HCI_UART
PBL_UART_QEMU_DEFINE(s_uart3, "uart3", QEMU_UART3_BASE, PBL_IRQN(UART3), 115200);
const struct pbl_uart *const BT_HCI_UART = &s_uart3.uart;
#endif

// Display device - QEMU framebuffer
PBL_DISPLAY_QEMU_DEFINE(s_display, "display", QEMU_DISPLAY_BASE, QEMU_DISPLAY_FB_BASE, 260, 260,
                        PBL_DISPLAY_FORMAT_ARGB2222);

const struct pbl_display *const DISPLAY = &s_display.disp;

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

PBL_IRQ_CONNECT(UART2, 5, pbl_uart_qemu_irq_handler, &s_uart2, 0);
PBL_IRQ_CONNECT(UART1, 6, pbl_uart_qemu_irq_handler, &s_uart1, 0);
#ifdef CONFIG_BT_HCI_UART
PBL_IRQ_CONNECT(UART3, 6, pbl_uart_qemu_irq_handler, &s_uart3, 0);
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

const struct pbl_touch *const TOUCH = QEMU_TOUCH;
const struct pbl_flash *const FLASH = QEMU_FLASH;
