#!/bin/sh
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Read the Docs: whether this build has the playground, recorded in a
# .pbl_playground marker for the later jobs and docs/conf.py. Pull requests
# get it with the ci:docs-playground label.

if [ "$READTHEDOCS_VERSION_TYPE" != "external" ] ||
  curl -fsSL "https://api.github.com/repos/coredevices/pebbleos/issues/$READTHEDOCS_VERSION/labels" |
  grep -q '"name": *"ci:docs-playground"'; then
  touch .pbl_playground
fi
