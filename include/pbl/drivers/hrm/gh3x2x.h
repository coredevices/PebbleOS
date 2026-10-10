/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdbool.h>

#include <pbl/drivers/exti.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>

#include <applib/app_timer.h>

/**
 * @defgroup drivers_hrm_gh3x2x GH3x2x
 * @ingroup drivers_hrm
 * @brief @ref drivers_hrm implementation for the Goodix GH3x2x sensors.
 *
 * Also exposes the hooks used by the vendor library and the manufacturing tests.
 * @{
 */

#ifdef CONFIG_BOARD_OBELIX
// FIXME(OBELIX): Provide proper GPIO layer abstraction
/** @brief The sensor reset line is driven by an nPM1300 GPIO instead of an MCU pin. */
#define GH3X2X_RESET_PIN_CTRLBY_NPM1300 1
#endif

/** @brief Number of PPG channels. */
#define HRM_PPG_CH_NUM 6
/** @brief Samples per channel averaged by the factory tests. */
#define HRM_PPG_FACTORY_TEST_FIFO_LEN 80

/** @cond INTERNAL_HIDDEN */
typedef struct {
  double result[HRM_PPG_CH_NUM];
  uint16_t test_mode;
  uint16_t drop_count;
  uint16_t wpos;
  uint16_t count;
  uint32_t *ppg_array[HRM_PPG_CH_NUM];
} GH3x2xFTData;

typedef struct HRMDeviceState {
  bool enabled;
  bool is_wear;
  bool spo2_assumed_wear; // is_wear is the SpO2 path's assumed-worn value, not an ADT verdict
  bool wear_before_spo2;  // ADT verdict to restore once the green path runs again
  int32_t work_mode;
  uint16_t timer_period_ms;
  AppTimer *timer;
  GH3x2xFTData *factory;
  bool initialized;
} HRMDeviceState;
/** @endcond */

/** @brief GH3x2x device. */
typedef const struct HRMDevice {
  /** Driver runtime state. */
  HRMDeviceState *state;
  /** Sensor I2C port. */
  I2CSlavePort *i2c;
  /** Sensor interrupt line. */
  ExtiConfig int_exti;
  /** Sensor interrupt line, as an input. */
  struct pbl_gpio int_input;
} HRMDevice;

/**
 * @brief Queue tuning protocol data received over BLE for processing on the system task.
 *
 * Only with @c CONFIG_BT_GH3X2X_TUNING_SERVICE.
 *
 * @param context Heap buffer holding a 32-bit length followed by the data; freed once processed.
 * @return True if queued.
 */
bool gh3x2x_ble_data_recv(void *context);

/**
 * @brief Report the wear detection state.
 *
 * @param is_wear True if the watch is worn.
 */
void gh3x2x_wear_evt_notify(bool is_wear);

/**
 * @brief Feed raw PPG data to the factory tests.
 *
 * Does nothing unless a factory test is running (MFG builds only).
 *
 * @param p_rawdata Raw samples, interleaved over #HRM_PPG_CH_NUM channels.
 * @param data_count Number of values in @p p_rawdata.
 */
void gh3x2x_rawdata_notify(uint32_t *p_rawdata, uint32_t data_count);

void gh3x2x_wear_evt_notify(bool is_wear);

/** @brief Factory test type. */
typedef enum {
  /** No test. */
  HRM_FACTORY_TEST_NONE,
  /** CTR (LED-to-sensor coupling) test. */
  HRM_FACTORY_TEST_CTR,
  /** Light leakage test. */
  HRM_FACTORY_TEST_LIGHT_LEAK,
  /** Noise test. */
  HRM_FACTORY_TEST_HSM,
} GH3x2xFTType;

/**
 * @brief Start the CTR factory test on the system task.
 *
 * Results are reported through the HRM manager as @c HRMFeature_CTR data. MFG builds only.
 */
void gh3x2x_start_ft_ctr(void);

/**
 * @brief Start the light leakage factory test on the system task.
 *
 * Results are reported through the HRM manager as @c HRMFeature_Leakage data. MFG builds only.
 */
void gh3x2x_start_ft_leakage(void);

/** @brief Stop the running factory test on the system task. MFG builds only. */
void gh3x2x_factory_test_disable();

/**
 * @brief Set the sampling functions used when the sensor is next enabled.
 *
 * Soft wear detection is always added. MFG builds only.
 *
 * @param mode Bitmask of @c GH3X2X_FUNCTION_* values.
 */
void gh3x2x_set_work_mode(int32_t mode);

/** @} */
