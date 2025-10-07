//! @file

#include "kernel/kernel_heap.h"
#include "memfault/components.h"
#include "drivers/imu/lsm6dso/lsm6dso.h"
#include "services/common/battery/battery_state.h"
#include "util/heap.h"

int memfault_platform_get_stateofcharge(sMfltPlatformBatterySoc *soc) {
  BatteryChargeState chargestate = battery_get_charge_state();

  *soc = (sMfltPlatformBatterySoc){
      .soc = chargestate.charge_percent,
      .discharging = !chargestate.is_charging,
  };

  return 0;
}

// Record some metrics.
void memfault_metrics_heartbeat_collect_data(void) {
  // battery_state_get_voltage() actually returns the voltage in millivolts,
  // which is the unit for the battery_v metric as recorded on device.
  MEMFAULT_METRIC_SET_UNSIGNED(battery_v, battery_state_get_voltage());

  // Kernel heap usage
  Heap *kernel_heap = kernel_heap_get();
  const uint32_t kernel_heap_size = heap_size(kernel_heap);
  const uint32_t kernel_heap_max_used = kernel_heap->high_water_mark;
  // kernel_heap_pct is a percentage with 2 decimal places of precision
  // (i.e. 10000 = 100.00%)
  const uint32_t kernel_heap_pct = (kernel_heap_max_used * 10000) / kernel_heap_size;

  MEMFAULT_LOG_INFO("Heap Usage: %lu/%lu (%lu.%02lu%%)\n", kernel_heap_max_used, kernel_heap_size,
                    kernel_heap_pct / 100, kernel_heap_pct % 100);

  MEMFAULT_METRIC_SET_UNSIGNED(memory_pct_max, kernel_heap_pct);

  // Capture accelerometer diagnostics to catch LSM6DSO stuck states.
  Lsm6dsoDiagnostics accel_diag;
  lsm6dso_get_diagnostics(&accel_diag);
  MEMFAULT_METRIC_SET_SIGNED(accel_lsm6dso_last_x_mg, accel_diag.last_sample_mg[0]);
  MEMFAULT_METRIC_SET_SIGNED(accel_lsm6dso_last_y_mg, accel_diag.last_sample_mg[1]);
  MEMFAULT_METRIC_SET_SIGNED(accel_lsm6dso_last_z_mg, accel_diag.last_sample_mg[2]);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_sample_age_ms, accel_diag.last_sample_age_ms);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_read_age_ms, accel_diag.last_successful_read_age_ms);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_interrupt_age_ms, accel_diag.last_interrupt_age_ms);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_wake_age_ms, accel_diag.last_wake_event_age_ms);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_double_tap_age_ms, accel_diag.last_double_tap_age_ms);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_state_flags, accel_diag.state_flags);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_i2c_error_count, accel_diag.i2c_error_count);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_consecutive_errors, accel_diag.consecutive_error_count);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_interrupt_count, accel_diag.interrupt_count);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_wake_events, accel_diag.wake_event_count);
  MEMFAULT_METRIC_SET_UNSIGNED(accel_lsm6dso_double_tap_events,
                               accel_diag.double_tap_event_count);

  extern uint32_t metric_firm_425_back_button_long_presses_cancelled;
  MEMFAULT_METRIC_SET_UNSIGNED(firm_425_back_button_long_presses_cancelled, metric_firm_425_back_button_long_presses_cancelled);
}
