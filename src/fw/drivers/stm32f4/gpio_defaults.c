/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"
#include "board/board.h"

void gpio_init_all(void) {
  GPIO_InitTypeDef gpio_init = (GPIO_InitTypeDef) {
    .GPIO_Mode = GPIO_Mode_AN,
    .GPIO_Speed = GPIO_Speed_2MHz,
    .GPIO_PuPd  = GPIO_PuPd_NOPULL
  };

  //  We program all the pins to be analog inputs to save power.
  //  We expect the following configuration after initialization code has run:
  //
  // GPIOA - don't touch PA0 (WKUP), PA13 (JTMS), PA14 (JTCK), PA15 (JTDI),
  //         PA1 & PA2 will be configured as analog pins.
  //         Expected non-analog mask: 0xff.f9
  //
  // GPIOB - don't touch PB3 (JTDO), PB4 (NJTRST). PB0, PB1, PB2, PB5, PB11,
  //         PB13 unused. Expected non-analog mask: 0xd7.d8
  //
  // GPIOC - PC0-PC9 are unused, PC14 (OSC32_IN) ok to set (see 8.3.13
  //         of ref manual) Expected non-analog mask: 0x1c.00
  //
  // GPIOD - PD0-PD15 are all for the parallel flash.
  //         Expected non-analog mask: 0xff.ff
  //
  // GPIOE - PE0-PE1 are accessory connector, PE2-PE15 are for flash.
  //         Expected non-analog mask: 0xff.ff
  //
  // GPIOF - PF6-PF9 (Audio SAI, not used?), PF5, PF10-PF12, PF15 unused.
  //         Expected non-analog mask: 0x60.1f
  //
  // GPIOG - PG0 unused, PG11 (PROG_SO) unused? Expected non-analog mask: 0xf7.fe
  //
  // GPIOH - Only PH0-PH1 on actual watch, PH2-PH15 unused on BB.
  //         Expected non-analog mask: 0x00.00
  //
  // GPIOI - Only on BB, nothing used. Expected non-analog mask: 0x00.00

  int tot_gpios = BOARD_CONFIG.num_avail_gpios;
  const int num_gpios_per_port = 16;
  for (uint32_t gpio_addr = (uint32_t)GPIOA;
      (gpio_addr <= (uint32_t)GPIOK) && (tot_gpios > 0); gpio_addr += 0x400) {
    if (gpio_addr == (uint32_t)GPIOA) {
      gpio_init.GPIO_Pin =
          GPIO_Pin_All & ~(GPIO_Pin_0 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
    } else if (gpio_addr == (uint32_t)GPIOB) {
      gpio_init.GPIO_Pin = GPIO_Pin_All & ~(GPIO_Pin_3 | GPIO_Pin_4);
    } else {
      gpio_init.GPIO_Pin = GPIO_Pin_All;
    }

    gpio_use((GPIO_TypeDef *)gpio_addr);
    GPIO_Init((GPIO_TypeDef *)gpio_addr, &gpio_init);
    gpio_release((GPIO_TypeDef *)gpio_addr);

    tot_gpios -= num_gpios_per_port;
  }
}
