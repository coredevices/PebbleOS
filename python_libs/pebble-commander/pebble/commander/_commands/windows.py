# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from .. import PebbleCommander, exceptions, parsers


@PebbleCommander.command()
def window_stack(cmdr):
    """ Dump the window stack.
    """
    return cmdr.send_prompt_command("window stack")


@PebbleCommander.command()
def modal_stack(cmdr):
    """ Dump the modal stack.
    """
    return cmdr.send_prompt_command("modal stack")
