#include "drivers/gpio.h"
#include "system/passert.h"

#include <hal/nrf_gpio.h>

void gpio_input_init(const InputConfig *pin_config) {
  nrf_gpio_pin_dir_set(pin_config->gpio_pin, NRF_GPIO_PIN_DIR_INPUT);
}

void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed) {
  if (otype == GPIO_OType_OD)
    WTF;

  /* XXX: speed */
  nrf_gpio_cfg(pin_config->gpio_pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
}

void gpio_output_set(const OutputConfig *pin_config, bool asserted) {
  if (!pin_config->active_high) {
    asserted = !asserted;
  }
  nrf_gpio_pin_write(pin_config->gpio_pin, !!asserted);
}
