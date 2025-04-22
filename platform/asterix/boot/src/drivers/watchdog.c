#include "drivers/watchdog.h"

#include <nrfx.h>
#include <helpers/nrfx_reset_reason.h>

void watchdog_init(void) {
	NRF_WDT->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;
	/* WDT expiration: 8s */
	NRF_WDT->CRV = 32768 * 8;
	NRF_WDT->TASKS_START = 1;
	// NOTE: at this point WDT can no longer be stopped, it will even survive
	// a system reset!
}

void watchdog_kick(void) {
	if (NRF_WDT->CRV != WDT_CRV_CRV_Msk) {
		NRF_WDT->RR[0] = WDT_RR_RR_Reload;
	}
}

bool watchdog_check_clear_reset_flag(void) {
	uint32_t reason = nrfx_reset_reason_get();
	nrfx_reset_reason_clear(0xFFFFFFFF);
	return (reason & NRFX_RESET_REASON_DOG_MASK) != 0;
}
