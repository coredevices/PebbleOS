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
