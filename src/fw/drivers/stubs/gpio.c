#include "drivers/gpio.h"

void gpio_use(GPIO_TypeDef* GPIOx) {

}

void gpio_release(GPIO_TypeDef* GPIOx) {
}

void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype) {
}

void gpio_output_set(const OutputConfig *pin_config, bool asserted) {
}

void gpio_input_init(const InputConfig *input_config) {
}

void gpio_input_init_pull_up_down(const InputConfig *input_config, GPIOPuPd_TypeDef pupd) {

}

bool gpio_input_read(const InputConfig *input_config) {

  return false;
}
