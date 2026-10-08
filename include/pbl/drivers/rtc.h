/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/services/time.h>

/**
 * @defgroup drivers_rtc RTC
 * @ingroup drivers
 * @brief Real-time clock: wall clock time, tick counter, wake-up alarm and timezone storage.
 *
 * Wall clock time is UTC, limited to years 2000 to 2037. Ticks count at @ref RTC_TICKS_HZ
 * since system start.
 *
 * @code{.c}
 * char buf[TIME_STRING_BUFFER_SIZE];
 * time_t now = rtc_get_time();
 *
 * PBL_LOG_DBG("%s", time_t_to_string(buf, now));
 *
 * RtcTicks start = rtc_get_ticks();
 * do_work();
 * uint32_t elapsed_ms = (rtc_get_ticks() - start) * 1000 / RTC_TICKS_HZ;
 * @endcode
 * @{
 */

/** @brief RTC tick count. */
typedef uint64_t RtcTicks;

#if defined(CONFIG_QEMU)
/** @brief Tick frequency in Hz (QEMU 1000 Hz counter). */
#define RTC_TICKS_HZ (1000u)
#elif defined(CONFIG_SOC_POSIX)
/** @brief Tick frequency in Hz (host monotonic clock). */
#define RTC_TICKS_HZ (1000u)
#elif defined(CONFIG_SOC_SF32LB52)
/** @brief Tick frequency in Hz (SF32LB52 LPTIM clocked by RC10K). */
#define RTC_TICKS_HZ (1000u)
#else
/** @brief Tick frequency in Hz. */
#define RTC_TICKS_HZ (1024u)
#endif

/**
 * @brief Initialize the RTC.
 *
 * Runs very early during startup, when few other systems are available.
 */
void rtc_init(void);

/**
 * @brief Calibrate the RTC.
 *
 * Separate from rtc_init() because the manufacturing registry is not available that early.
 *
 * @param frequency Measured crystal frequency in mHz.
 */
void rtc_calibrate_frequency(uint32_t frequency);

/**
 * @brief Start the timers the RTC driver needs.
 *
 * Separate from rtc_init() because the timer system is not available that early.
 */
void rtc_init_timers(void);

/**
 * @brief Clamp the year of a broken-down time to 2000-2037.
 *
 * Other fields are left untouched.
 *
 * @param[in,out] t Time to clamp.
 * @return true if @p t was changed.
 */
bool rtc_sanitize_struct_tm(struct pbl_tm *t);

/**
 * @brief Clamp the year of a time to 2000-2037, as rtc_sanitize_struct_tm().
 *
 * @param[in,out] t Time to clamp.
 * @return true if @p t was changed.
 */
bool rtc_sanitize_time_t(time_t *t);

/**
 * @brief Set the current time.
 *
 * Times outside the years 2000 to 2037 are not supported.
 *
 * @param time UTC time in seconds since the epoch.
 */
void rtc_set_time(time_t time);

/**
 * @brief Get the current time.
 *
 * @return UTC time in seconds since the epoch.
 */
time_t rtc_get_time(void);

/**
 * @brief Set the current time from a broken-down time.
 *
 * @param time_tm Time to set.
 */
void rtc_set_time_tm(struct pbl_tm *time_tm);

/**
 * @brief Get the current time as a broken-down local time.
 *
 * @param[out] time_tm Current time.
 */
void rtc_get_time_tm(struct pbl_tm *time_tm);

/**
 * @brief Get the current time with millisecond resolution.
 *
 * @param[out] out_seconds Current time in seconds.
 * @param[out] out_ms Millisecond part of the current second.
 */
void rtc_get_time_ms(time_t *out_seconds, uint16_t *out_ms);

/**
 * @brief Store the timezone in storage that survives reboots.
 *
 * @param tzinfo Timezone.
 */
void rtc_set_timezone(TimezoneInfo *tzinfo);

/**
 * @brief Load the stored timezone.
 *
 * @param[out] tzinfo Timezone.
 */
void rtc_get_timezone(TimezoneInfo *tzinfo);

/**
 * @brief Get the stored timezone region ID.
 *
 * @return Timezone region (Olson) index.
 */
uint16_t rtc_get_timezone_id(void);

/**
 * @brief Check whether a timezone is stored.
 *
 * Without one, the RTC runs in UTC.
 *
 * @return true if a timezone is set.
 */
bool rtc_is_timezone_set(void);

/** @brief Size of the buffers passed to rtc_get_time_string() and time_t_to_string(). */
#define TIME_STRING_BUFFER_SIZE 26

/**
 * @brief Format the current local time.
 *
 * @param[out] buffer Buffer of at least @ref TIME_STRING_BUFFER_SIZE bytes.
 * @return @p buffer.
 */
const char *rtc_get_time_string(char *buffer);

/**
 * @brief Get the tick count.
 *
 * @return Ticks since system start.
 */
RtcTicks rtc_get_ticks(void);

/**
 * @brief Initialize the RTC alarm, used to wake from stop mode.
 */
void rtc_alarm_init(void);

/**
 * @brief Arm the alarm.
 *
 * @param num_ticks Ticks from now until the alarm fires.
 */
void rtc_alarm_set(RtcTicks num_ticks);

/** @brief Clear the stored timezone, as part of a factory reset. */
void rtc_timezone_clear(void);

/**
 * @brief Get the ticks elapsed since the alarm was last set.
 *
 * @return Ticks since the last rtc_alarm_set().
 */
RtcTicks rtc_alarm_get_elapsed_ticks(void);

/**
 * @brief Check whether the alarm is initialized.
 *
 * Used to avoid entering stop mode before the alarm can wake the system.
 *
 * @return true if initialized.
 */
bool rtc_alarm_is_initialized(void);

/**
 * @brief Format a time as local time.
 *
 * @param[out] buffer Buffer of at least @ref TIME_STRING_BUFFER_SIZE bytes.
 * @param t Time to format.
 * @return @p buffer.
 */
const char *time_t_to_string(char *buffer, time_t t);

#ifdef CONFIG_SOC_NRF52
/** @brief RTC interrupt handler. */
void rtc_irq_handler(void);
/**
 * @brief Drive the OS tick from the RTC instead of SysTick.
 *
 * SysTick stops while the nRF52 sleeps.
 */
void rtc_enable_synthetic_systick(void);
/** @brief Pause the RTC-driven OS tick, before entering stop mode. */
void rtc_systick_pause(void);
/** @brief Resume the RTC-driven OS tick. */
void rtc_systick_resume(void);
#endif

/** @} */
