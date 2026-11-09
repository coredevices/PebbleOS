/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/ambient/w1160.h>
#include <pbl/drivers/audio/amp_gpio.h>
#include <pbl/drivers/audio/sf32lb52.h>
#include <pbl/drivers/backlight/aw2016.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/mbox/sf32lb52.h>
#include <pbl/drivers/pinctrl/sf32lb52.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/drivers/rtc/sf32lb.h>
#include <pbl/drivers/sf32lb52/hxt.h>
#include <pbl/drivers/temperature/sf32lb.h>
#include <pbl/drivers/touch/cst816.h>
#include <pbl/drivers/uart/sf32lb.h>
#include <pbl/drivers/vibe/aw86225.h>
#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>

#ifdef CONFIG_HRM
#include <pbl/drivers/hrm/gh3x2x.h>
#endif

#include <devicetree/devices.h>
#include <hardware/sf32lb52_memorymap.h>

const struct pbl_uart *const CONSOLE_UART = &dt_usart1;

const uint8_t BOARD_DISPLAY_CORNERS[] = {3, 3, 2, 1};
const uint8_t BOARD_DISPLAY_NUM_CORNERS = ARRAY_LENGTH(BOARD_DISPLAY_CORNERS);

const struct pbl_display *const DISPLAY = &dt_lcdc1;

PBL_MBOX_SF32LB52_DEFINE(s_mailbox1, "mailbox1", SF32LB52_MAILBOX1_BASE, PBL_MBOX_SF32LB52_NO_IRQ,
                         PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_MAILBOX1), nullptr);
PBL_MBOX_SF32LB52_DEFINE(s_mailbox2, "mailbox2", SF32LB52_MAILBOX2_BASE, PBL_IRQN(LCPU2HCPU),
                         PBL_CLK(nullptr, 0), nullptr);
PBL_IRQ_CONNECT(LCPU2HCPU, 5, pbl_mbox_sf32lb52_irq_handler, &s_mailbox2, 0);

#ifdef CONFIG_REMOTEPROC_SF32LB52
PBL_RPROC_SF32LB52_DEFINE(s_lcpu, "lcpu", PBL_MBOX_CHAN(&s_mailbox1.ctrl, 0),
                          PBL_MBOX_CHAN(&s_mailbox2.ctrl, 0),
                          PBL_MEM_REGION(SF32LB52_HPSYS_MBOX_BUF_BASE, SF32LB52_MBOX_BUF_SIZE));
const struct pbl_rproc_sf32lb52 *const LCPU = &s_lcpu;
#endif // CONFIG_REMOTEPROC_SF32LB52

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
PBL_PINCTRL_STATE_DEFINE(s_hci_trace_uart_default, SF32LB52_PINCTRL,
                         SF32LB52_PA20_USART3_TXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);

PBL_UART_SF32LB_DEFINE(s_usart3, "usart3", SF32LB52_USART3_BASE, PBL_SOC_IRQN_USART3,
                       PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_USART3), 1000000,
                       &s_hci_trace_uart_default);
const struct pbl_uart *const HCI_TRACE_UART = &s_usart3;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

PBL_SF32LB52_HXT_DEFINE(s_hxt, "hxt", &dt_flash0, "soc-config-user", "soc-config-cust");

const struct pbl_flash *const FLASH = &dt_flash0;

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;

PBL_AW86225_DEFINE(s_aw86225, &dt_i2c1, 0x58, PBL_GPIO(SF32LB_GPIO1, 1, PBL_GPIO_ACTIVE_LOW));

const struct pbl_vibe *const VIBE = &s_aw86225.vibe;

PBL_AW2016_DEFINE(s_aw2016, &dt_i2c1, 0x64);

const struct pbl_backlight *const BACKLIGHT = &s_aw2016.backlight;

#ifdef CONFIG_ACCEL_LSM6DSO
const struct pbl_accel *const ACCEL = &dt_lsm6dso;
#endif

#ifdef CONFIG_MAG_MMC5603NJ
const struct pbl_mag *const MAG = &dt_mmc5603nj;
#endif

PBL_CST816_DEFINE(s_cst816, &dt_i2c3, 0x15, 0x6A, PBL_GPIO(SF32LB_GPIO1, 27, PBL_GPIO_PULL_UP),
                  PBL_GPIO(&dt_pmic_gpio, 2, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_PULL_UP), .size_x = 200,
                  .size_y = 228);

const struct pbl_touch *const TOUCH = &s_cst816.touch;

#ifdef CONFIG_HRM
static const struct pbl_i2c_dev s_i2c_gh3x2x = PBL_I2C_DEV(&dt_i2c4, 0x14);

static HRMDeviceState s_hrm_state;
static HRMDevice s_hrm = {
  .state = &s_hrm_state,
  .i2c = &s_i2c_gh3x2x,
  .int_irq = PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, 44, PBL_IRQ_TYPE_EDGE_FALLING),
  .int_input = PBL_GPIO(SF32LB_GPIO1, 44, 0),
  .reset = PBL_GPIO(&dt_pmic_gpio, 3, PBL_GPIO_PULL_UP),
};

HRMDevice *const HRM = &s_hrm;
#endif

// Bench-calibrated on 3 production units (unit spread 1.3%); dark floor
// is <20 counts so no offset is needed.
static const struct pbl_als_scale s_w1160_scale = {.offset = 0, .num = 100, .den = 483};

PBL_W1160_DEFINE(s_w1160, &dt_i2c1, 0x48, &s_w1160_scale);

const struct pbl_als *const ALS = &s_w1160.als;

PBL_GPIO_KEYS_DEFINE(
    BUTTONS, "buttons",
    PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(SF32LB_GPIO1, 34, 0), PBL_INPUT_KEY_BACK, 40),
    PBL_GPIO_KEYS_KEY("Up", PBL_GPIO(SF32LB_GPIO1, 35, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_UP, 40),
    PBL_GPIO_KEYS_KEY("Select", PBL_GPIO(SF32LB_GPIO1, 36, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_SELECT, 40),
    PBL_GPIO_KEYS_KEY("Down", PBL_GPIO(SF32LB_GPIO1, 37, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_DOWN, 40));

static const struct pbl_dma_chan s_mic_dma =
    PBL_DMA_CHAN(&dt_dmac1, 4, SF32LB52_DMA_REQ_PDM1_L, SF32LB52_DMA_PL_LOW);
PBL_PINCTRL_STATE_DEFINE(s_mic_default, SF32LB52_PINCTRL,
                         SF32LB52_PA22_PDM1_CLK | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA23_PDM1_DATA | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA | PBL_PINCTRL_SF32LB52_PULL_DOWN);
PBL_AUDIO_SF32LB52_PDM_DEFINE(s_mic, "mic", PBL_PINCTRL(&s_mic_default, nullptr, 0), PBL_IRQN(PDM1),
                              &s_mic_dma, &dt_ldsw2);
PBL_IRQ_CONNECT(PDM1, 5, pbl_audio_sf32lb52_pdm_irq_handler, &s_mic, 0);

const struct pbl_audio *const MIC = &s_mic.audio;

// Class-D amplifier, one enable pulse selects its mode
PBL_AUDIO_AMP_GPIO_DEFINE(s_speaker_amp, "speaker_amp", PBL_GPIO(SF32LB_GPIO1, 0, 0), nullptr,
                          .mode_pulses = 1, .pulse_us = 200, .settle_us = 2000);

static const struct pbl_audio_link s_speaker_link = {
  .amp = &s_speaker_amp.amp,
};

static const struct pbl_dma_chan s_speaker_dma =
    PBL_DMA_CHAN(&dt_dmac1, 3, SF32LB52_DMA_REQ_AUDAC_CH0, SF32LB52_DMA_PL_HIGH);
PBL_AUDIO_SF32LB52_AUDCODEC_DEFINE(s_speaker, "speaker", &s_speaker_dma, &s_speaker_link);

const struct pbl_audio *const SPEAKER = &s_speaker.audio;

const struct pbl_temp *const DIE_TEMP = SF32LB_TEMP;

const struct pbl_rtc *const RTC = SF32LB_RTC;
