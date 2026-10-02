/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @defgroup services_process_management_process_loader_flash Flash process loader
 * @ingroup services_process_management
 * @brief Unused leftover of the flash process loader.
 * @{
 */

/**
 * @brief Unused stub with an empty body; processes are loaded with process_loader_load().
 *
 * @param task Task the process runs on.
 * @param config Process configuration.
 * @param app_md Process metadata.
 * @return Undefined.
 */
bool process_loader_load_from_flash(PebbleTask task, const ProcessConfig *config,
                                    const PebbleProcessMd *app_md) {
}

/** @} */
