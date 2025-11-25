# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

"""
This script is used to import any paths required by the SDK file structure for building Pebble
projects. Even though this script is not specifically a waftool, we benefit from bundling it
together with the other waftools because it automatically gets included in the search path used for
imports by other waftools.
"""

import os
import sys

# Go up 3 levels from extras/sdk_paths.py to SDK root
sdk_root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(sdk_root_dir, 'common/waftools'))
sys.path.append(os.path.join(sdk_root_dir, 'common/tools'))
