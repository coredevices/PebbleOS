#!/bin/sh
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

#
# Run this script to update waf to a newer version and get rid of the 
# files we do not need for Pebble SDK.

set -x

VERSION=2.1.4
DOWNLOAD="https://waf.io/waf-$VERSION.tar.bz2"

TMPFILE=`mktemp -t waf-tar-bz`

# Remove existing waf folder
rm -fr waf

# Download and extract what we need from waf distrib
wget -O - $DOWNLOAD |tar -yx         \
        --include "waf-$VERSION/waf-light" \
        --include "waf-$VERSION/waflib/*" \
        --include "waf-$VERSION/wscript" \
        --exclude "waf-$VERSION/waflib/extras" \
        -s "/waf-$VERSION/waf/"

# Add some python magic for our lib to work
# (they will be copied in extras and require the init)
mkdir waf/waflib/extras
touch waf/waflib/extras/__init__.py
