# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""
Generates a JSON file mapping SDK function names to their minimum required SDK minor version.
This file is used at app build time to compute the minimum SDK version an app requires
based on which SDK functions it actually uses.

Key insight: The "addedRevision" in exported_symbols.json is NOT the same as
SDK_VERSION_MINOR. We need to map function revisions to actual SDK minor versions.
"""

import json
import os.path

SYMBOL_REVISIONS_FILE = 'symbol_revisions.json'


def gen_symbol_sdk_minors(functions, current_revision, current_sdk_minor):
    """
    Generates a dict mapping function names to their minimum required SDK minor version.
    Only includes non-removed functions.

    The mapping is:
    - Functions added at current_revision -> current_sdk_minor
    - Functions added at current_revision - N -> current_sdk_minor - N
    - Functions added at revision 0 -> 0 (baseline)

    This ensures apps only require the SDK version that has the functions they use.
    """
    symbols = {}
    for f in functions:
        if f.removed:
            continue

        # Calculate SDK minor version for this function
        # The offset between revision and sdk_minor is: current_revision - current_sdk_minor
        # So: sdk_minor = revision - (current_revision - current_sdk_minor)
        # But we clamp to 0 as minimum
        offset = current_revision - current_sdk_minor
        sdk_minor = max(0, f.added_revision - offset)
        symbols[f.name] = sdk_minor

    return symbols


def make_symbol_revisions_file(functions, sdk_include_dir, current_revision, current_sdk_minor):
    """
    Writes the symbol revisions JSON file to the SDK include directory.
    This file maps SDK function names to their minimum required SDK minor version.

    Args:
        functions: List of FunctionExport objects from exported_symbols.json
        sdk_include_dir: Path to SDK include directory
        current_revision: Current revision from exported_symbols.json
        current_sdk_minor: Current SDK_VERSION_MINOR from pebble_process_info.h
    """
    data = {
        'version': 1,
        'current_sdk_minor': current_sdk_minor,
        'current_revision': current_revision,
        'symbols': gen_symbol_sdk_minors(functions, current_revision, current_sdk_minor)
    }

    revisions_path = os.path.join(sdk_include_dir, SYMBOL_REVISIONS_FILE)
    with open(revisions_path, 'w') as f:
        json.dump(data, f, sort_keys=True, indent=2)
