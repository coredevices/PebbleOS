#ifndef _HAPTIC_NV_H_
#define _HAPTIC_NV_H_
#include <stdint.h>
#include <stdio.h>
#include "haptic_nv_reg.h"
#include "system/logging.h"
/*********************************************************
 *
 * Macro Control
 *
 *********************************************************/
#define AWINIC_DEBUG_LOG
#define AWINIC_INFO_LOG
#define AWINIC_ERR_LOG
#define AW_CHECK_RAM_DATA
/* #define AW_IRQ_CONFIG */
#define AW_RST_CONFIG
#define AW_F0_CALI_DURING_STARTUP
/* #define AW_RTP_AUTOSIN */
/* #define AW_ENABLE_RTP_PRINT_LOG */
/* #define AW862X_MUL_GET_F0 */
/* #define AW862XX_RAM_GET_F0 */
/* #define HAPTIC_NV_DOUBLE */
/* #define AW_MAXIMUM_F0_CALI_DATA */
/* #define AW862X_DRIVER */
#define AW862XX_DRIVER
/* #define AW8623X_DRIVER */
/* #define AW8624X_DRIVER*/
/*********************************************************
 *
 * Haptic_NV CHIPID
 *
 *********************************************************/
#define AW8623_CHIP_ID						(0x23)
#define AW8624_CHIP_ID						(0x24)
#define AW8622X_CHIP_ID						(0x00)
#define AW86214_CHIP_ID						(0x01)
#define AW8623X_CHIP_ID_H					(0x23)
#define AW86233_CHIP_ID						(0x2330)
#define AW86234_CHIP_ID						(0x2340)
#define AW86235_CHIP_ID						(0x2350)
#define AW8624X_CHIP_ID_H					(0x24)
#define AW86243_CHIP_ID						(0x2430)
#define AW86245_CHIP_ID						(0x2450)
/*********************************************************
 *
 * Haptic_NV I2C_ADDR
 *
 *********************************************************/
#define AW862X_I2C_ADDR						(0x5A)
#define AW862XX_I2C_ADDR					(0x58)
/*********************************************************
 *
 * Marco
 *
 *********************************************************/
#define AW_I2C_NAME							"haptic_nv"
#define AW_I2C_RETRIES						(5)
#define AW_REG_ID							(0x00)
#define AW_REG_CHIPIDH						(0x57) /* AW8623X */
#define AW_SOFT_RESET						(0xAA)
#define AW_REG_MAX							(0xFF)
#define AW_TRIG_NUM							(3)
#define AW_VBAT_MIN							(3000)
#define AW_VBAT_MAX							(4500)
#define AW_VBAT_REFER						(4200)
#define AW_CONT_F0_VBAT_REFER				(4000)
#define AW_LOOP_NUM_MAX						(15)
#define AW_READ_CHIPID_RETRIES				(5)
#define AW_DEFAULT_GAIN						(0x80)
#define AW_SEQUENCER_SIZE					(8)
#define AW_RAMDATA_RD_BUFFER_SIZE			(1) /* 1024 */
#define AW_RAMDATA_WR_BUFFER_SIZE			(1) /* 2048 */

#define AW_I2C_BYTE_ONE						(1)
#define AW_I2C_BYTE_TWO						(2)
#define AW_I2C_BYTE_THREE					(3)
#define AW_I2C_BYTE_FOUR					(4)
#define AW_I2C_BYTE_FIVE					(5)
#define AW_I2C_BYTE_SIX						(6)
#define AW_I2C_BYTE_SEVEN					(7)
#define AW_I2C_BYTE_EIGHT					(8)

#define AW_RL_DELAY							(3)
#define AW_F0_DELAY							(10)
#define AW_RTP_DELAY						(2)
#define AW_PLAY_DELAY						(2)
#define AW_STOP_DELAY						(2)
#define AW_VBAT_DELAY						(2)
#define AW_CALI_DELAY						(3)

#define AW_SET_RAMADDR_H(addr)				((addr) >> 8)
#define AW_SET_RAMADDR_L(addr)				((addr) & 0x00FF)
#define AW_SET_BASEADDR_H(addr)				((addr) >> 8)
#define AW_SET_BASEADDR_L(addr)				((addr) & 0x00FF)
/*********************************************************
 *
 * aw862x marco
 *
 ********************************************************/
#define AW862X_F0_CALI_ACCURACY				(25)
#define AW862X_MUL_GET_F0_RANGE				(150)
#define AW862X_MUL_GET_F0_NUM				(3)

#define AW862X_VBAT_FORMULA(code)			(6100 * (code) / 256)
#define AW862X_F0_FORMULA(reg, coeff)		(1000000000 / ((reg) * (coeff)))
#define AW862X_RL_FORMULA(reg_val)			(298 * (reg_val))
#define AW862X_SET_AEADDR_H(addr)			((((addr) >> 1) >> 8))
#define AW862X_SET_AEADDR_L(addr)			(((addr) >> 1) & 0x00FF)
#define AW862X_SET_AFADDR_H(addr)			(((addr) - ((addr) >> 2)) >> 8)
#define AW862X_SET_AFADDR_L(addr)			(((addr) - ((addr) >> 2)) & 0x00FF)
/*********************************************************
 *
 * aw862xx marco
 *
 ********************************************************/
#define AW862XX_DRV2_LVL_MAX				(127)
#define AW862XX_DRV_WIDTH_MIN				(0)
#define AW862XX_DRV_WIDTH_MAX				(255)
#define AW862XX_F0_CALI_ACCURACY			(24)

#define AW862XX_VBAT_FORMULA(code)			(6100 * (code) / 1024)
#define AW862XX_OS_FORMULA(os_code, d2s_gain)	(2440 * ((os_code) - 512) / (1024 * ((d2s_gain) + 1)))
#define AW862XX_F0_FORMULA(code)			(384000 * 10 / (code))
#define AW862XX_RL_FORMULA(code, d2s_gain)	(((code) * 678 * 100) / (1024 * (d2s_gain)))
#define AW862XX_SET_AEADDR_H(addr)			((((addr) >> 1) >> 4) & 0xF0)
#define AW862XX_SET_AEADDR_L(addr)			(((addr) >> 1) & 0x00FF)
#define AW862XX_SET_AFADDR_H(addr)			((((addr) - ((addr) >> 2)) >> 8) & 0x0F)
#define AW862XX_SET_AFADDR_L(addr)			(((addr) - ((addr) >> 2)) & 0x00FF)
#define AW862XX_DRV2_LVL_FORMULA(f0, vrms)	((((f0) < 1800) ? 1809920 : 1990912) / 1000 * (vrms) / 61000)
#define AW862XX_DRV_WIDTH_FORMULA(f0, margin, brk_gain) \
											((240000 / (f0)) - (margin) - (brk_gain) - 8 )

/*********************************************************
 *
 * aw8623x marco
 *
 ********************************************************/
#define AW8623X_DRV2_LVL_MAX				(127)
#define AW8623X_DRV_WIDTH_MIN				(0)
#define AW8623X_DRV_WIDTH_MAX				(255)
#define AW8623X_F0_CALI_ACCURACY			(24)

#define AW8623X_VBAT_FORMULA(code)			(6100 * (code) / 1023)
#define AW8623X_F0_FORMULA(code)			(384000 * 10 / (code))
#define AW8623X_RL_FORMULA(code, d2s_gain)	(((code) * 678 * 1000) / (1023 * (d2s_gain)))
#define AW8623X_OS_FORMULA(code, d2s_gain)	(2440 * ((code) - 512) / (1023 * (1 + d2s_gain)))
#define AW8623X_SET_AEADDR_H(addr)			((((addr) >> 1) >> 4) & 0xF0)
#define AW8623X_SET_AEADDR_L(addr)			(((addr) >> 1) & 0x00FF)
#define AW8623X_SET_AFADDR_H(addr)			((((addr) - ((addr) >> 2)) >> 8) & 0x0F)
#define AW8623X_SET_AFADDR_L(addr)			(((addr) - ((addr) >> 2)) & 0x00FF)
#define AW8623X_DRV2_LVL_FORMULA(f0, vrms)	((((f0) < 1800) ? 1809920 : 1990912) / 1000 * (vrms) / 40000)
#define AW8623X_DRV_WIDTH_FORMULA(f0, margin, brk_gain) \
						((240000 / (f0)) - (margin) - (brk_gain) - 8)

/*********************************************************
 *
 * aw8624x marco
 *
 ********************************************************/
#define AW8624X_DRV2_LVL_MAX				(127)
#define AW8624X_DRV_WIDTH_MIN				(0)
#define AW8624X_DRV_WIDTH_MAX				(255)
#define AW8624X_F0_CALI_ACCURACY			(24)

#define AW8624X_VBAT_FORMULA(code)			(6100 * (code) / 1023)
#define AW8624X_F0_FORMULA(code)			(384000 * 10 / (code))
#define AW8624X_RL_FORMULA(code, d2s_gain)	(((code) * 610 * 1000) / (1023 * (d2s_gain)))
#define AW8624X_OS_FORMULA(code, d2s_gain)	(2440 * ((code) - 512) / (1023 * (1 + d2s_gain)))
#define AW8624X_SET_AEADDR_H(addr)			((((addr) >> 1) >> 4) & 0xF0)
#define AW8624X_SET_AEADDR_L(addr)			(((addr) >> 1) & 0x00FF)
#define AW8624X_SET_AFADDR_H(addr)			((((addr) - ((addr) >> 2)) >> 8) & 0x0F)
#define AW8624X_SET_AFADDR_L(addr)			(((addr) - ((addr) >> 2)) & 0x00FF)
#define AW8624X_DRV2_LVL_FORMULA(f0, vrms)	((((f0) < 1800) ? 1809920 : 1990912) / 1000 * (vrms) / 61000)
#define AW8624X_DRV_WIDTH_FORMULA(f0, margin, brk_gain) \
						((240000 / (f0)) - (margin) - (brk_gain) - 8)

#ifdef AWINIC_ERR_LOG
#ifdef HAPTIC_NV_DOUBLE
#define AW_LOGE(format, ...)				PBL_LOG(LOG_LEVEL_ERROR, "[E][haptic_nv][%s]%s: " format "\r\n", g_haptic_nv->mark ,__func__, ##__VA_ARGS__)
#else
#define AW_LOGE(format, ...)				PBL_LOG(LOG_LEVEL_ERROR, "%s %d " format, __func__, __LINE__, ## __VA_ARGS__)
#endif
#else
#define AW_LOGE(format, ...)
#endif

#ifdef AWINIC_INFO_LOG
#ifdef HAPTIC_NV_DOUBLE
#define AW_LOGI(format, ...)				PBL_LOG(LOG_LEVEL_INFO, "[I][haptic_nv][%s]%s: " format "\r\n", g_haptic_nv->mark, __func__, ##__VA_ARGS__)
#else
#define AW_LOGI(format, ...)				PBL_LOG(LOG_LEVEL_INFO, "%s %d " format, __func__, __LINE__, ## __VA_ARGS__)
#endif
#else
#define AW_LOGI(format, ...)
#endif

#ifdef AWINIC_DEBUG_LOG
#ifdef HAPTIC_NV_DOUBLE
#define AW_LOGD(format, ...)				PBL_LOG(LOG_LEVEL_DEBUG, "[D][haptic_nv][%s]%s: " format "\r\n", g_haptic_nv->mark, __func__, ##__VA_ARGS__)
#else
#define AW_LOGD(format, ...)				PBL_LOG(LOG_LEVEL_INFO, "%s %d " format, __func__, __LINE__, ## __VA_ARGS__)
#endif
#else
#define AW_LOGD(format, ...)
#endif

/*********************************************************
 *
 * Enum Define
 *
 *********************************************************/
enum {
	AW_SUCCESS = 0,
	AW_ERROR = 1,
};

enum aw_haptic_irq_state {
	AW_IRQ_NULL = 0,
	AW_IRQ_UVL = 1,
	AW_IRQ_OCD = 2,
	AW_IRQ_OT = 3,
	AW_IRQ_DONE = 4,
	AW_IRQ_ALMOST_FULL = 5,
	AW_IRQ_ALMOST_EMPTY = 6,
};

enum aw_haptic_work_mode {
	AW_STANDBY_MODE = 0,
	AW_RAM_MODE = 1,
	AW_RAM_LOOP_MODE = 2,
	AW_CONT_MODE = 3,
	AW_RTP_MODE = 4,
	AW_TRIG_MODE = 5,
	AW_NULL = 6,
};

enum aw_haptic_cont_vbat_comp_mode {
	AW_CONT_VBAT_SW_COMP_MODE = 0,
	AW_CONT_VBAT_HW_COMP_MODE = 1,
};

enum aw_haptic_ram_vbat_comp_mode {
	AW_RAM_VBAT_COMP_DISABLE = 0,
	AW_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw_haptic_pwm_mode {
	AW_PWM_48K = 0,
	AW_PWM_24K = 1,
	AW_PWM_12K = 2,
};

enum aw_haptic_cali_lra {
	AW_WRITE_ZERO = 0,
	AW_F0_CALI_LRA = 1,
};

enum aw_haptic_read_chip_type {
	AW_FIRST_TRY = 0,
	AW_LAST_TRY = 1,
};

enum aw_haptic_chip_name {
	AW_CHIP_NULL = 0,
	AW86223 = 1,
	AW86224 = 2,
	AW86225 = 3,
	AW86214 = 4,
	AW8623 = 5,
	AW8624 = 6,
	AW86233 = 7,
	AW86234 = 8,
	AW86235 = 9,
	AW86243 = 10,
	AW86245 = 11,
};

enum aw_haptic_protect_config {
	AW_PROTECT_EN = 1,
	AW_PROTECT_OFF = 0,
	AW_PROTECT_CFG_1 = 0x2D,
	AW_PROTECT_CFG_2 = 0x3E,
	AW_PROTECT_CFG_3 = 0X3F,
};

enum aw_haptic_pin {
	AW_TRIG1 = 0,
	AW_TRIG2 = 1,
	AW_TRIG3 = 2,
	AW_IRQ = 3,
};

enum aw_pin_control_flag {
	AW_PIN_LOW = 0,
	AW_PIN_HIGH = 1,
};

enum {
	AW_IRQ_OFF = 0,
	AW_IRQ_ON = 1,
};

typedef enum {
	AW_FALSE = 0,
	AW_TRUE = 1,
} aw_bool;

typedef enum {
    MOTOR_L = 0,
    MOTOR_R = 1,
} haptic_nv_motor_name;

enum aw_trim_lra {
	AW_TRIM_LRA_BOUNDARY = 0x20,
	AW8624X_TRIM_LRA_BOUNDARY = 0x40,
};
/*********************************************************
 *
 * Enum aw8623x
 *
 *********************************************************/
enum aw8623x_sram_size_flag {
	AW8623X_HAPTIC_SRAM_1K = 0,
	AW8623X_HAPTIC_SRAM_2K = 1,
	AW8623X_HAPTIC_SRAM_3K = 2,
};

/*********************************************************
 *
 * Enum aw862xx
 *
 *********************************************************/
enum aw862xx_ef_id {
	AW86223_EF_ID = 0x01,
	AW86224_EF_ID = 0x00,
	AW86225_EF_ID = 0x00,
	AW86214_EF_ID = 0x41,
};

enum aw862xx_sram_size_flag {
	AW862XX_HAPTIC_SRAM_1K = 0,
	AW862XX_HAPTIC_SRAM_2K = 1,
	AW862XX_HAPTIC_SRAM_3K = 2,
};
/*********************************************************
 *
 * Struct Define
 *
 *********************************************************/
struct trig {
	uint8_t enable;
	uint8_t trig_edge;
	uint8_t trig_brk;
	uint8_t trig_level;
	uint8_t trig_polar;
	uint8_t pos_enable;
	uint8_t neg_enable;
	uint8_t pos_sequence;
	uint8_t neg_sequence;
};

struct aw_haptic_ram {
	uint8_t ram_num;
	uint8_t ram_shift;
	uint8_t baseaddr_shift;
	uint32_t len;
	uint32_t check_sum;
	uint32_t base_addr;
};

struct aw_haptic_dts_info {
	aw_bool is_enabled_auto_brk;
	/* aw8624x */
	aw_bool is_enabled_smart_loop;
	aw_bool is_enabled_inter_brake;

	uint32_t f0_cali_percent;
	uint32_t f0_pre;
	uint8_t cont_tset;
	uint8_t cont_drv1_lvl;
	uint8_t cont_drv2_lvl;
	/* aw862x */
	uint32_t cont_td;
	uint32_t cont_zc_thr;
	uint32_t f0_coeff;
	uint8_t cont_num_brk;
	uint8_t cont_brake[8];
	uint8_t bemf_config[4];
	uint8_t sw_brake[2];
	uint8_t f0_trace_parameter[4];
	/* aw862xx */
	uint32_t lra_vrms;
	uint8_t cont_drv1_time;
	uint8_t cont_drv2_time;
	uint8_t cont_brk_time;
	uint8_t cont_track_margin;
	uint8_t cont_drv_width;
	uint8_t cont_brk_gain;
	uint8_t d2s_gain;
	/* aw8624x */
	uint8_t f0_d2s_gain;
};

struct aw_haptic_container {
	uint32_t len;
	uint8_t *data;
};

struct haptic_nv {
	aw_bool rtp_init;
	aw_bool ram_init;
	aw_bool is_used_irq_pin;

	uint8_t i2c_addr;
	uint8_t play_mode;
	uint8_t chipid_flag;
	uint8_t irq_handle;
	uint8_t max_pos_beme;
	uint8_t max_neg_beme;
	uint8_t f0_cali_data;
	uint8_t ram_vbat_comp;
	uint8_t trim_lra_boundary;
#ifdef HAPTIC_NV_DOUBLE
	char mark[15];
#endif

	uint16_t rst_pin;

	uint32_t f0;
	uint32_t lra;
	uint32_t name;
	uint32_t vbat;
	uint32_t gain;
	uint32_t f0_pre;
	uint32_t rtp_cnt;
	uint32_t duration;
	uint32_t timer_ms_cnt;

	struct aw_haptic_ram ram;
	struct trig *trig[AW_TRIG_NUM];
	struct aw_haptic_dts_info *info;
	struct aw_haptic_container aw_fw;
};

struct aw_haptic_func {
	int (*check_qualify)(void);
	int (*get_irq_state)(void);
	int (*get_f0)(void);
	int (*offset_cali)(void);
	void (*trig_init)(void);
	void (*irq_clear)(void);
	void (*haptic_start)(void);
	void (*play_stop)(void);
	void (*cont_config)(void);
	void (*play_mode)(uint8_t);
	void (*ram_init)(aw_bool);
	void (*misc_para_init)(void);
	void (*interrupt_setup)(void);
	void (*vbat_mode_config)(uint8_t);
	void (*protect_config)(uint8_t, uint8_t);
	void (*calculate_cali_data)(void);
	void (*set_gain)(uint8_t);
	void (*set_wav_seq)(uint8_t, uint8_t);
	void (*set_wav_loop)(uint8_t, uint8_t);
	void (*set_rtp_data)(uint8_t *, uint32_t);
	void (*set_rtp_autosin)(uint8_t);
	void (*set_fifo_addr)(void);
	void (*get_fifo_addr)(void);
	void (*set_ram_data)(uint8_t *, int);
	void (*get_ram_data)(uint8_t *, int);
	void (*set_ram_addr)(void);
	void (*set_repeat_seq)(uint8_t);
	void (*set_base_addr)(void);
	void (*set_trim_lra)(uint8_t);
	void (*set_rtp_aei)(aw_bool);
	void (*get_vbat)(void);
	void (*get_reg)(void);
	void (*get_lra_resistance)(void);
	uint8_t (*get_glb_state)(void);
	uint8_t (*judge_rtp_going)(void);
	uint8_t (*rtp_get_fifo_afs)(void);

	void (*f0_show)(void);
	void (*ram_show)(void);
	void (*cali_show)(void);
	void (*irq_handle)(void);
	void (*get_ram_num)(void);
	void (*rtp_vib_work)(uint8_t gain);
	void (*set_hw_irq_status)(uint8_t);
	uint8_t (*get_hw_irq_status)(void);
	int (*f0_cali)(void);
	int (*rtp_going)(void);
	int (*long_vib_work)(uint8_t, uint8_t, uint32_t);
	int (*short_vib_work)(uint8_t, uint8_t, uint8_t);
#ifdef HAPTIC_NV_DOUBLE
	int (*dual_short_vib)(uint8_t, uint8_t, uint8_t,
						  uint8_t, uint8_t, uint8_t);
	int (*dual_long_vib)(uint8_t, uint8_t,
						 uint8_t, uint8_t, uint32_t);
#endif
};

extern int haptic_nv_i2c_reads(uint8_t, uint8_t *, uint16_t);
extern int haptic_nv_i2c_writes(uint8_t, uint8_t *, uint16_t);
extern int haptic_nv_read_chipid(uint32_t *, uint8_t);
extern void haptic_nv_i2c_write_bits(uint8_t, uint32_t, uint8_t);
extern void haptic_nv_mdelay(uint32_t);
extern void haptic_nv_udelay(uint32_t);
extern void haptic_nv_stop_hrtimer(void);
extern void haptic_nv_start_hrtimer(void);
extern void haptic_nv_disable_irq(void);
extern void haptic_nv_enable_irq(void);
extern void haptic_nv_pin_control(uint16_t, uint8_t);
extern void haptic_nv_set_cali_to_flash(void);
extern void haptic_nv_get_cali_from_flash(void);
extern struct haptic_nv *g_haptic_nv;

extern uint8_t haptic_nv_rtp_data[];
extern uint32_t haptic_nv_rtp_len;

#ifdef AW862X_DRIVER
extern struct aw_haptic_func aw862x_func_list;
extern struct aw_haptic_dts_info aw862x_dts;
extern uint8_t aw862x_ram_data[];
extern uint32_t aw862x_ram_len;
#endif

#ifdef AW862XX_DRIVER
extern struct aw_haptic_func aw862xx_func_list;
extern struct aw_haptic_dts_info aw8622x_dts;
#endif

#ifdef AW8623X_DRIVER
extern struct aw_haptic_func aw8623x_func_list;
extern struct aw_haptic_dts_info aw8623x_dts;
#endif

#ifdef AW8624X_DRIVER
extern struct aw_haptic_func aw8624x_func_list;
extern struct aw_haptic_dts_info aw8624x_dts;
#endif

#if defined (AW862XX_DRIVER) || defined (AW8623X_DRIVER) || defined (AW8624X_DRIVER)
extern uint8_t aw862xx_ram_data[];
extern uint32_t aw862xx_ram_len;
#endif

/*****************************************************
 * haptic_nv_boot_init
 * It is initialization function,
 * please call it to complete initialization first,
 * and then pass g_func_haptic_nv pointer calling function.
 *****************************************************/
#ifdef HAPTIC_NV_DOUBLE
extern void haptic_nv_change_motor(haptic_nv_motor_name);
#endif
extern int haptic_nv_boot_init(void);
extern struct aw_haptic_func *g_func_haptic_nv;
extern void haptic_nv_play_start(void);
extern void haptic_nv_play_stop(void);
#endif
