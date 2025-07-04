# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re

import waflib.Context
import waflib.Logs

def get_git_revision(ctx):
    commit = ctx.cmd_and_log(['git', 'rev-parse', '--short', 'HEAD'], quiet=waflib.Context.BOTH).strip()
    timestamp = ctx.cmd_and_log(['git', 'log', '-1', '--format=%ct', 'HEAD'], quiet=waflib.Context.BOTH).strip()

    try:
        tag = ctx.cmd_and_log(['git', 'describe', '--dirty'], quiet=waflib.Context.BOTH).strip()
    except Exception:
        tag = "v9.9.9-dev"
        waflib.Logs.warn(f'Git tag not found, using {tag}')

    # Validate that git tag follows the required form:
    # See https://github.com/pebble/tintin/wiki/Firmware,-PRF-&-Bootloader-Versions
    # Note: version_regex.groups() returns sequence ('0', '0', '0', 'suffix'):
    version_regex = re.search(r"^v(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:(?:-)(.+))?$", tag)
    if not version_regex:
        raise ValueError(f'Invalid tag: {tag}')

    # Get version numbers from version_regex.groups() sequence and replace None values with 0
    # e.g. v2-beta11 => ('2', None, None, 'beta11') => ('2', '0', '0')
    version = [x if x else '0' for x in version_regex.groups()]

    # Used for pebble_pipeline payload, generate a string that contains everything after minor.
    # Force include patch as 0 if it doesn't exist.
    patch_verbose = str(version[2])
    str_after_patch = version[3]
    if (str_after_patch):
        patch_verbose += '-' + str_after_patch

    return {'TAG': tag,
            'COMMIT': commit,
            'TIMESTAMP': timestamp,
            'MAJOR_VERSION': version[0],
            'MINOR_VERSION': version[1],
            'PATCH_VERSION': version[2],
            'MAJOR_MINOR_PATCH_STRING' : ".".join(version[0:3]),
            "PATCH_VERBOSE_STRING": patch_verbose}
