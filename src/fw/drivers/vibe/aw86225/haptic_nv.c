/*
 * File: haptic_nv.c
 *
 * Author: <wangzhi@awinic.com>
 *
 * Copyright ©2021-2023 awinic.All Rights Reserved
 *
 */
#include <inttypes.h>
#include "haptic_nv.h"

#define HAPTIC_NV_DRIVER_VERSION			"v1.0.0"

struct aw_haptic_func *g_func_haptic_nv = NULL;

#ifndef HAPTIC_NV_DOUBLE
struct haptic_nv haptic_nv = {
	.i2c_addr = AW862X_I2C_ADDR,
	.rst_pin = -1,
	.gain = 0x80,
	.timer_ms_cnt = 0,
	.name = AW_CHIP_NULL,
	.is_used_irq_pin = AW_FALSE,
};
struct haptic_nv *g_haptic_nv = &haptic_nv;
#else
struct haptic_nv haptic_nv_l = {
	.i2c_addr = AW862X_I2C_ADDR,
	.rst_pin = AW_RST_Pin_L,
	.gain = 0x80,
	.timer_ms_cnt = 0,
	.name = AW_CHIP_NULL,
	.is_used_irq_pin = AW_FALSE,
};
struct haptic_nv haptic_nv_r = {
	.i2c_addr = AW862X_I2C_ADDR,
	.rst_pin = AW_RST_Pin_R,
	.gain = 0x80,
	.timer_ms_cnt = 0,
	.name = AW_CHIP_NULL,
	.is_used_irq_pin = AW_FALSE,
};
struct haptic_nv *g_haptic_nv = &haptic_nv_l;

void haptic_nv_change_motor(haptic_nv_motor_name motor_name)
{
	switch (motor_name) {
		case MOTOR_L:
			g_haptic_nv = &haptic_nv_l;
			strcpy(g_haptic_nv->mark , "left");
			break;
		case MOTOR_R:
			g_haptic_nv = &haptic_nv_r;
			strcpy(g_haptic_nv->mark , "right");
			break;
		default:
			break;
	}
}
#endif

static int get_base_addr(void)
{
	uint16_t last_end = 0;
	uint16_t next_start = 0;
	uint32_t i = 0;
	int ram_num = 1;

	for (i = 3; i < g_haptic_nv->aw_fw.len; i = i + 4) {
		last_end = (g_haptic_nv->aw_fw.data[i] << 8) | g_haptic_nv->aw_fw.data[i + 1];
		next_start = (g_haptic_nv->aw_fw.data[i + 2] << 8) | g_haptic_nv->aw_fw.data[i + 3];
		if ((next_start - last_end) == 1)
			ram_num++;
		else
			break;
	}

	for (i = ram_num * 4; i >= 4; i = i - 4) {
		last_end = (g_haptic_nv->aw_fw.data[i - 1] << 8) | g_haptic_nv->aw_fw.data[i];
		g_haptic_nv->ram.base_addr = (int)((g_haptic_nv->aw_fw.data[1] << 8) | g_haptic_nv->aw_fw.data[2]) - ram_num * 4 - 1;
		if ((last_end - g_haptic_nv->ram.base_addr + 1) == g_haptic_nv->aw_fw.len) {
			AW_LOGI("base_addr = 0x%04" PRIx32, g_haptic_nv->ram.base_addr);
			return AW_SUCCESS;
		} else {
			ram_num--;
		}
	}
	return AW_ERROR;
}

static void ram_vbat_comp(aw_bool flag)
{
	int temp_gain = 0;

	if (flag) {
		if (g_haptic_nv->ram_vbat_comp == AW_RAM_VBAT_COMP_ENABLE) {

			g_func_haptic_nv->get_vbat();
			temp_gain = g_haptic_nv->gain * AW_VBAT_REFER / g_haptic_nv->vbat;
			if (temp_gain > (AW_DEFAULT_GAIN * AW_VBAT_REFER / AW_VBAT_MIN)) {
				temp_gain = AW_DEFAULT_GAIN * AW_VBAT_REFER / AW_VBAT_MIN;
				AW_LOGI("gain limit=%d", temp_gain);
			}
			g_func_haptic_nv->set_gain(temp_gain);
			AW_LOGI("ram vbat comp open");
		} else {
			g_func_haptic_nv->set_gain(g_haptic_nv->gain);
			AW_LOGI("ram vbat comp close");
		}
	} else {
		g_func_haptic_nv->set_gain(g_haptic_nv->gain);
		AW_LOGI("ram vbat comp close");
	}
}

static void upload_lra(uint8_t flag)
{
	uint8_t reg_val;

	switch (flag) {
	case AW_WRITE_ZERO:
		AW_LOGI("write zero to trim_lra!");
		reg_val = 0x00;
		break;
	case AW_F0_CALI_LRA:
		AW_LOGI("write f0_cali_data to trim_lra = 0x%02X", g_haptic_nv->f0_cali_data);
		reg_val = g_haptic_nv->f0_cali_data;
		break;
	default:
		AW_LOGE("flag is error");
		reg_val = 0x00;
		break;
	}
	g_func_haptic_nv->set_trim_lra(reg_val);
}

static void haptic_nv_vib_work_cancel(void)
{
	/* canel before timer handler || rtp */
	if ((g_haptic_nv->timer_ms_cnt != 0) || (g_haptic_nv->rtp_init == AW_TRUE)) {
		haptic_nv_stop_hrtimer();
		g_haptic_nv->rtp_init = AW_FALSE;
		g_haptic_nv->timer_ms_cnt = 0;
	}
	g_func_haptic_nv->play_stop();
}
static int long_vib_work(uint8_t index, uint8_t gain, uint32_t duration)
{
	if (!g_haptic_nv->ram_init) {
		AW_LOGE("ram init failed, ram_num = 0!");
		return AW_ERROR;
	}
	if ((duration == 0) || (index == 0)) {
		AW_LOGE("duration = %" PRId32 ", index = %d, err", duration, index);
		return AW_ERROR;
	}

	AW_LOGI("start duration = %" PRId32 ", index = %d", duration, index);
	haptic_nv_vib_work_cancel();


	g_haptic_nv->gain = gain;
	g_haptic_nv->duration = duration;
	upload_lra(AW_F0_CALI_LRA);
	ram_vbat_comp(AW_TRUE);
	g_func_haptic_nv->set_repeat_seq(index);
	g_func_haptic_nv->play_mode(AW_RAM_LOOP_MODE);
	g_func_haptic_nv->haptic_start();
	haptic_nv_start_hrtimer();
	return AW_SUCCESS;
}

static int short_vib_work(uint8_t index, uint8_t gain, uint8_t loop)
{
	if (!g_haptic_nv->ram_init) {
		AW_LOGE("ram init failed, ram_num = 0!");
		return AW_ERROR;
	}
	if ((loop >= AW_LOOP_NUM_MAX) || (index == 0) || (index > g_haptic_nv->ram.ram_num)) {
		AW_LOGE("loop = %d, index = %d, err", loop, index);
		//return AW_ERROR;
	}

	AW_LOGI("start loop = %d, index = %d", loop, index);

	haptic_nv_vib_work_cancel();
	upload_lra(AW_F0_CALI_LRA);
	g_func_haptic_nv->set_wav_seq(0x00, index);
	g_func_haptic_nv->set_wav_seq(0x01, 0x00);
	g_func_haptic_nv->set_wav_loop(0x00, loop - 1);
	g_func_haptic_nv->set_gain(gain);
	g_func_haptic_nv->play_mode(AW_RAM_MODE);
	g_func_haptic_nv->haptic_start();
	return AW_SUCCESS;
}
#ifdef HAPTIC_NV_DOUBLE
static int haptic_nv_dual_short_vib_work(uint8_t index_l, uint8_t gain_l, uint8_t loop_l,
										 uint8_t index_r, uint8_t gain_r, uint8_t loop_r)
{
	haptic_nv_change_motor(MOTOR_L);
	if (!g_haptic_nv->ram_init) {
		AW_LOGE("ram init failed, ram_num = 0!");
		return AW_ERROR;
	}
	if ((loop_l >= AW_LOOP_NUM_MAX) || (index_l == 0) || (index_l > g_haptic_nv->ram.ram_num)) {
		AW_LOGE("loop_l = %d, index_l = %d, err", loop_l, index_l);
		return AW_ERROR;
	}
	AW_LOGI("start loop_l = %d, index_l = %d", loop_l, index_l);

	haptic_nv_vib_work_cancel();
	upload_lra(AW_F0_CALI_LRA);
	g_func_haptic_nv->set_wav_seq(0x00, index_l);
	g_func_haptic_nv->set_wav_seq(0x01, 0x00);
	g_func_haptic_nv->set_wav_loop(0x00, loop_l - 1);
	g_func_haptic_nv->set_gain(gain_l);
	g_func_haptic_nv->play_mode(AW_RAM_MODE);

	haptic_nv_change_motor(MOTOR_R);
	if ((loop_r >= AW_LOOP_NUM_MAX) || (index_r == 0) || (index_r > g_haptic_nv->ram.ram_num)) {
		AW_LOGE("loop_r = %d, index_r = %d, err", loop_r, index_r);
		return AW_ERROR;
	}
	AW_LOGI("start loop_r = %d, index_r = %d", loop_r, index_r);
	haptic_nv_vib_work_cancel();
	upload_lra(AW_F0_CALI_LRA);
	g_func_haptic_nv->set_wav_seq(0x00, index_r);
	g_func_haptic_nv->set_wav_seq(0x01, 0x00);
	g_func_haptic_nv->set_wav_loop(0x00, loop_r - 1);
	g_func_haptic_nv->set_gain(gain_r);
	g_func_haptic_nv->play_mode(AW_RAM_MODE);

	haptic_nv_change_motor(MOTOR_L);
	g_func_haptic_nv->haptic_start();
	haptic_nv_change_motor(MOTOR_R);
	g_func_haptic_nv->haptic_start();
	return AW_SUCCESS;
};

static int haptic_nv_dual_long_vib_work(uint8_t index_l, uint8_t gain_l,
									    uint8_t index_r, uint8_t gain_r, uint32_t duration)
{
	haptic_nv_change_motor(MOTOR_L);
	if (!g_haptic_nv->ram_init) {
		AW_LOGE("ram init failed, ram_num = 0!");
		return AW_ERROR;
	}
	if ((duration == 0) || (index_l == 0) || (index_l > g_haptic_nv->ram.ram_num)) {
		AW_LOGE("duration = %d, index_l = %d, err", duration, index_l);
		return AW_ERROR;
	}
	AW_LOGI("start duration = %d, index_l = %d", duration, index_l);

	haptic_nv_vib_work_cancel();
	g_haptic_nv->gain = gain_l;
	g_haptic_nv->duration = duration;
	upload_lra(AW_F0_CALI_LRA);
	ram_vbat_comp(AW_TRUE);
	g_func_haptic_nv->set_repeat_seq(index_l);
	g_func_haptic_nv->play_mode(AW_RAM_LOOP_MODE);

	haptic_nv_change_motor(MOTOR_R);
	if ((duration == 0) || (index_r == 0) || (index_r > g_haptic_nv->ram.ram_num)) {
		AW_LOGE("duration = %d, index_r = %d, err", duration, index_r);
		return AW_ERROR;
	}
	AW_LOGI("start duration = %d, index_r = %d", duration, index_r);
	haptic_nv_vib_work_cancel();
	g_haptic_nv->gain = gain_r;
	g_haptic_nv->duration = duration;
	upload_lra(AW_F0_CALI_LRA);
	ram_vbat_comp(AW_TRUE);
	g_func_haptic_nv->set_repeat_seq(index_r);
	g_func_haptic_nv->play_mode(AW_RAM_LOOP_MODE);

	haptic_nv_change_motor(MOTOR_L);
	g_func_haptic_nv->haptic_start();
	haptic_nv_change_motor(MOTOR_R);
	g_func_haptic_nv->haptic_start();
	haptic_nv_start_hrtimer();
	return AW_SUCCESS;

}
#endif


static int judge_within_cali_range(void)
{
	uint32_t f0_cali_min = 0;
	uint32_t f0_cali_max = 0;

	f0_cali_min = g_haptic_nv->info->f0_pre * (100 - g_haptic_nv->info->f0_cali_percent) / 100;
	f0_cali_max = g_haptic_nv->info->f0_pre * (100 + g_haptic_nv->info->f0_cali_percent) / 100;

	AW_LOGI("f0_pre = %" PRId32 ", f0_cali_min = %" PRId32 ", f0_cali_max = %" PRId32 ", f0 = %" PRId32 "",
			g_haptic_nv->info->f0_pre, f0_cali_min, f0_cali_max, g_haptic_nv->f0);

	if (g_haptic_nv->f0 < f0_cali_min) {
		AW_LOGE("lra f0 is too small, lra_f0 = %" PRId32 "!", g_haptic_nv->f0);
#ifdef AW_MAXIMUM_F0_CALI_DATA
		g_haptic_nv->f0_cali_data = aw_haptic->trim_lra_boundary;
		upload_lra(AW_F0_CALI_LRA);
#endif
		return AW_ERROR;
	}

	if (g_haptic_nv->f0 > f0_cali_max) {
		AW_LOGE("lra f0 is too large, lra_f0 = %" PRId32 "!", g_haptic_nv->f0);
#ifdef AW_MAXIMUM_F0_CALI_DATA
		g_haptic_nv->f0_cali_data = aw_haptic->trim_lra_boundary - 1;
		upload_lra(AW_F0_CALI_LRA);
#endif
		return AW_ERROR;
	}

	return AW_SUCCESS;
}

static int f0_cali(void)
{
	int ret = 0;

	AW_LOGI("enter");
	upload_lra(AW_WRITE_ZERO);
	if (g_func_haptic_nv->get_f0()) {
		AW_LOGE("get f0 error, user defafult f0");
	} else {
		/* max and min limit */
		ret = judge_within_cali_range();
		if (ret != AW_SUCCESS)
			return AW_ERROR;
		/* calculate cali step */
		g_func_haptic_nv->calculate_cali_data();
	}
	upload_lra(AW_F0_CALI_LRA);
	g_func_haptic_nv->play_stop();
#ifndef AW_F0_CALI_DURING_STARTUP
	haptic_nv_set_cali_to_flash();
#endif
	return ret;
}

static void f0_show(void)
{
	upload_lra(AW_WRITE_ZERO);
	g_func_haptic_nv->get_f0();
	upload_lra(AW_F0_CALI_LRA);
}

static void cali_show(void)
{
	upload_lra(AW_F0_CALI_LRA);
	g_func_haptic_nv->get_f0();
}

static int write_rtp_data(void)
{
	uint32_t buf_len = 0;
	uint32_t rtp_len = haptic_nv_rtp_len;
	uint32_t rtp_cnt = g_haptic_nv->rtp_cnt;
	uint32_t base_addr = g_haptic_nv->ram.base_addr;

	if (!rtp_len) {
		AW_LOGI("rtp_data is null");
		return AW_ERROR;
	}

#ifdef AW_ENABLE_RTP_PRINT_LOG
		AW_LOGI("rtp mode fifo update, cnt=%d", g_haptic_nv->rtp_cnt);
#endif

	if (rtp_cnt < base_addr) {
		if ((rtp_len - rtp_cnt) < base_addr)
			buf_len = rtp_len - rtp_cnt;
		else
			buf_len = base_addr;
	} else if ((rtp_len - rtp_cnt) < (base_addr >> 2)) {
		buf_len = rtp_len - rtp_cnt;
	} else {
		buf_len = base_addr >> 2;
	}

#ifdef AW_ENABLE_RTP_PRINT_LOG
		AW_LOGI("buf_len = %d", buf_len);
#endif

	g_func_haptic_nv->set_rtp_data(&(haptic_nv_rtp_data[g_haptic_nv->rtp_cnt]), buf_len);
	g_haptic_nv->rtp_cnt += buf_len;
	return AW_SUCCESS;
}

static int judge_rtp_load_end(void)
{
	uint8_t glb_st = 0;
	int ret = AW_ERROR;

	glb_st = g_func_haptic_nv->get_glb_state();

	if ((g_haptic_nv->rtp_cnt == haptic_nv_rtp_len) ||
	    ((glb_st & AW_BIT_GLBRD_STATE_MASK) == AW_BIT_STATE_STANDBY)) {
		if (g_haptic_nv->rtp_cnt != haptic_nv_rtp_len)
			AW_LOGE("rtp play suspend!");
		else
			AW_LOGI("rtp update complete!,cnt=%" PRId32 "", g_haptic_nv->rtp_cnt);
		g_haptic_nv->rtp_cnt = 0;
		g_haptic_nv->rtp_init = AW_FALSE;
		g_func_haptic_nv->set_rtp_aei(AW_FALSE);
		ret = AW_SUCCESS;
	}

	return ret;
}

static int rtp_going(void)
{
	int ret = 0;

	AW_LOGI("enter mode %d", g_haptic_nv->play_mode);

	g_haptic_nv->rtp_cnt = 0;

	while (!g_func_haptic_nv->rtp_get_fifo_afs() && (g_haptic_nv->play_mode == AW_RTP_MODE)) {
		ret = write_rtp_data();
		if (ret == AW_ERROR)
			return ret;
		ret = judge_rtp_load_end();
		if (ret == AW_SUCCESS)
			return ret;
	}

	if (g_haptic_nv->play_mode == AW_RTP_MODE)
		g_func_haptic_nv->set_rtp_aei(AW_TRUE);

	AW_LOGI("cnt = %" PRId32 ", exit", g_haptic_nv->rtp_cnt);
	return AW_SUCCESS;
}


#ifdef AW_IRQ_CONFIG
static void irq_handle(void)
{
	int ret = 0;
	int irq_state = 0;

	AW_LOGI("enter");
	g_haptic_nv->irq_handle = AW_IRQ_OFF;
	haptic_nv_disable_irq();
	do {
		irq_state = g_func_haptic_nv->get_irq_state();

		if (irq_state == AW_IRQ_ALMOST_EMPTY) {
			if (g_haptic_nv->rtp_init) {
				while ((!g_func_haptic_nv->rtp_get_fifo_afs()) && (g_haptic_nv->play_mode == AW_RTP_MODE)) {
					if (!g_haptic_nv->rtp_cnt) {
						AW_LOGI("g_haptic_nv->rtp_cnt is 0!");
						break;
					}

					ret = write_rtp_data();
					if (ret == AW_ERROR)
						break;
					ret = judge_rtp_load_end();
					if (ret == AW_SUCCESS)
						break;
				}
			} else {
				AW_LOGI("rtp_init: %d", g_haptic_nv->rtp_init);
			}
		}
		if (g_haptic_nv->play_mode != AW_RTP_MODE)
			g_func_haptic_nv->set_rtp_aei(AW_FALSE);
	} while(irq_state != AW_IRQ_NULL);
	AW_LOGI("exit");
	haptic_nv_enable_irq();
}

static void set_hw_irq_status(uint8_t aw_hw_irq_handle)
{
	g_haptic_nv->irq_handle = aw_hw_irq_handle;
}

static uint8_t get_hw_irq_status(void)
{
	return g_haptic_nv->irq_handle;
}
#endif

static int wait_enter_rtp_mode(int cnt)
{
	aw_bool rtp_work_flag = AW_FALSE;
	uint8_t ret = 0;

	while (cnt) {
		ret = g_func_haptic_nv->judge_rtp_going();
		if (ret == AW_SUCCESS) {
			rtp_work_flag = AW_TRUE;
			AW_LOGI("RTP_GO!");
			break;
		}
		cnt--;
		AW_LOGI("wait for RTP_GO, glb_state=0x%02X", ret);
		haptic_nv_mdelay(AW_RTP_DELAY);
	}

	if (!rtp_work_flag) {
		g_func_haptic_nv->play_stop();
		AW_LOGE("failed to enter RTP_GO status!");
		return AW_ERROR;
	}

	return AW_SUCCESS;
}

static void rtp_vib_work(uint8_t gain)
{
	int ret = 0;

	AW_LOGI("rtp file size = %" PRId32 "", haptic_nv_rtp_len);

	g_haptic_nv->rtp_init = AW_TRUE;
	g_func_haptic_nv->play_stop();
	g_func_haptic_nv->set_rtp_aei(AW_FALSE);
	g_func_haptic_nv->irq_clear();
	g_func_haptic_nv->set_gain(gain);
	g_func_haptic_nv->play_mode(AW_RTP_MODE);
#ifdef AW_RTP_AUTOSIN
	g_func_haptic_nv->set_rtp_autosin(AW_TRUE);
#else
	g_func_haptic_nv->set_rtp_autosin(AW_FALSE);
#endif
	upload_lra(AW_WRITE_ZERO);
	g_func_haptic_nv->haptic_start();
	haptic_nv_mdelay(AW_RTP_DELAY);
	ret = wait_enter_rtp_mode(200);
	if (ret == AW_ERROR)
		return;
	rtp_going();
}

static void get_ram_num(void)
{
	uint32_t first_wave_addr = 0;

	if (!g_haptic_nv->ram_init) {
		AW_LOGE("ram init failed, ram_num = 0!");
		return;
	}

	first_wave_addr = (g_haptic_nv->aw_fw.data[1] << 8) | g_haptic_nv->aw_fw.data[2];
	g_haptic_nv->ram.ram_num = (first_wave_addr - g_haptic_nv->ram.base_addr - 1) / 4;
	AW_LOGI("ram num = %d", g_haptic_nv->ram.ram_num);
}

static void ram_show(void)
{
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t size = 0;
	uint32_t print_cnt = 0;
	uint8_t ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};

	g_func_haptic_nv->play_stop();
	g_func_haptic_nv->ram_init(AW_TRUE);
	g_func_haptic_nv->set_ram_addr();
	AW_LOGD("aw_haptic_ram:\r\n");
	while (i < g_haptic_nv->ram.len) {
		if ((g_haptic_nv->ram.len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = g_haptic_nv->ram.len - i;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		g_func_haptic_nv->get_ram_data(ram_data, size);

		for (j = 0; j < size; j++) {
			AW_LOGD("0x%02X,", ram_data[j]);
			print_cnt++;
			if (print_cnt % 16 == 0) {
				AW_LOGD("\r\n");
			}
		}
		i += size;
	}
	g_func_haptic_nv->ram_init(AW_FALSE);
	AW_LOGD("\r\n");
}

static void sw_reset(void)
{
	uint8_t reset = AW_SOFT_RESET;

	AW_LOGI("enter!");
	haptic_nv_i2c_writes(AW_REG_ID, &reset, AW_I2C_BYTE_ONE);
	haptic_nv_mdelay(2);
}

static void hw_reset(void)
{
#ifdef	AW_RST_CONFIG
	haptic_nv_pin_control(g_haptic_nv->rst_pin, AW_PIN_LOW);
	haptic_nv_mdelay(2);
	haptic_nv_pin_control(g_haptic_nv->rst_pin, AW_PIN_HIGH);
	haptic_nv_mdelay(8);
#else
	AW_LOGI("no need rst pin!");
#endif
}


static int parse_chipid(void)
{
	uint8_t ef_id = 0;
	uint8_t cnt = 0;
	uint32_t reg = 0;
	int ret = AW_SUCCESS;

	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		ret = haptic_nv_read_chipid(&reg, AW_FIRST_TRY);
		if (ret != AW_SUCCESS) {
			g_haptic_nv->i2c_addr = AW862XX_I2C_ADDR;
			AW_LOGI("try to replace i2c addr [(0x%02X)] to read chip id again",
					g_haptic_nv->i2c_addr);
			ret = haptic_nv_read_chipid(&reg, AW_LAST_TRY);
			if (ret != AW_SUCCESS)
				break;
		}

		switch (reg) {
		case AW8623_CHIP_ID:
			g_haptic_nv->name = AW8623;
			AW_LOGI("detected aw8623.");
			return AW_SUCCESS;
		case AW8624_CHIP_ID:
			g_haptic_nv->name = AW8624;
			AW_LOGI("detected aw8624.");
			return AW_SUCCESS;
		case AW8622X_CHIP_ID:
			haptic_nv_i2c_reads(AW862XX_REG_EFRD9, &ef_id, AW_I2C_BYTE_ONE);
			if ((ef_id & 0x41) == AW86223_EF_ID) {
				g_haptic_nv->name = AW86223;
				AW_LOGI("aw86223 detected");
				return AW_SUCCESS;
			}
			if ((ef_id & 0x41) == AW86224_EF_ID) {
				g_haptic_nv->name = AW86224;
				AW_LOGI("aw86224 or aw86225 detected");
				return AW_SUCCESS;
			}
			AW_LOGI("unsupported ef_id = (0x%02X)", ef_id);
			break;
		case AW86214_CHIP_ID:
			haptic_nv_i2c_reads(AW862XX_REG_EFRD9, &ef_id, AW_I2C_BYTE_ONE);
			if ((ef_id & 0x41) == AW86214_EF_ID) {
				g_haptic_nv->name = AW86214;
				AW_LOGI("aw86214 detected");
				return AW_SUCCESS;
			}
			AW_LOGI("unsupported ef_id = (0x%02X)", ef_id);
			break;
		case AW86233_CHIP_ID:
			g_haptic_nv->name = AW86233;
			AW_LOGI("aw86233 detected");
			return 0;
		case AW86234_CHIP_ID:
			g_haptic_nv->name = AW86234;
			AW_LOGI("aw86234 detected");
			return 0;
		case AW86235_CHIP_ID:
			g_haptic_nv->name = AW86235;
			AW_LOGI("aw86235 detected");
			return 0;
		case AW86243_CHIP_ID:
			g_haptic_nv->name = AW86243;
			AW_LOGI("aw86243 detected");
			return 0;
		case AW86245_CHIP_ID:
			g_haptic_nv->name = AW86245;
			AW_LOGI("aw86245 detected");
			return 0;
		default:
			AW_LOGI("unsupport device revision (0x%02" PRIX32 ")", reg);
			break;
		}
		haptic_nv_mdelay(2);
	}
	return AW_ERROR;
}

static void haptic_init(void)
{
	g_haptic_nv->f0_pre = g_haptic_nv->info->f0_pre;
	g_func_haptic_nv->play_mode(AW_STANDBY_MODE);
	g_func_haptic_nv->misc_para_init();
	g_func_haptic_nv->vbat_mode_config(AW_CONT_VBAT_HW_COMP_MODE);
#ifdef AW_F0_CALI_DURING_STARTUP
	f0_cali();
#else
	haptic_nv_get_cali_from_flash();
#endif
}

static void write_ram_data(void)
{
	uint32_t i = 0;
	uint32_t len = 0;

	AW_LOGI("enter");
	g_func_haptic_nv->set_ram_addr();

	while (i < g_haptic_nv->aw_fw.len) {
		if ((g_haptic_nv->aw_fw.len - i) < AW_RAMDATA_WR_BUFFER_SIZE)
			len = g_haptic_nv->aw_fw.len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;

		g_func_haptic_nv->set_ram_data(&g_haptic_nv->aw_fw.data[i], len);
		i += len;
	}

}

#ifdef AW_CHECK_RAM_DATA
static int parse_ram_data(uint32_t len, uint8_t *cont_data, uint8_t *ram_data)
{
	uint32_t i = 0;

	for (i = 0; i < len; i++) {
		if (ram_data[i] != cont_data[i]) {
			AW_LOGE("check ramdata error, addr=0x%04" PRIX32 ", ram_data=0x%02X, file_data=0x%02X",
					i, ram_data[i], cont_data[i]);
			return AW_ERROR;
		}
	}

	return AW_SUCCESS;
}

static int check_ram_data(void)
{
	uint32_t i = 0;
	uint32_t len = 0;
	int ret = AW_SUCCESS;
	uint8_t ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};

	g_func_haptic_nv->set_ram_addr();
	while (i < g_haptic_nv->aw_fw.len) {
		if ((g_haptic_nv->aw_fw.len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = g_haptic_nv->aw_fw.len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		g_func_haptic_nv->get_ram_data(ram_data, len);
		ret = parse_ram_data(len, &g_haptic_nv->aw_fw.data[i], ram_data);
		if (ret == AW_ERROR)
			break;
		i += len;
	}
	return ret;
}
#endif

static int container_update(void)
{
	int ret = 0;

	g_func_haptic_nv->play_stop();
	g_func_haptic_nv->ram_init(AW_TRUE);
	g_func_haptic_nv->set_base_addr();
	g_func_haptic_nv->set_fifo_addr();
	g_func_haptic_nv->get_fifo_addr();
	write_ram_data();

#ifdef AW_CHECK_RAM_DATA
	ret = check_ram_data();
	if (ret)
		AW_LOGE("ram data check sum error");
	else
		AW_LOGI("ram data check sum pass");
#endif

	g_func_haptic_nv->ram_init(AW_FALSE);
	return ret;
}

static int ram_load(void)
{
	int ret = 0;
	AW_LOGI("ram load size: %" PRIX32 "", g_haptic_nv->aw_fw.len);
	ret = container_update();
	if (ret) {
		AW_LOGE("ram firmware update failed!");
	} else {
		g_haptic_nv->ram_init = AW_TRUE;
		g_haptic_nv->ram.len = g_haptic_nv->aw_fw.len;
		g_func_haptic_nv->trig_init();

		AW_LOGI("ram firmware update complete!");
		get_ram_num();
	}
	return AW_SUCCESS;
}

static int ram_init(void)
{
	int ret = AW_SUCCESS;
	g_haptic_nv->ram_init = AW_FALSE;
	g_haptic_nv->rtp_init = AW_FALSE;
	if (get_base_addr() != AW_SUCCESS) {
		AW_LOGE("base addr error, please check your ram data");
		return AW_ERROR;
	}
	ret = ram_load();

	return ret;
}

static int func_ptr_init(void)
{
	int ret = AW_SUCCESS;
	switch (g_haptic_nv->name) {
#ifdef AW862X_DRIVER
	case AW8623:
	case AW8624:
		g_func_haptic_nv = &aw862x_func_list;
		break;
#endif

#ifdef AW862XX_DRIVER
	case AW86214:
	case AW86223:
	case AW86224:
	case AW86225:
		g_func_haptic_nv = &aw862xx_func_list;
		break;
#endif

#ifdef AW8623X_DRIVER
	case AW86233:
	case AW86234:
	case AW86235:
		g_func_haptic_nv = &aw8623x_func_list;
		break;
#endif

#ifdef AW8624X_DRIVER
	case AW86243:
	case AW86245:
		g_func_haptic_nv = &aw8624x_func_list;
		break;
#endif

	default:
		AW_LOGE("unexpected chip!");
		ret = AW_ERROR;
		break;
	}
	if(g_func_haptic_nv == NULL) {
		AW_LOGE("g_func_haptic_nv is null!");
		ret = AW_ERROR;
	}
	return ret;
}

static int create_node(void)
{
	if (!g_func_haptic_nv)
			return AW_ERROR;
#ifdef AW_IRQ_CONFIG
	g_func_haptic_nv->irq_handle = irq_handle;
	g_func_haptic_nv->set_hw_irq_status = set_hw_irq_status;
	g_func_haptic_nv->get_hw_irq_status = get_hw_irq_status;
#endif

	g_func_haptic_nv->f0_cali = f0_cali;
	g_func_haptic_nv->f0_show = f0_show;
	g_func_haptic_nv->cali_show = cali_show;
	g_func_haptic_nv->rtp_going = rtp_going;
	g_func_haptic_nv->long_vib_work = long_vib_work;
	g_func_haptic_nv->short_vib_work = short_vib_work;
	g_func_haptic_nv->rtp_vib_work = rtp_vib_work;
	g_func_haptic_nv->get_ram_num = get_ram_num;
	g_func_haptic_nv->ram_show = ram_show;
#ifdef HAPTIC_NV_DOUBLE
	g_func_haptic_nv->dual_short_vib = haptic_nv_dual_short_vib_work;
	g_func_haptic_nv->dual_long_vib = haptic_nv_dual_long_vib_work;
#endif
	return AW_SUCCESS;
}

static void chip_private_init(void)
{
	switch (g_haptic_nv->name) {
#ifdef AW862X_DRIVER
	case AW8623:
	case AW8624:
		g_haptic_nv->info = &aw862x_dts;
		g_haptic_nv->aw_fw.data = aw862x_ram_data;
		g_haptic_nv->aw_fw.len = aw862x_ram_len;
		g_haptic_nv->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
		g_haptic_nv->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
		break;
#endif

#ifdef AW862XX_DRIVER
	case AW86214:
	case AW86223:
	case AW86224:
	case AW86225:
		g_haptic_nv->info = &aw8622x_dts;
		g_haptic_nv->aw_fw.data = aw862xx_ram_data;
		g_haptic_nv->aw_fw.len = aw862xx_ram_len;
		g_haptic_nv->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
		g_haptic_nv->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
		break;
#endif

#ifdef AW8623X_DRIVER
	case AW86233:
	case AW86234:
	case AW86235:
		g_haptic_nv->info = &aw8623x_dts;
		g_haptic_nv->aw_fw.data = aw862xx_ram_data;
		g_haptic_nv->aw_fw.len = aw862xx_ram_len;
		g_haptic_nv->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
		g_haptic_nv->ram_vbat_comp = AW_RAM_VBAT_COMP_DISABLE;
		break;
#endif

#ifdef AW8624X_DRIVER
	case AW86243:
	case AW86245:
		g_haptic_nv->info = &aw8624x_dts;
		g_haptic_nv->aw_fw.data = aw862xx_ram_data;
		g_haptic_nv->aw_fw.len = aw862xx_ram_len;
		g_haptic_nv->trim_lra_boundary = AW8624X_TRIM_LRA_BOUNDARY;
		g_haptic_nv->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
		break;
#endif
	default:
		break;
	}
}

#ifdef AW_IRQ_CONFIG
static void irq_config(void)
{
	g_haptic_nv->is_used_irq_pin = AW_TRUE;
	g_func_haptic_nv->interrupt_setup();
}
#endif

int haptic_nv_boot_init(void)
{
	int ret = AW_SUCCESS;

	AW_LOGI("haptic_nv driver version %s", HAPTIC_NV_DRIVER_VERSION);

	hw_reset();
	ret = parse_chipid();
	if (ret != AW_SUCCESS) {
		AW_LOGE("read chip id failed!");
		return ret;
	}
	chip_private_init();
	ret = func_ptr_init();
	if (ret != AW_SUCCESS) {
		AW_LOGE("ctrl_init failed");
		return ret;
	}
	ret = g_func_haptic_nv->check_qualify();
	if (ret != AW_SUCCESS) {
		AW_LOGE("qualify check failed.");
		return ret;
	}
	sw_reset();
	ret = g_func_haptic_nv->offset_cali();
	if (ret != 0)
		sw_reset();

#ifdef AW_IRQ_CONFIG
	irq_config();
#endif

	haptic_init();
	ret = ram_init();
	if (ret != AW_SUCCESS) {
		AW_LOGE("ram init err!!!");
		return ret;
	}
	create_node();

	return ret;
}

void haptic_nv_play_start(void)
{
	g_func_haptic_nv->play_mode(AW_CONT_MODE);
	g_func_haptic_nv->haptic_start();
}

void haptic_nv_play_stop(void)
{
	g_func_haptic_nv->play_stop();
}