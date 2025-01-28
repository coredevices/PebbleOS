#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "ble_advdata.h"

#include "comm/bt_lock.h"
#include "drivers/qemu/qemu_serial.h"
#include "drivers/qemu/qemu_settings.h"
#include "kernel/event_loop.h"
#include "pebble_errors.h"
#include "system/logging.h"
#include "system/passert.h"
#include "board/board.h"

#include <bluetooth/init.h>
#include <bluetooth/qemu_transport.h>

#include <stdlib.h>

#define CONN_TAG 1

static bool s_callback_pending = false;
static void prv_sdh_evts_poll_cb(void *ctx) {
  s_callback_pending = false;
  nrf_sdh_evts_poll();
}

void SD_EVT_IRQHandler(void)
{
  BaseType_t yield_req = false;

  if (!s_callback_pending) {
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_sdh_evts_poll_cb,
        .data = NULL
      }
    };
    yield_req |= event_put_isr(&e);
    s_callback_pending = true;
  }

  portEND_SWITCHING_ISR(yield_req);
}
IRQ_MAP_NRFX(SWI2_EGU2, SD_EVT_IRQHandler);

// ----------------------------------------------------------------------------------------
void bt_driver_init(void) {
  ret_code_t rv;

  bt_lock_init();
  
  /* the softdevice was enabled by early init so the LFCLK / RTC will work */
  PBL_LOG(LOG_LEVEL_INFO, "nRF52: enabling BLE");
  
  uint32_t ram_start = 0;
  extern uint8_t __KERNEL_RAM_start__;
  uint32_t pebbleos_ram_start = (uint32_t) &__KERNEL_RAM_start__;
  
  rv = nrf_sdh_ble_default_cfg_set(CONN_TAG, &ram_start);
  PBL_ASSERTN(rv == NRF_SUCCESS);
  PBL_ASSERTN(ram_start <= pebbleos_ram_start);
  
  rv = nrf_sdh_ble_enable(&pebbleos_ram_start);
  PBL_ASSERTN(rv == NRF_SUCCESS);
  PBL_LOG(LOG_LEVEL_INFO, "nRF52: BLE stack enabled");
}

static uint8_t _adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t _advdata_buf[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint8_t _srdata_buf[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static ble_gap_adv_data_t _advdata = {
  .adv_data = { .p_data = _advdata_buf, .len = sizeof(_advdata_buf) },
  .scan_rsp_data = { .p_data = _srdata_buf, .len = sizeof(_srdata_buf) },
};

bool bt_driver_start(BTDriverConfig *config) {
  PBL_LOG(LOG_LEVEL_INFO, "nRF52: BLE dummy driver start");
  static uint8_t _name_buf[24] = "Asterix softdevice";
  
  ble_gap_conn_sec_mode_t sec_mode;
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
  sd_ble_gap_device_name_set(&sec_mode, _name_buf, strlen((char *)_name_buf));
  
  sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_WATCH);
  
  ble_advdata_t advdata;
  ble_advdata_t srdata;
  ret_code_t rv;
  
  ble_gap_adv_params_t advparams;
  
  memset(&advdata, 0, sizeof(advdata));
  _advdata.adv_data.len = sizeof(_advdata_buf);
  advdata.name_type = BLE_ADVDATA_FULL_NAME;
  advdata.include_appearance = 1;
  advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  rv = ble_advdata_encode(&advdata, _advdata.adv_data.p_data, &_advdata.adv_data.len);
  PBL_ASSERTN(rv == NRF_SUCCESS);
  
  memset(&srdata, 0, sizeof(srdata));
  _advdata.scan_rsp_data.len = sizeof(_srdata_buf);
  rv = ble_advdata_encode(&srdata, _advdata.scan_rsp_data.p_data, &_advdata.scan_rsp_data.len);
  PBL_ASSERTN(rv == NRF_SUCCESS);
  
  memset(&advparams, 0, sizeof(advparams));
  advparams.primary_phy = BLE_GAP_PHY_1MBPS;
  advparams.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
  advparams.properties.type = /* vis ? */ BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED /*: BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED */;
  advparams.p_peer_addr = NULL;
  advparams.filter_policy = /* vis ? */ BLE_GAP_ADV_FP_ANY /* : BLE_GAP_ADV_FP_FILTER_BOTH */; /* XXX: Later, whitelist things that can try to connect. */
  advparams.interval = 64;
  rv = sd_ble_gap_adv_set_configure(&_adv_handle, &_advdata, &advparams);
  PBL_ASSERTN(rv == NRF_SUCCESS);
  
  rv = sd_ble_gap_adv_start(_adv_handle, CONN_TAG);
  PBL_ASSERTN(rv == NRF_SUCCESS);
  
  prv_sdh_evts_poll_cb(NULL);
  
  return true;
}

void bt_driver_stop(void) {
}

void bt_driver_power_down_controller_on_boot(void) {
  // no-op
}
