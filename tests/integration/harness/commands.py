# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The console commands the harness uses, as the firmware spells them: the
shell (CONFIG_SHELL), or the prompt it replaced."""

from harness.errors import Unsupported

SHELL = {
    "app_launch": "app launch {id}",
    "app_list": "app list",
    "bt_adv_slow": "bt adv_slow",
    "bt_airplane": "bt airplane {mode}",
    "bt_mac": "bt mac",
    "bt_pairing": "bt pairing",
    "bt_status": "bt status",
    "bt_unpair": "bt prefs_wipe",
    "click": "button click {button}",
    "click_multiple": "button multi {button} {presses} {hold_ms} {gap_ms}",
    "modals": "ui modals",
    "reset": "sys reset",
    "rx_disable": "sys rx_disable {seconds}",
    "set_time": "time set {timestamp}",
    "version": "version",
    "windows": "ui windows",
}

PROMPT = {
    "app_launch": "app launch {id}",
    "app_list": "app list",
    "click": "click short {button}",
    "click_multiple": "click multiple {button} {presses} {hold_ms} {gap_ms}",
    "modals": "modal stack",
    "reset": "reset",
    "rx_disable": "console disable rx {seconds}",
    "set_time": "set time {timestamp}",
    "version": "version",
    "windows": "window stack",
}


def command(build, name, **args):
    """Command ``name`` with ``args``, for ``build``'s console."""
    shell = bool(build.config.get("CONFIG_SHELL"))
    table = SHELL if shell else PROMPT
    if name not in table:
        raise Unsupported(
            f"no {name!r} command in the {'shell' if shell else 'prompt'}"
        )
    return table[name].format(**args)
