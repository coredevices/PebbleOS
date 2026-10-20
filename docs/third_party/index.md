# Third-party

Code PebbleOS builds on but does not maintain lives under `third_party/`.
Each component is a Git submodule, next to the glue that builds it into
the firmware: a `CMakeLists.txt` and, for optional components, a Kconfig
symbol that enables it. Some are forks under
[coredevices](https://github.com/coredevices) carrying PebbleOS changes.

## Components

| Component | Used for | License |
| --- | --- | --- |
| [CMSIS](https://github.com/ARM-software/CMSIS_5) | Arm Cortex-M core headers | Apache-2.0 |
| [nrfx](https://github.com/NordicSemiconductor/nrfx) | nRF52840 peripheral drivers | BSD-3-Clause |
| [SiFli-SDK](https://github.com/coredevices/SiFli-SDK) | SF32LB52 HAL and peripheral drivers (fork) | Apache-2.0 |
| [NimBLE](https://github.com/coredevices/mynewt-nimble) | Bluetooth Low Energy host stack (fork) | Apache-2.0 |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) | Cryptography | Apache-2.0 OR GPL-2.0-or-later |
| [Moddable](https://github.com/coredevices/moddable) | XS JavaScript engine (fork); see {doc}`moddable` | LGPL-3.0-or-later AND Apache-2.0 |
| [picolibc](https://github.com/picolibc/picolibc) | C library | BSD-3-Clause AND BSD-2-Clause |
| [nanopb](https://github.com/nanopb/nanopb) | Protocol Buffers encoding | Zlib |
| [Speex](https://github.com/xiph/speex) | Voice audio codec | BSD-3-Clause |
| [uPNG](https://github.com/coredevices/upng) | PNG and APNG decoding (fork) | Zlib |
| [uzlib](https://github.com/coredevices/uzlib) | Deflate decompression (fork) | Zlib |
| [QR-Code-generator](https://github.com/nayuki/QR-Code-generator) | QR code rendering | MIT |
| [TinyMT](https://github.com/MersenneTwister-Lab/TinyMT) | Pseudorandom numbers | BSD-3-Clause |
| [iconography](https://github.com/pebble-dev/iconography) | System icons | Apache-2.0 |

`third_party/nonfree` holds the vendor binaries and libraries the hardware
needs but that are not open source: the nRF Fuel Gauge library, the
CST816 touch controller firmware, the GH3x2x heart rate algorithms and the
SF32LB52 Bluetooth controller patches. Each
keeps its vendor's license.

## Adding or updating a component

Every component is described in `third_party/sbom.yml`: its name,
supplier, license and, for a submodule, the commit it is checked out at.
That metadata feeds the {doc}`sbom`, and CI checks it against
the tree: a submodule without an entry, or an entry whose commit does not
match the submodule, fails the check. Update the entry along with the
submodule:

```shell
python3 tools/cmake/sbom.py check
```

```{toctree}
:hidden:

moddable.md
sbom.md
```
