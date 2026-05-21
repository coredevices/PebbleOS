#!/bin/bash
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
# Generate Linux fixtures using Docker
# This script runs tests in Docker to generate Linux-specific test fixtures

set -e

DOCKER_IMAGE="ghcr.io/coredevices/pebbleos-docker:v3"
BOARD="${TEST_BOARD:-snowy_bb2}"
TEST_MATCH="${1:-}"

echo "Generating Linux fixtures for board: $BOARD"
if [ -n "$TEST_MATCH" ]; then
  echo "Running tests matching: $TEST_MATCH"
fi

docker run --rm --platform linux/amd64 \
  -v "$(pwd):/work:cached" \
  -w /work \
  "$DOCKER_IMAGE" \
  bash -c "
    set -e
    echo 'Installing dependencies...'
    pip install -U pip > /dev/null 2>&1
    pip install -r requirements.txt > /dev/null 2>&1

    echo 'Configuring...'
    rm -f .wafpickle* .lock-waf* 2>/dev/null
    ./waf configure --board=$BOARD

    echo 'Running tests...'
    if [ -n '$TEST_MATCH' ]; then
      ./waf test -M '$TEST_MATCH' || true
    else
      ./waf test || true
    fi

    echo ''
    echo 'Generated fixtures are in: build/test/tests/failed/'
    echo 'Copy them with: cp build/test/tests/failed/*-expected.pbi tests/fixtures/graphics/'
  "
