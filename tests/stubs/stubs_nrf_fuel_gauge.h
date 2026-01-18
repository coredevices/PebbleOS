#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Types and enums from nrf_fuel_gauge.h
typedef enum {
  NRF_FUEL_GAUGE_CHARGE_STATE_COMPLETE,
  NRF_FUEL_GAUGE_CHARGE_STATE_TRICKLE,
  NRF_FUEL_GAUGE_CHARGE_STATE_CC,
  NRF_FUEL_GAUGE_CHARGE_STATE_CV,
  NRF_FUEL_GAUGE_CHARGE_STATE_IDLE,
} nrf_fuel_gauge_charge_state_t;

typedef enum {
  NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_STATE_CHANGE,
  NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_CONNECTED,
  NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_DISCONNECTED,
  NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_CURRENT_LIMIT,
  NRF_FUEL_GAUGE_EXT_STATE_INFO_TERM_CURRENT,
} nrf_fuel_gauge_ext_state_info_type_t;

typedef union nrf_fuel_gauge_ext_state_info_data {
  nrf_fuel_gauge_charge_state_t charge_state;
  float charge_current_limit;
  float charge_term_current;
} nrf_fuel_gauge_ext_state_info_data_t;

struct battery_model {
  // Stub battery model - just needs to be a complete type
  int dummy;
};

struct nrf_fuel_gauge_init_parameters {
  const struct battery_model *model;
  float v0;
  float i0;
  float t0;
};

struct nrf_fuel_gauge_runtime_parameters {
  float a;
  float b;
  float c;
  float d;
  bool discard_positive_deltaz;
};

// Stub for NPM1300_CONFIG
#define NPM1300_CONFIG ((struct nrf_fuel_gauge_stub_config){ \
  .chg_current_ma = 100, \
  .term_current_pct = 10, \
})

struct nrf_fuel_gauge_stub_config {
  int chg_current_ma;
  int term_current_pct;
};

// isnanf stub
#define isnanf(x) isnan(x)

// Stub functions
static inline int nrf_fuel_gauge_init(const struct nrf_fuel_gauge_init_parameters *parameters, void *unused) {
  (void)parameters;
  (void)unused;
  return 0;
}

static inline int nrf_fuel_gauge_ext_state_update(nrf_fuel_gauge_ext_state_info_type_t type,
                                     const nrf_fuel_gauge_ext_state_info_data_t *data) {
  (void)type;
  (void)data;
  return 0;
}

static inline float nrf_fuel_gauge_process(float v, float i, float t, float delta, void *unused) {
  (void)v;
  (void)i;
  (void)t;
  (void)delta;
  (void)unused;
  return 100.0f;  // Return 100% battery as stub
}

static inline float nrf_fuel_gauge_tte_get(void) {
  return 0.0f;
}

static inline float nrf_fuel_gauge_ttf_get(void) {
  return 0.0f;
}

static inline void nrf_fuel_gauge_param_adjust(const struct nrf_fuel_gauge_runtime_parameters *params) {
  (void)params;
}

#define NAN_F NAN

#ifdef __cplusplus
}
#endif
