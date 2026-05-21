#!/bin/bash
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
# Run tests in Docker to match CI environment
# This ensures consistent test results across different development platforms

set -e

DOCKER_IMAGE="ghcr.io/coredevices/pebbleos-docker:v3"
BOARD="${TEST_BOARD:-snowy_bb2}"

echo "Running tests in Docker for board: $BOARD"
echo "This matches the CI environment for consistent test results"

docker run --rm --platform linux/amd64 \
  -v "$(pwd):/work:cached" \
  -w /work \
  "$DOCKER_IMAGE" \
  ./waf configure --board="$BOARD" \
  && docker run --rm --platform linux/amd64 \
  -v "$(pwd):/work:cached" \
  -w /work \
  "$DOCKER_IMAGE" \
  ./waf test "$@"
