#include "drivers/watchdog.h"

#include <nrfx.h>
#include <helpers/nrfx_reset_reason.h>

void watchdog_init(void) {
	NRF_WDT->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;
	/* WDT expiration: 4s */
	NRF_WDT->CRV = 32768 * 4;
}

void watchdog_start(void) {
	NRF_WDT->TASKS_START = 1;
}

bool watchdog_check_clear_reset_flag(void) {
	uint32_t reason = nrfx_reset_reason_get();
	nrfx_reset_reason_clear(0xFFFFFFFF);
	return (reason & NRFX_RESET_REASON_DOG_MASK) != 0;
}
