/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bf0_hal.h"
#include "bf0_hal_efuse.h"
#include "bf0_hal_hlp.h"
#include "bf0_hal_lcpu_config.h"
#include "bf0_hal_pmu.h"
#include "bf0_hal_rcc.h"
#include "board/board.h"
#include "board/splash.h"
#include "drivers/sf32lb52/debounced_button_definitions.h"
#include "drivers/stubs/hrm.h"
#include "system/passert.h"
#include "kernel/util/stop.h"


#define HCPU_FREQ_MHZ 240

static UARTDeviceState s_dbg_uart_state = {
  .huart = {
    .Instance = USART1,
    .Init = {
      .BaudRate = 1000000,
      .WordLength = UART_WORDLENGTH_8B,
      .StopBits = UART_STOPBITS_1,
      .Parity = UART_PARITY_NONE,
      .HwFlowCtl = UART_HWCONTROL_NONE,
      .OverSampling = UART_OVERSAMPLING_16,
    },
  },
  .hdma = {
    .Instance = DMA1_Channel1,
    .Init = {
      .Request = DMA_REQUEST_5,
      .IrqPrio = 5,
    },
  },
};

static UARTDevice DBG_UART_DEVICE = {
    .state = &s_dbg_uart_state,
    .tx = {
        .pad = PAD_PA19,
        .func = USART1_TXD,
        .flags = PIN_NOPULL,
    },
    .rx = {
        .pad = PAD_PA18,
        .func = USART1_RXD,
        .flags = PIN_PULLUP,
    },
    .irqn = USART1_IRQn,
    .irq_priority = 5,
    .dma_irqn = DMAC1_CH1_IRQn,
    .dma_irq_priority = 5,
};

UARTDevice *const DBG_UART = &DBG_UART_DEVICE;

IRQ_MAP(USART1, uart_irq_handler, DBG_UART);
IRQ_MAP(DMAC1_CH1, uart_dma_irq_handler, DBG_UART);

static PwmState s_pwm1_ch1_state = {
    .handle = {
        .Instance = hwp_gptim1,
        .Init = {
             .CounterMode = GPT_COUNTERMODE_UP,
        },

    },
    .clock_config = {
        .ClockSource = GPT_CLOCKSOURCE_INTERNAL,
    },
    .channel = 1,
};

static PwmState s_pwm1_ch2_state = {
    .handle = {
        .Instance = hwp_gptim1,
        .Init = {
             .CounterMode = GPT_COUNTERMODE_UP,
        },

    },
    .clock_config = {
        .ClockSource = GPT_CLOCKSOURCE_INTERNAL,
    },
    .channel = 2,
};

static PwmState s_pwm1_ch3_state = {
    .handle = {
        .Instance = hwp_gptim1,
        .Init = {
             .CounterMode = GPT_COUNTERMODE_UP,
        },

    },
    .clock_config = {
        .ClockSource = GPT_CLOCKSOURCE_INTERNAL,
    },
    .channel = 3,
};

#if !BOARD_OBELIX_DVT && !BOARD_OBELIX_BB2
const LedControllerPwm LED_CONTROLLER_PWM = {
    .pwm = {
        [0] = {
            .pwm_pin = {
                .pad = PAD_PA28,
                .func = GPTIM1_CH1,
                .flags = PIN_NOPULL,
            },
            .state = &s_pwm1_ch1_state,
        },
        [1] = {
            .pwm_pin = {
                .pad = PAD_PA29,
                .func = GPTIM1_CH2,
                .flags = PIN_NOPULL,
            },
            .state = &s_pwm1_ch2_state,
        },
        [2] = {
            .pwm_pin = {
                .pad = PAD_PA44,
                .func = GPTIM1_CH3,
                .flags = PIN_NOPULL,
            },
            .state = &s_pwm1_ch3_state,
        },
    },
    .initial_color = LED_WARM_WHITE,
};
#endif

static DisplayJDIState s_display_state = {
    .hlcdc = {
        .Instance = LCDC1,
    },
};

static DisplayJDIDevice s_display = {
    .state = &s_display_state,
    .irqn = LCDC1_IRQn,
    .irq_priority = 5,
    .vcom = {
        .lptim = hwp_lptim2,
        .freq_hz = 60U,
    },
    .pinmux = {
        .xrst = {
            .pad = PAD_PA40,
            .func = LCDC1_JDI_XRST,
            .flags = PIN_NOPULL,
            },
        .vst = {
            .pad = PAD_PA08,
            .func = LCDC1_JDI_VST,
            .flags = PIN_NOPULL,
            },
        .vck = {
            .pad = PAD_PA39,
            .func = LCDC1_JDI_VCK,
            .flags = PIN_NOPULL,
            },
        .enb = {
            .pad = PAD_PA07,
            .func = LCDC1_JDI_ENB,
            .flags = PIN_NOPULL,
            },
        .hst = {
            .pad = PAD_PA06,
            .func = LCDC1_JDI_HST,
            .flags = PIN_NOPULL,
            },
        .hck = {
            .pad = PAD_PA41,
            .func = LCDC1_JDI_HCK,
            .flags = PIN_NOPULL,
            },
        .r1 = {
            .pad = PAD_PA05,
            .func = LCDC1_JDI_R1,
            .flags = PIN_NOPULL,
            },
        .r2 = {
            .pad = PAD_PA42,
            .func = LCDC1_JDI_R2,
            .flags = PIN_NOPULL,
            },
        .g1 = {
            .pad = PAD_PA04,
            .func = LCDC1_JDI_G1,
            .flags = PIN_NOPULL,
            },
        .g2 = {
            .pad = PAD_PA43,
            .func = LCDC1_JDI_G2,
            .flags = PIN_NOPULL,
            },
        .b1 = {
            .pad = PAD_PA03,
            .func = LCDC1_JDI_B1,
            .flags = PIN_NOPULL,
            },
        .b2 = {
            .pad = PAD_PA02,
            .func = LCDC1_JDI_B2,
            .flags = PIN_NOPULL,
            },
        .vcom = {
            .pad = PAD_PA24,
            .func = GPIO_A24,
            .flags = PIN_NOPULL,
        },
        .va = {
            .pad = PAD_PA25,
            .func = GPIO_A25,
            .flags = PIN_NOPULL,
        },
    },
#if BOARD_OBELIX_DVT || BOARD_OBELIX_BB2
    .vddp = {hwp_gpio1, 28, true},
    .vlcd = {hwp_gpio1, 29, true},
#endif
    .splash = {
        .data = splash_bits,
        .width = splash_width,
        .height = splash_height,
    },
};

DisplayJDIDevice *const DISPLAY = &s_display;
IRQ_MAP(LCDC1, display_jdi_irq_handler, DISPLAY);

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
static UARTDeviceState s_hci_trace_uart_state = {
  .huart = {
    .Instance = USART3,
    .Init = {
      .WordLength = UART_WORDLENGTH_8B,
      .StopBits = UART_STOPBITS_1,
      .Parity = UART_PARITY_NONE,
      .HwFlowCtl = UART_HWCONTROL_NONE,
      .OverSampling = UART_OVERSAMPLING_16,
    },
  },
};

static UARTDevice HCI_TRACE_UART_DEVICE = {
    .state = &s_hci_trace_uart_state,
    .tx = {
        .pad = PAD_PA20,
        .func = USART3_TXD,
        .flags = PIN_NOPULL,
    },
};
UARTDevice *const HCI_TRACE_UART = &HCI_TRACE_UART_DEVICE;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

static QSPIPortState s_qspi_port_state;
static QSPIPort QSPI_PORT = {
    .state = &s_qspi_port_state,
    .cfg = {
      .Instance = FLASH2,
      .line = HAL_FLASH_QMODE,
      .base = FLASH2_BASE_ADDR,
      .msize = 16,
      .SpiMode = SPI_MODE_NOR,
    },
    .clk_div = 5U,
    .dma = {
      .Instance = DMA1_Channel2,
      .dma_irq = DMAC1_CH2_IRQn,
      .request = DMA_REQUEST_1,
    },
};
QSPIPort *const QSPI = &QSPI_PORT;

static QSPIFlashState s_qspi_flash_state;
static QSPIFlash QSPI_FLASH_DEVICE = {
    .state = &s_qspi_flash_state,
    .qspi = &QSPI_PORT,
};
QSPIFlash *const QSPI_FLASH = &QSPI_FLASH_DEVICE;

static I2CDeviceState s_i2c_device_state_1 = {
    .int_enabled = true,
};

static struct I2CBusHal s_i2c_bus_hal_1 = {
    .i2c_state = &s_i2c_device_state_1,
    .hi2c =
        {
            .Instance = I2C1,
            .Init = {
                .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
                .ClockSpeed = 400000,
                .GeneralCallMode = I2C_GENERALCALL_DISABLE,
            },
            .Mode = HAL_I2C_MODE_MASTER,
            .core = CORE_ID_HCPU,
        },
    .scl =
        {
            .pad = PAD_PA31,
            .func = I2C1_SCL,
            .flags = PIN_NOPULL,
        },
    .sda =
        {
            .pad = PAD_PA30,
            .func = I2C1_SDA,
            .flags = PIN_NOPULL,
        },
    .core = CORE_ID_HCPU,
    .module = RCC_MOD_I2C1,
    .irqn = I2C1_IRQn,
    .irq_priority = 5,
    .timeout = 5000,
};

static I2CBusState s_i2c_bus_state_1;

static I2CBus s_i2c_bus_1 = {
    .hal = &s_i2c_bus_hal_1,
    .state = &s_i2c_bus_state_1,
    .name = "i2c1",
    .stop_mode_inhibitor = InhibitorI2C1,
};

I2CBus *const I2C1_BUS = &s_i2c_bus_1;

IRQ_MAP(I2C1, i2c_irq_handler, I2C1_BUS);

static I2CDeviceState s_i2c_device_state_2 = {
    .int_enabled = true,
};

static struct I2CBusHal s_i2c_bus_hal_2 = {
    .i2c_state = &s_i2c_device_state_2,
    .hi2c =
        {
            .Instance = I2C2,
            .Init = {
                .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
                .ClockSpeed = 400000,
                .GeneralCallMode = I2C_GENERALCALL_DISABLE,
            },
            .Mode = HAL_I2C_MODE_MASTER,
            .core = CORE_ID_HCPU,
        },
    .scl =
        {
            .pad = PAD_PA32,
            .func = I2C2_SCL,
            .flags = PIN_NOPULL,
        },
    .sda =
        {
            .pad = PAD_PA33,
            .func = I2C2_SDA,
            .flags = PIN_NOPULL,
        },
    .core = CORE_ID_HCPU,
    .module = RCC_MOD_I2C2,
    .irqn = I2C2_IRQn,
    .irq_priority = 5,
    .timeout = 5000,
};

static I2CBusState s_i2c_bus_state_2;

static I2CBus s_i2c_bus_2 = {
    .hal = &s_i2c_bus_hal_2,
    .state = &s_i2c_bus_state_2,
    .name = "i2c2",
    .stop_mode_inhibitor = InhibitorI2C2,
};

I2CBus *const I2C2_BUS = &s_i2c_bus_2;

IRQ_MAP(I2C2, i2c_irq_handler, I2C2_BUS);

static const I2CSlavePort s_i2c_lsm6d = {
    .bus = &s_i2c_bus_2,
    .address = 0x6A,
};

I2CSlavePort *const I2C_LSM6D = &s_i2c_lsm6d;

static const I2CSlavePort s_i2c_mmc5603nj = {
    .bus = &s_i2c_bus_2,
    .address = 0x30,
};

I2CSlavePort *const I2C_MMC5603NJ = &s_i2c_mmc5603nj;

static const I2CSlavePort s_i2c_npm1300 = {
    .bus = &s_i2c_bus_1,
    .address = 0x6B,
};

I2CSlavePort *const I2C_NPM1300 = &s_i2c_npm1300;

static const I2CSlavePort s_i2c_aw86225 = {
    .bus = &s_i2c_bus_1,
    .address = 0x58,
};
  
I2CSlavePort *const I2C_AW86225 = &s_i2c_aw86225;

static const I2CSlavePort s_i2c_aw2016 = {
    .bus = &s_i2c_bus_1,
    .address = 0x64,
};

I2CSlavePort *const I2C_AW2016 = &s_i2c_aw2016;

static HRMDeviceState s_hrm_state;
static HRMDevice s_hrm = {
  .state = &s_hrm_state,
};
HRMDevice * const HRM = &s_hrm;

const BoardConfigActuator BOARD_CONFIG_VIBE = {
    .ctl = {hwp_gpio1, 1, true},
};

static I2CDeviceState s_i2c_device_state_3 = {
    .int_enabled = true,
};

static struct I2CBusHal s_i2c_bus_hal_3 = {
    .i2c_state = &s_i2c_device_state_3,
    .hi2c =
        {
            .Instance = I2C3,
            .Init = {
                .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
                .ClockSpeed = 400000,
                .GeneralCallMode = I2C_GENERALCALL_DISABLE,
            },
            .Mode = HAL_I2C_MODE_MASTER,
            .core = CORE_ID_HCPU,
        },
    .scl =
        {
            .pad = PAD_PA11,
            .func = I2C3_SCL,
            .flags = PIN_NOPULL,
        },
    .sda =
        {
            .pad = PAD_PA10,
            .func = I2C3_SDA,
            .flags = PIN_NOPULL,
        },
    .core = CORE_ID_HCPU,
    .module = RCC_MOD_I2C3,
    .irqn = I2C3_IRQn,
    .irq_priority = 5,
    .timeout = 5000,
};

static I2CBusState s_i2c_bus_state_3;

static I2CBus s_i2c_bus_3 = {
    .hal = &s_i2c_bus_hal_3,
    .state = &s_i2c_bus_state_3,
    .name = "i2c3",
    .stop_mode_inhibitor = InhibitorI2C3,
};

I2CBus *const I2C3_BUS = &s_i2c_bus_3;
IRQ_MAP(I2C3, i2c_irq_handler, I2C3_BUS);

static const I2CSlavePort s_i2c_cst816 = {
    .bus = I2C3_BUS,
    .address = 0x15,
};

static const I2CSlavePort s_i2c_cst816_boot = {
    .bus = I2C3_BUS,
    .address = 0x6A,
};

static const TouchSensor touch_cst816 = {
    .i2c = &s_i2c_cst816,
    .i2c_boot = &s_i2c_cst816_boot,
    .int_exti = {
        .peripheral = hwp_gpio1,
        .gpio_pin = 27,
    },
};
const TouchSensor *CST816 = &touch_cst816;

// TODO(OBELIX): Adjust to final battery parameters
const Npm1300Config NPM1300_CONFIG = {
  // 190mA = 1C (rapid charge, max limit from datasheet)
  .chg_current_ma = 190,
  .dischg_limit_ma = 1000,
  .term_current_pct = 10,
  .thermistor_beta = 3380,
  .vbus_current_lim0 = 500,
  .vbus_current_startup = 500,
};

static const I2CSlavePort s_i2c_w1160 = {
    .bus = &s_i2c_bus_1,
    .address = 0x48,
  };
  
I2CSlavePort *const I2C_W1160 = &s_i2c_w1160;

const BoardConfigPower BOARD_CONFIG_POWER = {
  .pmic_int = {
    .peripheral = hwp_gpio1,
    .gpio_pin = 26,
  },
  .low_power_threshold = 5U,
  .battery_capacity_hours = 100U,
};

const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 25,
  .ambient_light_dark_threshold = 150,
  .ambient_k_delta_threshold = 25,
  .dynamic_backlight_min_threshold = 20,
  .dynamic_backlight_max_threshold = 120,
};

const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK]   = { "Back",   hwp_gpio1, 34, GPIO_PuPd_NOPULL, true },
#if defined(IS_BIGBOARD) && !BOARD_OBELIX_BB2
    [BUTTON_ID_UP]     = { "Up",     hwp_gpio1, 37, GPIO_PuPd_UP, false},
#else
    [BUTTON_ID_UP]     = { "Up",     hwp_gpio1, 35, GPIO_PuPd_UP, false},
#endif
    [BUTTON_ID_SELECT] = { "Select", hwp_gpio1, 36, GPIO_PuPd_UP, false},
#if defined(IS_BIGBOARD) && !BOARD_OBELIX_BB2
    [BUTTON_ID_DOWN]   = { "Down",   hwp_gpio1, 35, GPIO_PuPd_UP, false},
#else
    [BUTTON_ID_DOWN]   = { "Down",   hwp_gpio1, 37, GPIO_PuPd_UP, false},
#endif
  },
  .timer = GPTIM2,
  .timer_irqn = GPTIM2_IRQn,
};
IRQ_MAP(GPTIM2, debounced_button_irq_handler, GPTIM2);

static MicDeviceState mic_state = {
  .hdma = {
  .Instance = DMA1_Channel5,
    .Init = {
       .Request = DMA_REQUEST_36,
       .IrqPrio = 5,
    },
  },
};
static const MicDevice mic_device = {
    .state = &mic_state,
    .pdm_instance = hwp_pdm1,
    .clk_gpio = {
        .pad = PAD_PA22,
        .func = PDM1_CLK,
        .flags = PIN_NOPULL, 
    },
    .data_gpio = {
        .pad = PAD_PA23,
        .func = PDM1_DATA,
        .flags = PIN_PULLDOWN,
    },
    .pdm_dma_irq = DMAC1_CH5_IRQn,
    .pdm_irq = PDM1_IRQn,
    .pdm_irq_priority = 5, 
    .channels = 2,
    .sample_rate = 16000,
    .channel_depth = 16,
};
const MicDevice* MIC = &mic_device;
IRQ_MAP(PDM1, pdm1_data_handler, MIC);
IRQ_MAP(DMAC1_CH5, pdm1_l_dma_handler, MIC);

static AudioDeviceState audio_state;
static const AudioDevice audio_device = {
    .state = &audio_state,
    .irq_priority = 5,
    .channels = 1,
    .samplerate = 16000,
    .data_format = 16,
    .audec_dma_irq = DMAC1_CH4_IRQn,
    .audec_dma_channel = DMA1_Channel4,
    .audec_dma_request = DMA_REQUEST_41,

    .pa_ctrl = {
        .gpio = hwp_gpio1,
        .gpio_pin = 0,
        .active_high =true,
    },
};
const AudioDevice* AUDIO = &audio_device;
IRQ_MAP(DMAC1_CH4, audec_dac0_dma_irq_handler, AUDIO);

uint32_t BSP_GetOtpBase(void) {
  return MPI2_MEM_BASE;
}

void board_early_init(void) {
  HAL_StatusTypeDef ret;
  uint32_t bootopt;

  // Adjust bootrom pull-up/down delays on PA21 (flash power control pin) so
  // that the flash is properly power cycled on reset. A flash power cycle is
  // needed if left in 4-byte addressing mode, as bootrom does not support it.
  bootopt = HAL_Get_backup(RTC_BACKUP_BOOTOPT);
  bootopt &= ~(RTC_BACKUP_BOOTOPT_PD_DELAY_Msk | RTC_BACKUP_BOOTOPT_PU_DELAY_Msk);
  bootopt |= RTC_BACKUP_BOOTOPT_PD_DELAY_MS(100) | RTC_BACKUP_BOOTOPT_PU_DELAY_MS(10);
  HAL_Set_backup(RTC_BACKUP_BOOTOPT, bootopt);

  if (HAL_RCC_HCPU_GetClockSrc(RCC_CLK_MOD_SYS) == RCC_SYSCLK_HRC48) {
    HAL_HPAON_EnableXT48();
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_HXT48);
  }

  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_PERI, RCC_CLK_PERI_HXT48);

  // Halt LCPU first to avoid LCPU in running state
  HAL_HPAON_WakeCore(CORE_ID_LCPU);
  HAL_RCC_Reset_and_Halt_LCPU(1);

  // Load system configuration from EFUSE
  BSP_System_Config();

  HAL_HPAON_StartGTimer();

  HAL_PMU_EnableRC32K(1);

  HAL_PMU_LpCLockSelect(PMU_LPCLK_RC32);

  HAL_PMU_EnableDLL(1);
#ifdef SF32LB52_USE_LXT
  HAL_PMU_EnableXTAL32();
  ret = HAL_PMU_LXTReady();
  PBL_ASSERTN(ret == HAL_OK);

  HAL_RTC_ENABLE_LXT();
#endif

  HAL_RCC_LCPU_ClockSelect(RCC_CLK_MOD_LP_PERI, RCC_CLK_PERI_HXT48);

  HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();

  HAL_RCC_HCPU_ConfigHCLK(HCPU_FREQ_MHZ);

  // Reset sysclk used by HAL_Delay_us
  HAL_Delay_us(0);

  ret = HAL_RCC_CalibrateRC48();
  PBL_ASSERTN(ret == HAL_OK);

  HAL_RCC_Init();
  HAL_PMU_Init();

  __HAL_SYSCFG_CLEAR_SECURITY();
  HAL_EFUSE_Init();

  // Reset HR pin, clear IE, PE
  hwp_pinmux1->PAD_PA09 &= ~((1 << 6) | (1 << 4));
  hwp_pinmux1->PAD_PA20 &= ~((1 << 6) | (1 << 4));
  hwp_pinmux1->PAD_PA25 &= ~((1 << 6) | (1 << 4));
}

void board_init(void) {
  i2c_init(I2C1_BUS);
  i2c_init(I2C2_BUS);
  i2c_init(I2C3_BUS);

  mic_init(MIC);

  /* Enable user buttons as AON wakeup source */
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN10, AON_PIN_MODE_HIGH);
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN11, AON_PIN_MODE_LOW);
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN12, AON_PIN_MODE_LOW);
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN13, AON_PIN_MODE_LOW);

  // Temporarily disable stop mode (GH-452)
  stop_mode_disable(InhibitorMain);
}
