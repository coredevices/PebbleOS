#include "comm/ble/gap_le_advert.h"

#include <bluetooth/adv_reconnect.h>

const GAPLEAdvertisingJobTerm *bt_driver_adv_reconnect_get_job_terms(size_t *num_terms_out) {
  *num_terms_out = 0;
  return NULL;
}
