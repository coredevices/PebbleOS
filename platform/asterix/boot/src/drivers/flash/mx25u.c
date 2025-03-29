#include <drivers/flash.h>
#include <drivers/dbgserial.h>

#include <board.h>

#include <nrfx_qspi.h>

static nrfx_qspi_config_t config =
	NRFX_QSPI_DEFAULT_CONFIG(BOARD_QSPI_SCK_PIN, BOARD_QSPI_SCN_PIN, BOARD_QSPI_IO0_PIN,
				 BOARD_QSPI_IO1_PIN, BOARD_QSPI_IO2_PIN, BOARD_QSPI_IO3_PIN);

#define SPI_NOR_CMD_RDID 0x9F
#define SPI_NOR_CMD_RDPD 0xAB

void flash_init(void)
{
	nrf_qspi_pins_t pins;
	nrf_qspi_pins_t disconnected_pins = {
		.sck_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.csn_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io0_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io1_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io2_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io3_pin = NRF_QSPI_PIN_NOT_CONNECTED,
	};

	config.prot_if.readoc = NRF_QSPI_READOC_READ4IO;
	config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP4IO;
	config.phy_if.sck_freq = NRF_QSPI_FREQ_DIV4;
	config.phy_if.sck_delay = 0x00;
	// config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	nrfx_qspi_init(&config, NULL, NULL);

	nrf_qspi_pins_set(NRF_QSPI, &disconnected_pins);
	nrfx_qspi_activate(true);
	nrf_qspi_pins_set(NRF_QSPI, &config.pins);

	/* exit DPD */
	nrf_qspi_cinstr_conf_t instr_config = {
		.opcode = SPI_NOR_CMD_RDPD,
		.length = NRF_QSPI_CINSTR_LEN_1B,
		.io2_level = true,
		.io3_level = true,
		.wipwait = false,
		.wren = false,
	};

	nrfx_qspi_cinstr_xfer(&instr_config, NULL, NULL);

	NRFX_DELAY_US(35);
}

bool flash_sanity_check(void)
{
	uint8_t buf[3];
	uint32_t id;
	nrf_qspi_cinstr_conf_t instr_config = {
		.opcode = SPI_NOR_CMD_RDID,
		.length = NRF_QSPI_CINSTR_LEN_4B,
		.io2_level = true,
		.io3_level = true,
		.wipwait = false,
		.wren = false,
	};

	nrfx_qspi_cinstr_xfer(&instr_config, NULL, buf);

	id = buf[0] << 16 | buf[1] << 8 | buf[2];
	dbgserial_print("JEDEC ID: ");
	dbgserial_print_hex(id);
	dbgserial_newline();

	return (id == BOARD_FLASH_JEDEC_ID);
}

void flash_read_bytes(uint8_t *buffer_ptr, uint32_t start_addr, uint32_t buffer_size)
{
	nrfx_qspi_read(buffer_ptr, buffer_size, start_addr);
}
