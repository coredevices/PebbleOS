# Build system

The firmware is built by three cooperating pieces:

- [Kconfig](kconfig.md) decides *what* is built: every option, driver and
  service is a symbol, and a board's defconfig plus the project and variant
  fragments resolve to one configuration per build directory.
- [CMake](cmake.md), with Ninja as the generator, decides *how*: each
  directory declares a library from the sources its symbols enable, and all
  of them are linked into a single firmware image.
- Python generators under `tools/cmake/` produce what neither can express
  directly: the Kconfig output itself, resources, version and registry
  headers, and the post-link image steps.

{doc}`../../pbl/index` drives the whole thing, but adds nothing a plain
CMake invocation cannot do:

```shell
cmake -B build -GNinja -DBOARD=asterix
cmake --build build
```

## From configuration to image

```{mermaid}
flowchart TD
  in["BOARD, VARIANT, -DCONFIG_*"]
  kconfig["tools/cmake/kconfig.py"]
  cfg[".config, autoconf.h, config.cmake"]
  cmake["CMakeLists.txt, cmake/modules/<br/>third_party, lib, kernel, soc, subsys, drivers, fw"]
  libs["Object and static libraries<br/>one pbl_library() per directory"]
  gen["Resource pack, generated headers<br/>pbl_resources(), tools/cmake/ generators"]
  link["pbl_link_firmware()"]
  out["pebbleos.elf, .hex, .bin,<br/>log hash dictionary"]
  in --> kconfig --> cfg --> cmake
  cmake --> libs --> link
  cmake --> gen --> link
  link --> out

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class in,out app
  class kconfig,cmake,link system
  class cfg,libs,gen base
```

1. **Configure.** The root `CMakeLists.txt` takes the board and variant
   (`-DBOARD=`, `-DVARIANT=`), and hands them with any `-DCONFIG_*`
   overrides to `tools/cmake/kconfig.py`. It writes `.config`,
   `autoconf.h` and `config.cmake`, which CMake includes, so every Kconfig
   symbol becomes a CMake variable.
2. **Toolchain.** The bare-metal ARM toolchain comes from the installed
   PebbleOS SDK; boards that do not run on ARM, such as `native_emery`,
   bring their own `toolchain.cmake`.
3. **Libraries.** The top-level directories are added in turn. Each one
   contributes libraries, global include directories, linker script
   fragments and generated sources through the `pbl_*` helpers.
4. **Generated sources.** Version and registry headers, the resource
   pack and its tables, and the SDK syscall shims are produced by custom
   commands; every library waits for the generated headers.
5. **Link.** `pbl_link_firmware()` preprocesses the linker script with the
   registered fragments, links every library into `pebbleos.elf`, and
   derives the flashable images, the log hash dictionary and the size
   checks from it.

Kconfig re-runs whenever a Kconfig file or a configuration fragment
changes, and CMake regenerates whenever a `CMakeLists.txt` does, so
`pbl build` is all that is needed after editing either.

## Build directory

| Path | Contents |
| --- | --- |
| `.config` | The resolved configuration |
| `autoconf.h` | Every symbol as a C macro, force-included into each compilation |
| `config.cmake` | Every symbol as a CMake variable, plus the board name, revision and runners |
| `menuconfig.conf` | Changes made with {doc}`../../pbl/menuconfig`, kept across reconfigurations |
| `pebbleos.elf`, `.hex`, `.bin` | The firmware image |
| `system_resources.pbpack` | The system resource pack |
| `fw/loghash_dict.json` | The dictionary that decodes hashed log messages |
| `generated/` | Generated headers and sources |
| `linker/` | The linker script fragments, collected per hook point |

## PebbleOS SDK

`cmake/modules/pebbleos_sdk.cmake` locates the installed SDK once and caches
it as `PEBBLEOS_SDK_ROOT`. It honours `PEBBLEOS_SDK_ROOT` from the
environment, which the SDK's `env.sh` exports, and otherwise picks the
newest `pebbleos-sdk-<version>` under the home directory or `/opt` that
satisfies `SDK_VERSION`. Pass `-DPEBBLEOS_SDK_ROOT=<dir>` when configuring
to use a specific one. Every tool lookup then uses the SDK as its first
search path: the toolchain in `cmake/toolchain/arm-none-eabi.cmake`, and
`PBL_QEMU`, `PBL_SFTOOL` and `PBL_GDB` for the `pbl` device commands, which
read them back from `CMakeCache.txt`. Each is an ordinary `find_program`
result, so it is cached too and can be overridden with `-D`.

## Directory layout

```text
CMakeLists.txt        the firmware image: what is built and linked
Kconfig               the root of the configuration tree
boards/<board>/       defconfig, Kconfig and manifest of each board
fw/prj*.conf          project and variant configuration fragments
cmake/modules/        the build model, split by concern
cmake/toolchain/      the bare-metal ARM and host toolchains
tools/cmake/          the generators the build shells out to
```

The unit tests are a CMake project of their own, built for the host out of
their own build directory; see {doc}`../tests/unit`. The waf under `sdk/` is
not used to build anything here: it is packaged into the SDK for app
developers to build their own projects with; see {doc}`../../sdk/building`.

```{toctree}
:hidden:

cmake.md
kconfig.md
```
