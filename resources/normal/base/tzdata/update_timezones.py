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

# backward goes last so we can just always do backreferences for links
sh.cat(
    "africa",
    "antarctica",
    "asia",
    "australasia",
    "europe",
    "etcetera",
    "northamerica",
    "southamerica",
    "backward",
    _out=tz_file,
)
print("Updated database written to {}".format(tz_file))
