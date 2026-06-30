#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
# run_tests.sh — Canonical host-test container runner for PebbleOS
#
# Reproduces test.yml steps inside ghcr.io/coredevices/pebbleos-docker:v5
#
# Usage: ./tools/ci/run_tests.sh [waf test args]
#
# Environment variables (all optional):
#   PEBBLEOS_DOCKER_IMAGE — Docker image (default: ghcr.io/coredevices/pebbleos-docker:v5)
#   TEST_BOARD            — Board to test (default: qemu_gabbro)
#
# Examples:
#   ./tools/ci/run_tests.sh                    # Run full test suite
#   ./tools/ci/run_tests.sh -k "test_foo"      # Run only test_foo
#   TEST_BOARD=qemu_obelix ./tools/ci/run_tests.sh  # Test on obelix

set -euo pipefail

# Configuration
IMAGE="${PEBBLEOS_DOCKER_IMAGE:-ghcr.io/coredevices/pebbleos-docker:v5}"
BOARD="${TEST_BOARD:-qemu_gabbro}"

# Resolve repo root from script location
REPO_ROOT="$(git -C "$(dirname "$(readlink -f "$0")")/../.." rev-parse --show-toplevel)"

# Run CI steps inside container (mirroring .github/workflows/test.yml)
docker run --rm -it \
  -v "$REPO_ROOT:/work" \
  -w /work \
  "$IMAGE" \
  bash -lc "
    set -euo pipefail

    # Mark /work as safe (required for submodules)
    git config --system --add safe.directory /work

    # Install Python dependencies
    pip install -U pip
    pip install -r requirements.txt

    # Configure build
    ./waf configure --board '$BOARD'

    # Run tests (pass through extra args)
    ./waf test $*
  "
