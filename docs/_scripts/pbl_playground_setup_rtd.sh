#!/bin/sh
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Read the Docs: what the playground's firmware build needs (see
# docs/_ext/pbl_playground.py). Emscripten goes to $HOME/emsdk; em-config,
# which CMake looks up, to the build's virtualenv, the only directory every
# job has on PATH.

set -e

EMSCRIPTEN_VERSION=6.0.12

git submodule update --init --depth 1 -- \
  third_party/cmsis_core/CMSIS \
  third_party/mbedtls/mbedtls \
  third_party/nanopb/nanopb \
  third_party/nimble/mynewt-nimble \
  third_party/qr_code_generator/QR-Code-generator \
  third_party/resources/iconography \
  third_party/speex/speex \
  third_party/tinymt/TinyMT \
  third_party/upng/upng \
  third_party/uzlib/uzlib

python -m pip install -r requirements.txt

git clone --depth 1 --branch "$EMSCRIPTEN_VERSION" \
  https://github.com/emscripten-core/emsdk.git "$HOME/emsdk"
"$HOME/emsdk/emsdk" install "$EMSCRIPTEN_VERSION"
"$HOME/emsdk/emsdk" activate "$EMSCRIPTEN_VERSION"

cat > "$READTHEDOCS_VIRTUALENV_PATH/bin/em-config" <<WRAPPER
#!/bin/sh
exec "$HOME/emsdk/upstream/emscripten/em-config" "\$@"
WRAPPER
chmod +x "$READTHEDOCS_VIRTUALENV_PATH/bin/em-config"
