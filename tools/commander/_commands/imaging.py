# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from .. import PebbleCommander, exceptions, parsers


@PebbleCommander.command()
def image_resources(cmdr, pack='build/system_resources.pbpack'):
    """ Image resources.
    """
    import pulse_flash_imaging
    pulse_flash_imaging.load_resources(cmdr.connection, pack,
                                       progress=cmdr.interactive, verbose=cmdr.interactive)


@PebbleCommander.command()
def image_firmware(cmdr, firm='build/prf/src/fw/tintin_fw.bin', address=None):
    """ Image recovery firmware.
    """
    import pulse_flash_imaging
    if address is not None:
        address = int(str(address), 0)
    pulse_flash_imaging.load_firmware(cmdr.connection, firm,
                                      verbose=cmdr.interactive, address=address)
