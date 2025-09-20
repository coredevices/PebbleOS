#include "drivers/watchdog.h"

#include <helpers/nrfx_reset_reason.h>
#include <nrfx.h>

#define WDT_INTERVAL_SECONDS 8

/* On nRF52840, the watchdog can only be disabled by a power-on reset.  We
 * don't really want the watchdog to be running during the bootloader: if
 * the bootloader hangs, there is precious little we can do about it, and we
 * don't want the watchdog to interrupt long-running operations like erasing
 * microflash, or reading / writing QSPI flash.
 *
 * The upshot of this is that, even if we are running on a no-watchdog
 * build, we must continually kick the watchdog, lest it bite, since the
 * watchdog could have been configured from the previous boot!
 */

void watchdog_init(void) {
  /* Allow us to be debugged, but keep the WDT ticking when the CPU is
   * asleep for normal reasons.  This is the reset value, as well, but it's
   * always good to be sure before we do anything that we can't take back.
   */
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) |
                    (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);
  NRF_WDT->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;
  /* WDT expiration: 8s */
  NRF_WDT->CRV = 32768 * WDT_INTERVAL_SECONDS;
  NRF_WDT->TASKS_START = 1;
  // NOTE: at this point WDT can no longer be stopped, it will even survive
  // a system reset!
}

void watchdog_kick(void) {
  if (NRF_WDT->RUNSTATUS) {
    // In theory, only RR0 should be enabled.  But in case someone else has
    // enabled other RRs out from under us, we had better kick all of them.
    for (int i = 0; i < 8; i++) {
      if (NRF_WDT->RREN & (1 << i)) {
        NRF_WDT->RR[i] = WDT_RR_RR_Reload;
      }
    }
  }
}

bool watchdog_check_clear_reset_flag(void) {
  uint32_t reason = nrfx_reset_reason_get();
  nrfx_reset_reason_clear(0xFFFFFFFF);
  return (reason & NRFX_RESET_REASON_DOG_MASK) != 0;
}
