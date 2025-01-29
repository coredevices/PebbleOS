#pragma once
#if PEBBLE_APP
#include <pebble_worker.h>
#endif
#include "constants_worker.h"
#include "helper_worker.h"
#include "fourier.h"

// #include <math.h>
typedef signed long long sfxp;
typedef unsigned long long ufxp;
#define int2sll(X)	(((sfxp) (X)) << 32)
#define sll2int(X)	((int32_t) ((X) >> 32))

// scaling constants
 // 13.5*10, convert the raw pim cpm to the actigraph vmcpm
// used for both VMCPM and CPM (cause a linear relation)
// static const  uint32_t x100_RAW_1G_PIM_CPM_TO_REAL_CPM = 1863;
static const  uint32_t x100_RAW_1G_PIM_CPM_TO_REAL_CPM = 2408;

int32_t mean_l1_stat(int16_t *d, int16_t dlen);

uint32_t pim_filt(int16_t *d, int16_t dlen, int16_t axis);

uint32_t calc_scaled_vmc(uint32_t *pim_ary);

uint8_t compressed_vmc(uint32_t *pim_ary);

uint32_t calc_real_vmc(uint32_t *pim_ary);

uint32_t calc_real_c(uint32_t pim);

uint32_t calc_x1000_kcal(uint32_t *pim_ary);

uint8_t compressed_x1000_kcal(uint32_t x1000_kcal, int16_t num_min);

uint8_t compressed_stepc(uint32_t stepc, int16_t num_min);

void vm_accel(int16_t **d, int16_t *w, int16_t max_vm, int16_t dlen);

void vm_accel_xy(int16_t **d, int16_t *w, int16_t max_vm, int16_t dlen);

int16_t score_fft_alg_0pad(int16_t *d, int16_t dlen_smp, int16_t dlenpwr_ary, int16_t oflw_scl);

void get_fftmag_0pad(int16_t *d, int16_t dlen_smp, int16_t dlenpwr_ary, int16_t oflw_scl);

void get_fftmag_0pad_mean0(int16_t *d, int16_t dlen_smp, int16_t dlenpwr_ary, int16_t oflw_scl);

uint16_t score_fftmag_hz_rng(int16_t *d, int16_t dlenpwr_ary,int16_t lhz_i,int16_t hhz_i);

uint16_t score_fftmag_hz_rng_l2(int16_t *d, int16_t dlenpwr_ary,int16_t lhz_i,int16_t hhz_i);

void filt_hann_win_mean0(int16_t *d, int16_t dlen_smp, int32_t g_fctr);

int16_t max_mag_hz_0pad(int16_t *d);

int16_t calc_stepc_5sec(int16_t *work_ary, int16_t dlen_smp, int16_t dlenpwr_ary,
  uint32_t *pim_5sec_ary, int16_t fft_oflw_scl);

// int16_t stepc_fftalg_0pad(int16_t *d, int16_t dlen_smp, int16_t dlenpwr_ary, int16_t oflw_scl);

void pim_filt_init(void);

void pim_filt_prime(int16_t *d, int16_t dlen, int16_t axis);
