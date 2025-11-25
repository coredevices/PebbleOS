# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from .. import PebbleCommander, exceptions, parsers


@PebbleCommander.command()
def audit_delay(cmdr):
    """ Audit delay_us.
    """
    return cmdr.send_prompt_command("audit delay")
