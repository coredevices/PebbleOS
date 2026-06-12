#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
# build_firmware.sh — Firmware build runner for PebbleOS in Docker
#
# Reproduces build-firmware.yml, build-prf.yml, and build-qemu.yml steps
# inside ghcr.io/coredevices/pebbleos-docker:v5
#
# Usage: ./tools/ci/build_firmware.sh --board <board> [options]
#
# Environment variables (all optional):
#   PEBBLEOS_DOCKER_IMAGE — Docker image (default: ghcr.io/coredevices/pebbleos-docker:v5)
#   BOARD                  — Board to build (required unless set via --board flag)
#   CONFIGURE_EXTRA        — Additional configure flags (e.g. --variant=prf -DCONFIG_MFG=y)
#
# Examples:
#   ./tools/ci/build_firmware.sh --board asterix
#   ./tools/ci/build_firmware.sh --board obelix_dvt
#   ./tools/ci/build_firmware.sh --board asterix --variant prf -DCONFIG_MFG=y -DCONFIG_LOG_HASHED=n
#   BOARD=qemu_gabbro ./tools/ci/build_firmware.sh

set -euo pipefail

# Configuration
IMAGE="${PEBBLEOS_DOCKER_IMAGE:-ghcr.io/coredevices/pebbleos-docker:v5}"
BOARD="${BOARD:-}"
CONFIGURE_EXTRA="${CONFIGURE_EXTRA:-}"

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    --board)
      BOARD="$2"
      shift 2
      ;;
    --variant|--js-engine|--nowatchdog|--nostop|--nosleep)
      # Pass through configure flags
      CONFIGURE_EXTRA="${CONFIGURE_EXTRA} $1${2:+=$2}"
      shift $([[ "$1" == --*=* ]] && echo 1 || echo $([[ $# -gt 1 ]] && echo 2 || echo 1))
      ;;
    -D*)
      # Pass through -D flags (e.g. -DCONFIG_MFG=y)
      CONFIGURE_EXTRA="${CONFIGURE_EXTRA} $1"
      shift
      ;;
    *)
      # Pass through other arguments to ./waf build
      break
      ;;
  esac
done

# Validate BOARD is set
if [[ -z "$BOARD" ]]; then
  cat >&2 <<EOF
Error: BOARD must be set. Specify via --board flag or BOARD environment variable.

Valid boards:
  - asterix
  - obelix_dvt
  - obelix_pvt
  - getafix_evt
  - getafix_dvt
  - getafix_dvt2
  - qemu_flint
  - qemu_emery
  - qemu_gabbro

Usage examples:
  ./tools/ci/build_firmware.sh --board asterix
  BOARD=obelix_dvt ./tools/ci/build_firmware.sh
  ./tools/ci/build_firmware.sh --board qemu_gabbro --variant prf
EOF
  exit 1
fi

# Resolve repo root from script location
REPO_ROOT="$(git -C "$(dirname "$(readlink -f "$0")")/../.." rev-parse --show-toplevel)"

# Determine build target based on board type
BUILD_TARGET=""
if [[ "$BOARD" == qemu_* ]]; then
  BUILD_TARGET="qemu_image_micro qemu_image_spi"
fi

# Run CI steps inside container (mirroring .github/workflows/build-*.yml)
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
    ./waf configure --board '$BOARD' $CONFIGURE_EXTRA

    # Build firmware (pass through remaining arguments)
    ./waf build $BUILD_TARGET $*
  "
