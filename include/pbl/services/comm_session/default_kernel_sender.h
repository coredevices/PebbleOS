/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_comm_session_default_kernel_sender Default kernel sender
 * @ingroup services_comm_session
 * @brief Kernel-heap send buffers behind the session send buffer API.
 * @{
 */

/**
 * @brief Initialize the default kernel sender, once at boot.
 */
void comm_default_kernel_sender_init(void);

/** @} */
