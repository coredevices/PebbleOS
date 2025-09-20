#include <board.h>
#include <drivers/dbgserial.h>
#include <drivers/flash.h>
#include <nrfx_qspi.h>
#include <system/passert.h>

#define SPI_NOR_CMD_RDID 0x9F
#define SPI_NOR_CMD_RDPD 0xAB
#define SPI_NOR_CMD_ENRST 0x66
#define SPI_NOR_CMD_RST 0x99
#define SPI_NOR_CMD_EN4B 0xB7
#define SPI_NOR_CMD_RDSR1 0x05
#define SPI_NOR_CMD_RDSR2 0x35
#define SPI_NOR_CMD_WRSR 0x01

static void prv_read_register(uint8_t instruction, uint8_t *data, uint32_t length) {
  nrfx_err_t err;
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(instruction, length + 1);
  instr.io2_level = true;
  instr.io3_level = true;
  err = nrfx_qspi_cinstr_xfer(&instr, NULL, data);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

static void prv_write_register(uint8_t instruction, const uint8_t *data, uint32_t length) {
  nrfx_err_t err;
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(instruction, length + 1);
  instr.io2_level = true;
  instr.io3_level = true;
  err = nrfx_qspi_cinstr_xfer(&instr, data, NULL);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

void flash_init(void) {
  uint8_t sr[2];
  nrfx_qspi_config_t config =
      NRFX_QSPI_DEFAULT_CONFIG(BOARD_QSPI_SCK_PIN, BOARD_QSPI_SCN_PIN, BOARD_QSPI_IO0_PIN,
                               BOARD_QSPI_IO1_PIN, BOARD_QSPI_IO2_PIN, BOARD_QSPI_IO3_PIN);

  config.prot_if.readoc = NRF_QSPI_READOC_READ4IO;
  config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP4O;
  config.phy_if.sck_freq = NRF_QSPI_FREQ_DIV4;
  config.prot_if.addrmode = NRF_QSPI_ADDRMODE_32BIT;

  nrfx_qspi_init(&config, NULL, NULL);

  // reset the flash
  prv_write_register(SPI_NOR_CMD_ENRST, NULL, 0);
  prv_write_register(SPI_NOR_CMD_RST, NULL, 0);
  NRFX_DELAY_US(35);

  // enable 4 byte addressing
  prv_write_register(SPI_NOR_CMD_EN4B, NULL, 0);

  // enable QE bit
  prv_read_register(SPI_NOR_CMD_RDSR1, &sr[0], 1);
  prv_read_register(SPI_NOR_CMD_RDSR2, &sr[1], 1);
  sr[1] |= (1 << 1);
  prv_write_register(SPI_NOR_CMD_WRSR, sr, 2);
}

bool flash_sanity_check(void) {
  uint8_t buf[3];
  uint32_t id;

  prv_read_register(SPI_NOR_CMD_RDID, buf, sizeof(buf));

  id = buf[0] << 16 | buf[1] << 8 | buf[2];
  dbgserial_print("JEDEC ID: ");
  dbgserial_print_hex(id);
  dbgserial_newline();

  return (id == BOARD_FLASH_JEDEC_ID);
}

void flash_read_bytes(uint8_t *buffer_ptr, uint32_t start_addr, uint32_t buffer_size) {
  uint8_t __attribute__((aligned(4))) b_buf[4];
  uint8_t buf_pre;
  uint8_t buf_suf;
  uint32_t buf_mid;
  nrfx_err_t err;

  buf_pre = (4U - (uint8_t)((uint32_t)buffer_ptr % 4U)) % 4U;
  if (buf_pre > buffer_size) {
    buf_pre = buffer_size;
  }

  buf_suf = (uint8_t)((buffer_size - buf_pre) % 4U);
  buf_mid = buffer_size - buf_pre - buf_suf;

  if (buf_pre != 0U) {
    err = nrfx_qspi_read(b_buf, 4U, start_addr);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    memcpy(buffer_ptr, b_buf, buf_pre);
    start_addr += buf_pre;
    buffer_ptr = ((uint8_t *)buffer_ptr) + buf_pre;
  }

  if (buf_mid != 0U) {
    err = nrfx_qspi_read(buffer_ptr, buf_mid, start_addr);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    start_addr += buf_mid;
    buffer_ptr = ((uint8_t *)buffer_ptr) + buf_mid;
  }

  if (buf_suf != 0U) {
    err = nrfx_qspi_read(b_buf, 4U, start_addr);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    memcpy(buffer_ptr, b_buf, buf_suf);
  }
}
