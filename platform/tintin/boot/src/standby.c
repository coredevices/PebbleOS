/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"

#include "drivers/button.h"
#include "drivers/otp.h"
#include "drivers/rtc.h"
#include "drivers/periph_config.h"
#include "drivers/dbgserial.h"

#include "system/reset.h"

static bool prv_is_wake_on_usb_supported(void) {
  // we accidentally left off a pull-up on early BB2s and v1_5 boards
  //   with the upshot of not being able to support wake from standby on VUSB
  if (BOARD_CONFIG_POWER.wake_on_usb_power) {
    if (!otp_is_locked(OTP_HWVER)) {
      dbgserial_putstr("No HW Version in OTP");
      // let's be optimistic
      return true;
    }

    const char* hw_ver = otp_get_slot(OTP_HWVER);
#if defined(BOARD_BB2)
    // We fixed the issue for BB2.1 (900-0-22-02-R1)
    const char* no_support_hw_ver = "BB2.0";
    return (memcmp(hw_ver, no_support_hw_ver, strlen(no_support_hw_ver)) != 0);
#elif defined(BOARD_V1_5)
    // We fixed the issue for V3R2 (101-0-22-10-R3)
    const char* no_support_hw_ver = "V3R1";
    return (memcmp(hw_ver, no_support_hw_ver, strlen(no_support_hw_ver)) != 0);
#else
    return true;
#endif
  } else {
    return false;
  }
}

static void prv_wait_until_buttons_are_released(void) {
  for (int bounce_count = 0; bounce_count < 10; ++bounce_count) {
    uint8_t button_state;

    // First, see if the buttons are all released for a period of time.
    for (int i = 0; i < 10000; ++i) {
      button_state = button_get_state_bits();
      if (button_state != 0) {
        // Someone pushed a button!
        break;
      }
    }

    if (button_state == 0) {
      // We made it through with all the buttons released. We're good.
      return;
    }

    // Alright, so either the button is held down or we hit a bounce. Wait
    // for all the buttons to release again.
    // 100000 is about a second in practice
    for (int i = 0; i < 100000; ++i) {
      if (button_get_state_bits() == 0) {
        // All the buttons are released!
        break;
      }
    }
  }
}

static void prv_clear_wakeup_flags(void) {
  // This function follows the steps listed in Erratum 2.1.4 "Wakeup sequence from Standby mode..."
  // to avoid a situation where the watch cannot wake up
  // or immediately wakes up after going into standby.

  // The erratum says all used wakeup sources need to be disabled before
  // reenabling the required ones, so to be safe we disable all wakeup sources
  // to avoid dependence on knowing which wakeup sources the firmware left set
  // Possible wakeup sources taken from reference manual 4.3.5 "Exiting Standby Mode"

  // Disable the Wakeup pin
  PWR_WakeUpPinCmd(DISABLE);

  // Clear RTC interrupts, this ensures the flags won't be reset after we clear them
  RTC_ITConfig(RTC_IT_TAMP
               | RTC_IT_TS
               | RTC_IT_WUT
               | RTC_IT_ALRA
               | RTC_IT_ALRB, DISABLE);
  // Clear all RTC wakeup flags
  RTC_ClearFlag(RTC_FLAG_TAMP1F
                | RTC_FLAG_TSF
                | RTC_FLAG_WUTF
                | RTC_FLAG_ALRBF
                | RTC_FLAG_ALRAF);

  // At this point we know the wakeup flags are cleared so we can clear the PWR wakeup flag
  PWR->CR |= PWR_CR_CWUF;
}

static void prv_enable_wake_on_usb(void) {
  // Use the RTC timestamp alternate function to trigger a wakeup from the VUSB interrupt
  // We don't clear all the wakeup flags here as said in
  // 4.3.6 "Safe RTC alternate function wakeup flag clearing sequence", because
  // prv_clear_wakeup_flags already cleared them for use by multiple wakeup sources
  RTC_TimeStampPinSelection(RTC_TimeStampPin_PC13);
  RTC_TimeStampCmd(RTC_TimeStampEdge_Falling, ENABLE);
  RTC_ITConfig(RTC_IT_TS, ENABLE);
}

void enter_standby_mode(void) {
  rtc_slow_down();

  // Set wakeup events for the board
  // If the WKUP pin is high when we enable wakeup, an additional
  // wakeup event is registered (4.4.2 "PWR power control/status register"),
  // which will cause the board to wake up immediately after entering standby. Therefore we
  // wait until the button is released (or too much time has passed).
  // It is possible to work around needing this by enabling the WKUP pin before
  // clearing the PWR WUF flag, but that risks running afoul of errata 2.1.4
  prv_wait_until_buttons_are_released();

  prv_clear_wakeup_flags();

  PWR_WakeUpPinCmd(ENABLE);

  if (prv_is_wake_on_usb_supported()) {
    dbgserial_putstr("usb wakeup supported");
    prv_enable_wake_on_usb();
  }

  // Put the board into standby mode. The standard peripheral library provides PWR_EnterSTANDBYMode
  // to do this, but that function clears the WUF (wakeup) flag. According to errata 2.1.4 if the
  // wakeup flag is cleared when any wakeup source is high, further wakeup events may be masked.
  // This means if a button press or usb plugin was to occur in between enabling the wakeup events
  // and clearing the flag, the watch wouldn't wake up.
  dbgserial_putstr("Entering standby");

  // Steps to enter standby follow 4.3.5 "Entering Standby mode" Table 11
  // (except where they conflict with errata 2.1.4)

  // Select STANDBY mode
  PWR->CR |= PWR_CR_PDDS;

  // Set SLEEPDEEP bit on the cortex system control register
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  // Wait for interrupt
  __WFI();
}

bool should_leave_standby_mode(void) {
  if (RTC_GetFlagStatus(RTC_FLAG_TSF)) {
    // we were woken by the USB power being plugged in
    dbgserial_putstr("USB wakeup");
    return true;
  }

  // Make sure a button is held down before waking up
  for (int i = 0; i < 100000; ++i) {
    if (button_get_state_bits() == 0) {
      // stop waiting if not held down any longer and go back to sleep
      return false;
    }
  }

  return true;
}

void leave_standby_mode(void) {
  // Speed up the RTC so the firmware doesn't need to deal with it
  rtc_speed_up();
}
