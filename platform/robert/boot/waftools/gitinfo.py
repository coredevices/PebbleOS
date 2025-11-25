# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import re

import waflib.Context
import waflib.Logs


def get_git_revision(ctx):
    try:
        tag = ctx.cmd_and_log(['git', 'describe'], quiet=waflib.Context.BOTH).strip()
        commit = ctx.cmd_and_log(['git', 'rev-parse', '--short', 'HEAD'],
                                 quiet=waflib.Context.BOTH).strip()
        timestamp = ctx.cmd_and_log(['git', 'log', '-1', '--format=%ct', 'HEAD'],
                                    quiet=waflib.Context.BOTH).strip()
    except Exception:
        waflib.Logs.warn('get_git_version: unable to determine git revision')
        tag, commit, timestamp = ("?", "?", "1")
    # Validate that git tag follows the required form:
    # See https://github.com/pebble/tintin/wiki/Firmware,-PRF-&-Bootloader-Versions
    # Note: version_regex.groups() returns sequence ('0', '0', '0', 'suffix'):
    version_regex = re.search("^v(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:(?:-)(.+))?$", tag)
    if version_regex:
        # Get version numbers from version_regex.groups() sequence and replace None values with 0
        # e.g. v2-beta11 => ('2', None, None, 'beta11') => ('2', '0', '0')
        version = [x if x else '0' for x in version_regex.groups()[:3]]
    else:
        waflib.Logs.warn('get_git_revision: Invalid git tag! '
                         'Must follow this form: `v0[.0[.0]][-suffix]`')
        version = ['0', '0', '0', 'unknown']
    return {'TAG': tag,
            'COMMIT': commit,
            'TIMESTAMP': timestamp,
            'MAJOR_VERSION': version[0],
            'MINOR_VERSION': version[1],
            'PATCH_VERSION': version[2]}
