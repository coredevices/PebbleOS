/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup soc_nrf_ppi PPI
 * @ingroup soc_nrf
 * @brief nRF52 programmable peripheral interconnect channels.
 *
 * A channel connects a peripheral event to a task, and optionally to a second, fork task, so
 * that the task runs on the event without the CPU. Drivers allocate their channels once:
 *
 * @code{.c}
 * ret = soc_nrf_ppi_alloc(&ch);
 * if (ret != 0) {
 *   return ret;
 * }
 *
 * soc_nrf_ppi_setup(ch, timer_base + EVENTS_COMPARE0, gpiote_base + TASKS_SET0, 0U);
 * soc_nrf_ppi_enable(PBL_BIT(ch));
 * @endcode
 *
 * The channels the NimBLE radio driver programs directly are never allocated when it is built.
 * @{
 */

/**
 * @brief Allocate a channel, for good.
 *
 * @param[out] channel Allocated channel.
 * @return 0 or @c -ENOMEM if all channels are in use.
 */
int soc_nrf_ppi_alloc(uint8_t *channel);

/**
 * @brief Connect the event and tasks of a channel.
 *
 * @param channel Channel, from soc_nrf_ppi_alloc().
 * @param eep Event register address.
 * @param tep Task register address.
 * @param fork_tep Fork task register address, or 0 for none.
 */
void soc_nrf_ppi_setup(uint8_t channel, uintptr_t eep, uintptr_t tep, uintptr_t fork_tep);

/**
 * @brief Enable channels.
 *
 * @param mask Channels, one bit each.
 */
void soc_nrf_ppi_enable(uint32_t mask);

/**
 * @brief Disable channels.
 *
 * @param mask Channels, one bit each.
 */
void soc_nrf_ppi_disable(uint32_t mask);

/** @} */
