# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""
Computes the minimum required SDK version for an app based on which SDK
functions it actually uses. This allows apps to only require SDK versions
that contain the functions they need, rather than always requiring the
newest SDK version they were compiled against.

The key insight is that apps that don't use newly-added SDK functions
should be able to run on older firmware that doesn't have those functions.
"""

import json
import os
import subprocess


def get_used_symbols(elf_path):
    """
    Extract the list of defined symbols from an ELF file.
    These are the SDK functions that were linked into the app.
    """
    try:
        # Use nm to list all defined symbols in the text section
        result = subprocess.run(
            ['arm-none-eabi-nm', '--defined-only', elf_path],
            capture_output=True,
            text=True,
            check=True
        )

        symbols = set()
        for line in result.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3:
                # Format: address type name
                symbol_type = parts[1]
                symbol_name = parts[2]
                # Only include text (T/t) symbols - these are functions
                if symbol_type in ('T', 't'):
                    symbols.add(symbol_name)

        return symbols
    except subprocess.CalledProcessError:
        return set()
    except FileNotFoundError:
        # arm-none-eabi-nm not found
        return set()


def compute_sdk_version(elf_path, symbol_revisions_path, default_major, default_minor):
    """
    Compute the SDK version for an app.

    The symbol_revisions.json file contains a mapping of function names to the
    SDK minor version (not revision!) when they were added. We find the maximum
    SDK minor version among all SDK functions the app uses.

    Returns a dict with 'major' and 'minor' keys.
    """
    if not os.path.exists(symbol_revisions_path):
        # Fall back to default if symbol_revisions.json doesn't exist
        return {
            'major': default_major,
            'minor': default_minor
        }

    try:
        # Load the symbol -> sdk_minor mapping
        with open(symbol_revisions_path, 'r') as f:
            symbol_data = json.load(f)
    except (json.JSONDecodeError, IOError):
        return {
            'major': default_major,
            'minor': default_minor
        }

    symbol_sdk_minors = symbol_data.get('symbols', {})

    # Get symbols used by the app
    used_symbols = get_used_symbols(elf_path)

    if not used_symbols or not symbol_sdk_minors:
        return {
            'major': default_major,
            'minor': default_minor
        }

    # Find the maximum SDK minor version among used SDK functions
    max_sdk_minor = 0
    for symbol in used_symbols:
        if symbol in symbol_sdk_minors:
            sdk_minor = symbol_sdk_minors[symbol]
            if sdk_minor > max_sdk_minor:
                max_sdk_minor = sdk_minor

    if max_sdk_minor > 0:
        return {
            'major': default_major,
            'minor': max_sdk_minor
        }
    else:
        return {
            'major': default_major,
            'minor': default_minor
        }
