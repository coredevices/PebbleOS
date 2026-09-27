<!-- SPDX-FileCopyrightText: 2026 Core Devices LLC -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Firmware text rendering in WebAssembly

With Emscripten on PATH:

```sh
emcmake cmake -S tools/text2wasm -B build-text2wasm
cmake --build build-text2wasm --target text2wasm
```

`dist/renderer.js` contains WASM and its loader. The output also includes Gothic
base fonts and the firmware revision. Load with `createPebbleRenderer()`, pass a
UTF-8 string, base PBF pointer/length, and extension PBF pointer/length to `render`, then copy the returned 144×168
RGBA framebuffer before the next call. The caller retains ownership of inputs.

The target compiles firmware glyph decoding, layout, RTL/shaping, and drawing
sources unchanged. Resource reads are served from memory. Test adapters supply
application state and heap services. A generated header disables the on-watch
event-size assertion for Emscripten's different time ABI; events are not used.

This is a text-box demo, not a full firmware emulator. It has no emoji font,
or system-screen layout support yet. Optional extension PBFs use firmware glyph fallback. Input fonts must be
trusted, compiler-produced PBF files; malformed PBF handling is not hardened.
The browser host should cap text length and recreate the module after a trap.
