# CMake

The build model lives in `cmake/modules/`: every directory that contributes
code declares a library, and the helpers below register what it needs.
Nothing has to list the libraries; all of them end up in the single
firmware image.

## Libraries

```cmake
pbl_library()
pbl_library_sources(service.c util.c)
```

`pbl_library()`
: Declare the library the following `pbl_library_*()` calls apply to,
  named after its directory (`fw/services/timeline` →
  `fw__services__timeline`). The target is only created once it gets its
  first source, so a directory whose sources are all configured out
  contributes nothing.

`pbl_library_named(name)`
: The same, with a name of its own, for a library something else refers
  to or a directory that builds more than one.

`pbl_library_kind(kind)`
: `objects` (the default) links every object file. `stlib` builds a
  static library the linker pulls objects from on demand; use it for
  third-party code that ships more than the firmware references. Call it
  before the first source.

`pbl_library_sources(src...)`
: Add sources. Each object is compiled with `__FILE_NAME_LEGACY__` set to
  its source's name, which asserts and log messages report.

`pbl_library_sources_ifdef(SYMBOL src...)`, `pbl_library_sources_ifndef(SYMBOL src...)`
: Add sources when a Kconfig symbol is, or is not, set.

`pbl_library_sources_glob(pattern... [RECURSE] [EXCLUDE pattern...])`
: Add the sources matching glob patterns, re-evaluated when the build
  regenerates. An `EXCLUDE` pattern is a glob relative to the directory
  (`voice/*.c`, `**/system_nrf*.c`) or a directory whose whole subtree is
  dropped (`vendor`, `ble_hrm/**`).

`pbl_library_include_directories(dir...)`, `pbl_library_compile_definitions(def...)`, `pbl_library_compile_options(opt...)`
: Compile settings private to the current library.

`pbl_library_import(name path...)`
: Link a prebuilt vendor archive, `lib<name>.a` found under one of the
  paths.

Dependencies between firmware libraries are not declared: they resolve at
the final link.

## Global settings

Headers and defines are visible to every library, registered once next to
what they expose:

```cmake
pbl_include_directories(include)
pbl_compile_definitions(FOO_ENABLED=1)
```

`pbl_include_directories(dir...)`
: Add header directories for every library. Relative paths resolve
  against the calling directory, and the matching build directory is added
  too, so generated headers are found next to their hand-written
  neighbours.

`pbl_compile_definitions(def...)`, `pbl_compile_options(opt...)`
: Defines and compiler options for every library.

`pbl_include_directories_ifdef(SYMBOL dir...)`, `pbl_compile_definitions_ifdef(SYMBOL def...)`
: The same, when a Kconfig symbol is set.

`pbl_link_options(opt...)`
: Options for the firmware link, from anywhere in the tree.

Headers are global; there is no per-consumer opt-in. A library that is not
always needed is gated behind a Kconfig symbol instead, and consumers
`select` (or `imply`) it; see [Kconfig](kconfig.md).

## Conditional directories

Kconfig symbols are ordinary CMake variables, so directories and sources
follow the configuration:

```cmake
pbl_add_subdirectory_ifdef(CONFIG_AUDIO audio)
pbl_library_sources_ifdef(CONFIG_ACCEL_LSM6DSO lsm6dso/lsm6dso.c)
```

`pbl_add_subdirectory_ifdef(SYMBOL dir...)`
: `add_subdirectory()` when a Kconfig symbol is set.

## Linker script

The master script, `kernel/pebbleos.ld`, includes the common sections in
`kernel/linker/` and the fragments the architecture provides, such as the
vector table. It goes through the C preprocessor with `autoconf.h`, so it
can use Kconfig symbols, `#include` and `#if`. Libraries add to it at fixed
hook points:

```cmake
pbl_linker_sources(rom-start shell.ld)
pbl_library_ramfunc(${ramfunc_sources})
```

`pbl_linker_include_directories(dir...)`
: Add directories to the master script's include path. The architecture
  uses it for the fragments the master script includes by name.

`pbl_linker_sources(location file... [SORT_KEY key])`
: Include linker script fragments at a hook point: `memory`, `rom-start`,
  `ram-sections`, `ramfunc` or `footer`. Fragments are included sorted by
  sort key, then by registration order.

`pbl_library_ramfunc(src...)`
: Place the code and read-only data of some of the current library's
  sources in `.ramfunc`, to run from RAM; see {doc}`../../kernel/memory`. A
  no-op without `CONFIG_RAMFUNC`.

## Generated sources

`pbl_library_nanopb_sources(schema.proto...)`
: Compile protobuf schemas with nanopb into the current library. A
  `<name>.options` file next to a schema tunes its generated C types.

`pbl_dynamic_resource(path ...)`
: Add a resource produced by another build step to the system resource
  pack, after the ones the board's resource maps declare.

Generated headers are produced by custom commands attached to the
`pbl_generated_headers` target, which every library waits for. The
generators live in `tools/cmake/`:

- `kconfig.py`: runs Kconfig and writes `.config`, `autoconf.h` and
  `config.cmake`.
- `resources.py`: turns the board's resource maps into build rules, in
  batches so a few hundred resources do not cost a few hundred interpreter
  start-ups, and builds the resource pack and its tables.
- `generate.py`: the other source generators (version header, app
  registry, protocol endpoint table, applib allocator tables, log string
  hashing, stored apps).
- `firmware.py`: image steps, such as size checks, bundling and QEMU flash
  images.

## Native builds

The {doc}`native simulator <../emulation/native_simulator>` runs the firmware as a
host program. The bottom halves of its drivers talk to the host and see
none of the firmware's headers:

`pbl_host_sources(src...)`, `pbl_host_include_directories(dir...)`, `pbl_host_compile_options(opt...)`
: Sources, header directories and compile options of the host side.

## Unit tests

The unit tests are a separate CMake project, built for the host. Tests are
declared with `pbl_clar_test()`; see {doc}`../tests/unit`.
