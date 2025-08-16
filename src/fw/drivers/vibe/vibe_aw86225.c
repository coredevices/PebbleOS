#include "drivers/vibe.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "drivers/pwm.h"
#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include "services/common/analytics/analytics.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/battery/battery_state.h"
#include "services/common/analytics/analytics.h"

#include <string.h>
//#include "haptic_nv.h"

#define AW862XX_REG_ID									(0x00)
#define AW862XX_REG_CONTCFG1							(0x18)
#define AW862XX_REG_CONTCFG2							(0x19)
#define AW862XX_REG_CONTCFG3							(0x1A)
#define AW862XX_REG_CONTCFG4							(0x1B)
#define AW862XX_REG_CONTCFG5							(0x1C)
#define AW862XX_REG_CONTCFG6							(0x1D)
#define AW862XX_REG_CONTCFG7							(0x1E)
#define AW862XX_REG_CONTCFG8							(0x1F)
#define AW862XX_REG_CONTCFG9							(0x20)
#define AW862XX_REG_CONTCFG10							(0x21)
#define AW862XX_REG_CONTCFG11							(0x22)
#define AW862XX_REG_CONTCFG12							(0x23)
#define AW862XX_REG_CONTCFG13							(0x24)
#define AW862XX_REG_PLAYCFG3							(0x08)
#define AW862XX_REG_PLAYCFG4							(0x09)


#define AW862XX_BIT_PLAYCFG3_BRK_EN_MASK  (~(1<<2))
#define AW862XX_BIT_PLAYCFG3_BRK					(1<<2)
#define AW862XX_BIT_PLAYCFG3_BRK_ENABLE		(1<<2)
#define AW862XX_BIT_PLAYCFG3_BRK_DISABLE	(0<<2)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK				(~(3<<0))
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_STOP				(3<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT				(2<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RTP				(1<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM				(0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW862XX_BIT_PLAYCFG4_STOP_MASK					(~(1<<1))
#define AW862XX_BIT_PLAYCFG4_STOP_ON					(1<<1)
#define AW862XX_BIT_PLAYCFG4_STOP_OFF					(0<<1)
#define AW862XX_BIT_PLAYCFG4_GO_MASK					(~(1<<0))
#define AW862XX_BIT_PLAYCFG4_GO_ON						(1<<0)
#define AW862XX_BIT_PLAYCFG4_GO_OFF						(0<<0)

static bool s_initialized = false;

static bool prv_read_register(uint8_t register_address, uint8_t *result, uint16_t size) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_read_register_block(I2C_AW86225, register_address, size, result);
	i2c_release(I2C_AW86225);
	return rv;
}
  
static bool prv_write_register(uint8_t register_address, uint8_t* datum, uint16_t size) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_write_register_block(I2C_AW86225, register_address, size, datum);
	i2c_release(I2C_AW86225);
	return rv;
}

void prv_write_bits(uint8_t reg_addr, uint32_t mask, uint8_t reg_data)
{
	uint8_t reg_val = 0;
	uint8_t reg_mask = (uint8_t)mask;

	prv_read_register(reg_addr, &reg_val, 1);
	reg_val &= reg_mask;
	reg_val |= (reg_data & (~reg_mask));
	prv_write_register(reg_addr, &reg_val, 1);
}

static void aw862xx_play_go(bool flag)
{
	uint8_t val = 0;

  if (flag) {
		val = AW862XX_BIT_PLAYCFG4_GO_ON;
		prv_write_register(AW862XX_REG_PLAYCFG4, &val, 1);
	} else {
		val = AW862XX_BIT_PLAYCFG4_STOP_ON;
		prv_write_register(AW862XX_REG_PLAYCFG4, &val, 1);
	}
	//aw862xx_get_glb_state();
}

void vibe_init(void) {
  gpio_output_init(&BOARD_CONFIG_VIBE.ctrl, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctrl, true);
  HAL_Delay(2);
 
  uint8_t chip_id;
  bool ret = prv_read_register(AW862XX_REG_ID, &chip_id, 1);
  if (ret) {
    PBL_LOG(LOG_LEVEL_INFO, "aw86225 get chip:%x", chip_id);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "aw86225 get chip err.");
  }

  uint8_t data = 0xC1;
  prv_write_register(AW862XX_REG_CONTCFG1, &data, 1);
  data = 100;
  prv_write_register(AW862XX_REG_CONTCFG2, &data, 1);
  data = 80;
  prv_write_register(AW862XX_REG_CONTCFG3, &data, 1);
  data = 0xFF;
  prv_write_register(AW862XX_REG_CONTCFG9, &data, 1);
  
  prv_write_bits(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK, AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  prv_write_bits(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK, AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT);


  //aw862xx_play_go(true);

  //gpio_output_set(&BOARD_CONFIG_VIBE.ctrl, false);
  s_initialized = true;
}

static bool s_vibe_ctl_on = false;

/* Sadly, you cannot play music with DRV2604 this way.  Maybe we should
 * modulate DRIVE_TIME too?
 */
void vibe_set_strength(int8_t strength) {


}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on) {
    PBL_LOG(LOG_LEVEL_DEBUG, "vibe ctrl on");
    aw862xx_play_go(true);
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "vibe ctrl off");
    aw862xx_play_go(false);
  }

}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }

}

int8_t vibe_get_braking_strength(void) {
  // We support the -100..100 range because BIDIR_INPUT is set
  return VIBE_STRENGTH_MIN;
}


void command_vibe_ctl(const char *arg) {
  int strength = atoi(arg);

  const bool out_of_bounds = ((strength < 0) || (strength > VIBE_STRENGTH_MAX));
  const bool not_a_number = (strength == 0 && arg[0] != '0');
  if (out_of_bounds || not_a_number) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);

  const bool turn_on = strength != 0;
  vibe_ctl(turn_on);
  prompt_send_response("OK");
}
