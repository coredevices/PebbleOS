#!/usr/bin/env python
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


import os
import tempfile

import sh

TZDATA_DIR = os.path.dirname(os.path.abspath(__file__))

tmp_dir = tempfile.mkdtemp()
os.chdir(tmp_dir)

print("Downloading timezone data to {}".format(tmp_dir))

sh.wget("https://ftp.iana.org/tz/tzdata-latest.tar.gz")

print("Download complete")
print(sh.ls("-la", "tzdata-latest.tar.gz").strip())

sh.tar("-xvzf", "tzdata-latest.tar.gz")

tz_file = os.path.join(TZDATA_DIR, "timezones_olson.txt")

# Process each file to convert Vanguard sections to Rearguard sections as we concatenate
# Our parser doesn't support negative DST, so we need to use the Rearguard format
def process_tzfile(filename):
    """Process a timezone file to use Rearguard format instead of Vanguard."""
    in_vanguard = False
    in_rearguard = False
    
    with open(filename, 'r') as f:
        for line in f:
            # Detect section markers
            if '# Vanguard section' in line:
                in_vanguard = True
                in_rearguard = False
                yield line
                continue
            elif '# Rearguard section' in line:
                in_vanguard = False
                in_rearguard = True
                yield line
                continue
            elif '# End of rearguard section' in line or (in_rearguard and line.strip() and not line.strip().startswith('#') and not line.startswith('\t') and not line.startswith(' ')):
                # End of the vanguard/rearguard block
                in_vanguard = False
                in_rearguard = False
                yield line
                continue
            
            if in_vanguard:
                # Comment out vanguard lines
                if line.strip() and not line.strip().startswith('#'):
                    yield '#' + line
                else:
                    yield line
            elif in_rearguard:
                # Uncomment rearguard lines
                if line.strip().startswith('#\t') or line.strip().startswith('# \t'):
                    # Remove the leading '# ' or '#\t' to uncomment
                    yield line.replace('#\t', '\t', 1).replace('# \t', '\t', 1)
                else:
                    yield line
            else:
                # Normal line, write as-is
                yield line

print("Processing timezone data to use Rearguard format...")
# backward goes last so we can just always do backreferences for links
with open(tz_file, 'w') as outfile:
    for filename in ["africa", "antarctica", "asia", "australasia", "europe", 
                      "etcetera", "northamerica", "southamerica", "backward"]:
        for line in process_tzfile(filename):
            outfile.write(line)

print("Updated database written to {}".format(tz_file))
