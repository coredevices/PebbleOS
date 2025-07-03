#include "board/board.h"

#include "drivers/accel.h"
#include "drivers/imu.h"
#include "drivers/spi.h"
#include "kernel/util/delay.h"
#include "drivers/i2c.h"

#define MMC5603_PRODUCT_ID 0x39
#define MMC5603_PRODUCT_ID_VALUE 0x10
#define MMC5603_CONTROL2 0x1D

#define BMP390_CHIP_ID 0x00
#define BMP390_CHIP_ID_VALUE 0x60
#define BMP390_PWR_CTRL 0x1B

#define LSM6D_FUNC_CFG_ACCESS 0x01

#define LSM6D_WHO_AM_I 0x0F
#define LSM6D_WHO_AM_I_VALUE 0x6C

#define LSM6D_CTRL1_XL 0x10
#define LSM6D_CTRL2_G 0x11
#define LSM6D_CTRL4_C 0x13
#define LSM6D_CTRL4_C_SLEEP_G 0x40



static bool prv_read_register(I2CSlavePort *i2c, uint8_t register_address, uint8_t *result) {
  i2c_use(i2c);
  bool rv = i2c_write_block(i2c, 1, &register_address);
  if (rv)
    rv = i2c_read_block(i2c, 1, result);
  i2c_release(i2c);
  return rv;
}

static bool prv_write_register(I2CSlavePort *i2c, uint8_t register_address, uint8_t datum) {
  i2c_use(i2c);
  uint8_t d[2] = { register_address, datum };
  bool rv = i2c_write_block(i2c, 2, d);
  i2c_release(i2c);
  return rv;
}

void imu_init(void) {
  bool rv;
  uint8_t result;
  
  rv = prv_read_register(I2C_MMC5603NJ, MMC5603_PRODUCT_ID, &result);
  if (!rv || result != MMC5603_PRODUCT_ID_VALUE) {
    PBL_LOG(LOG_LEVEL_DEBUG, "MMC5603 probe failed; rv %d, result 0x%02x", rv, result);
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "found the MMC5603NJ, setting to low power");
    (void) prv_write_register(I2C_MMC5603NJ, MMC5603_CONTROL2, 0);
  }

  rv = prv_read_register(I2C_BMP390, BMP390_CHIP_ID, &result);
  if (!rv || result != BMP390_CHIP_ID_VALUE) {
    PBL_LOG(LOG_LEVEL_DEBUG, "BMP390 probe failed; rv %d, result 0x%02x", rv, result);
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "found the BMP390, setting to low power");
    (void) prv_write_register(I2C_BMP390, BMP390_PWR_CTRL, 0);
  }
  
  rv = prv_read_register(I2C_LSM6D, LSM6D_WHO_AM_I, &result);
  if (!rv || result != LSM6D_WHO_AM_I_VALUE) {
    PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO probe failed; rv %d, result 0x%02x", rv, result);
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "found the LSM6DSO, setting to lower power");
    (void) prv_write_register(I2C_LSM6D, LSM6D_FUNC_CFG_ACCESS, 0);
    (void) prv_write_register(I2C_LSM6D, LSM6D_CTRL1_XL, 0);
    (void) prv_write_register(I2C_LSM6D, LSM6D_CTRL2_G, 0);
    (void) prv_write_register(I2C_LSM6D, LSM6D_CTRL4_C, LSM6D_CTRL4_C_SLEEP_G);
  }
}

void imu_power_up(void) {
  // NYI
}

void imu_power_down(void) {
  // NYI
}

