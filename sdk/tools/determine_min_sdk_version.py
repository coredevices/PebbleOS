# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import json
from subprocess import Popen, PIPE

# Shim internals that should not be matched against the API revision map
SHIM_INTERNALS = {'jump_to_pbl_function', 'pbl_table_addr'}

# Revision 89 is the last historical mapping, frozen at minor 0x56.
# From revision 90 onward, minor byte = revision number.
HISTORICAL_CUTOFF_REVISION = 89
HISTORICAL_CUTOFF_MINOR = 0x56
SDK_MAJOR = 0x5


def revision_to_minor(revision):
    """Convert an API revision number to an SDK minor byte.
    Revisions <= 89 map to 0x56 (the historical cutoff).
    Revisions >= 90 map directly to the revision number."""
    if revision <= HISTORICAL_CUTOFF_REVISION:
        return HISTORICAL_CUTOFF_MINOR
    return revision


def determine_min_sdk_version(elf_path, revision_map_path):
    """Determine the minimum SDK version an app needs based on which
    API functions it actually uses.

    Args:
        elf_path: Path to the linked ELF binary
        revision_map_path: Path to api_update_revisions.json

    Returns:
        Tuple of (major, minor) SDK version bytes
    """
    with open(revision_map_path, 'r') as f:
        revision_map = json.load(f)

    # Get all defined symbols from the ELF
    nm_process = Popen(['arm-none-eabi-nm', '--defined-only', elf_path], stdout=PIPE)
    nm_output = nm_process.communicate()[0].decode('utf8')

    if not nm_output:
        # Fallback: no symbols found, use historical cutoff
        return (SDK_MAJOR, HISTORICAL_CUTOFF_MINOR)

    # Parse nm output to extract symbol names
    defined_symbols = set()
    for line in nm_output.splitlines():
        parts = line.split()
        if len(parts) == 3:
            defined_symbols.add(parts[2])

    # Find max revision among SDK functions used by the app
    max_revision = 0
    for symbol in defined_symbols:
        if symbol in SHIM_INTERNALS:
            continue
        if symbol in revision_map:
            max_revision = max(max_revision, revision_map[symbol])

    if max_revision == 0:
        # No SDK functions matched, use historical cutoff
        return (SDK_MAJOR, HISTORICAL_CUTOFF_MINOR)

    return (SDK_MAJOR, revision_to_minor(max_revision))
