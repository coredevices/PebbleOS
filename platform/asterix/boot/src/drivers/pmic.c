#include <board.h>
#include <drivers/dbgserial.h>
#include <nrfx_twi.h>
#include <system/passert.h>

#define LDSW_BASE 0x08U

#define TASKLDSW2SET 0x02U

#define LDSW2LDOSEL 0x09U
#define LDSW2LDOSEL_LDO 0x01U

#define LDSW2VOUTSEL 0x0DU
#define LDSW2VOUTSEL_1V8 0x08U

static nrfx_twi_t twi = NRFX_TWI_INSTANCE(BOARD_PMIC_I2C);
static const nrfx_twi_config_t config =
    NRFX_TWI_DEFAULT_CONFIG(BOARD_PMIC_I2C_SCL_PIN, BOARD_PMIC_I2C_SDA_PIN);

void pmic_init(void) {
  nrfx_err_t err;
  uint8_t data[3];
  nrfx_twi_xfer_desc_t xfer = NRFX_TWI_XFER_DESC_TX(0x6b, data, sizeof(data));

  err = nrfx_twi_init(&twi, &config, NULL, NULL);
  PBL_ASSERT(err == NRFX_SUCCESS, "TWI init failed: %d", err);

  nrfx_twi_enable(&twi);

  data[0] = LDSW_BASE;
  data[1] = LDSW2LDOSEL;
  data[2] = LDSW2LDOSEL_LDO;
  err = nrfx_twi_xfer(&twi, &xfer, 0);
  PBL_ASSERT(err == NRFX_SUCCESS, "TWI transfer failed: %d", err);

  data[0] = LDSW_BASE;
  data[1] = LDSW2VOUTSEL;
  data[2] = LDSW2VOUTSEL_1V8;
  err = nrfx_twi_xfer(&twi, &xfer, 0);
  PBL_ASSERT(err == NRFX_SUCCESS, "TWI transfer failed: %d", err);

  data[0] = LDSW_BASE;
  data[1] = TASKLDSW2SET;
  data[2] = 0x01U;
  err = nrfx_twi_xfer(&twi, &xfer, 0);
  PBL_ASSERT(err == NRFX_SUCCESS, "TWI transfer failed: %d", err);

  nrfx_twi_disable(&twi);
}
