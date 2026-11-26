/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <host/ble_gap.h>

/**
 * @defgroup bluetooth_adv Advertising
 * @ingroup bluetooth
 * @brief Connectable advertising that follows what there is to advertise.
 *
 * A device has a few advertisements, e.g. one to be discovered and one for a bonded peer to
 * reconnect to, each enabled when it applies. The first one enabled, in the order they are
 * given, is on air while there is no connection. It airs at a short interval for some time, to
 * be found fast, and at a long one from then on, to save power. A stack that refuses to
 * advertise is asked again a second later.
 * @{
 */

/** @brief An advertisement. Zero-initialize what is not documented. */
struct pbl_bt_adv {
  /**
   * @brief Set the advertising and scan response payloads, e.g. with
   * @c ble_gap_adv_set_fields().
   *
   * @return 0 on success, a code of the stack otherwise.
   */
  int (*set_data)(void);
  /**
   * How long it airs at the short interval each time a connection ends, in seconds. With 0, the
   * end of a connection leaves it as it was.
   */
  uint16_t resume_short_secs;
  /** @cond INTERNAL_HIDDEN */
  bool enabled;
  uint64_t short_until;
  /** @endcond */
};

/** @brief How to advertise. */
struct pbl_bt_adv_config {
  /** The advertisements, the one to air first when several are enabled first. */
  struct pbl_bt_adv *const *advs;
  /** Number of advertisements. */
  size_t num_advs;
  /** Short advertising interval, in milliseconds. */
  uint16_t short_interval_ms;
  /** Long advertising interval, in milliseconds. */
  uint16_t long_interval_ms;
  /**
   * Connections that end this many times within @c churn_window_secs no longer bring the short
   * interval back: a peer that reconnects and drops over and over would otherwise keep the
   * device at it. 0 to always bring it back.
   */
  uint8_t churn_threshold;
  /** See @c churn_threshold. */
  uint16_t churn_window_secs;
  /** Handler of the GAP events of the connections made, as given to @c ble_gap_adv_start(). */
  ble_gap_event_fn *gap_event;
  /** Its argument. */
  void *gap_event_arg;
  /**
   * @brief What is on air changed. Optional.
   *
   * @param adv The advertisement on air, NULL if none is.
   * @param short_interval Whether it airs at the short interval.
   */
  void (*changed)(const struct pbl_bt_adv *adv, bool short_interval);
};

/**
 * @brief Start advertising what is enabled, once the host is synchronized.
 *
 * Every advertisement starts disabled.
 *
 * @param config How to advertise. It must outlive the advertising.
 */
void pbl_bt_adv_start(const struct pbl_bt_adv_config *config);

/** @brief Stop advertising, and disable every advertisement. */
void pbl_bt_adv_stop(void);

/**
 * @brief Enable an advertisement, or air it anew if it is enabled: its payloads are set again.
 *
 * @param adv The advertisement.
 * @param short_secs How long it airs at the short interval, in seconds.
 */
void pbl_bt_adv_enable(struct pbl_bt_adv *adv, uint16_t short_secs);

/**
 * @brief Disable an advertisement.
 *
 * @param adv The advertisement.
 */
void pbl_bt_adv_disable(struct pbl_bt_adv *adv);

/** @} */
