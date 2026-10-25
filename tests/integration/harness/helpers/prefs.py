# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Preferences inserted into the watch's Prefs blob DB by the phone."""

from harness.errors import HarnessError


class _Key:
    def __init__(self, raw):
        self.bytes = raw


def set_pref(dut, key, value):
    """Insert a raw preference value under ``key``."""
    from libpebble2.protocol.blobdb import BlobStatus
    from libpebble2.services.blobdb import BlobDBClient, SyncWrapper

    status = SyncWrapper(
        BlobDBClient(dut.protocol).insert,
        0x07,  # Prefs is not named in older libpebble2 BlobDatabaseID enums.
        _Key(key.encode()),
        value,
    ).wait()
    if status != BlobStatus.Success:
        name = "no response" if status is None else BlobStatus(status).name
        raise HarnessError(f"blob DB insert preference {key!r}: {name}")
