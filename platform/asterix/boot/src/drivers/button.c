#include "drivers/button.h"

#include <board.h>

#include <nrfx.h>

typedef struct {
	const char *name;
	NRF_GPIO_Type *port;
	uint32_t pin;
	uint8_t pull;
} ButtonConfig;

static const ButtonConfig BOARD_CONFIG_BUTTON[] = {
	[BUTTON_ID_BACK] = {"Back", BOARD_BUTTON_BACK_PORT, BOARD_BUTTON_BACK_PIN,
			    GPIO_PIN_CNF_PULL_Pullup},
	[BUTTON_ID_UP] = {"Up", BOARD_BUTTON_UP_PORT, BOARD_BUTTON_UP_PIN,
			  GPIO_PIN_CNF_PULL_Pullup},
	[BUTTON_ID_SELECT] = {"Select", BOARD_BUTTON_SELECT_PORT, BOARD_BUTTON_SELECT_PIN,
			      GPIO_PIN_CNF_PULL_Pullup},
	[BUTTON_ID_DOWN] = {"Down", BOARD_BUTTON_DOWN_PORT, BOARD_BUTTON_DOWN_PIN,
			    GPIO_PIN_CNF_PULL_Pullup},
};

static void initialize_button(const ButtonConfig *config)
{
	config->port->PIN_CNF[config->pin] =
		((GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
		 (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
		 (config->pull << GPIO_PIN_CNF_PULL_Pos) |
		 (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
		 (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos));
}

bool button_is_pressed(ButtonId id)
{
	const ButtonConfig *config = &BOARD_CONFIG_BUTTON[id];
	return (config->port->IN & (1 << config->pin)) == 0;
}

uint8_t button_get_state_bits(void)
{
	uint8_t button_state = 0x00;
	for (int i = 0; i < NUM_BUTTONS; ++i) {
		button_state |= (button_is_pressed(i) ? 0x01 : 0x00) << i;
	}
	return button_state;
}

void button_init(void)
{
	for (int i = 0; i < NUM_BUTTONS; ++i) {
		initialize_button(&BOARD_CONFIG_BUTTON[i]);
	}
}
