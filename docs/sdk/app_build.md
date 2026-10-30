# Building apps

Apps, watchfaces and Pebble packages build with CMake. The SDK ships a
CMake package, `PebbleSDK`, that a project finds before `project()`.
Building with `waf` still works but is deprecated: it warns on every
build, and will be removed.

## A project

A project is two files: `package.json` says what the app is (name, UUID,
target platforms, resources, message keys, capabilities, dependencies),
and `CMakeLists.txt` says what it is built from.

```cmake
cmake_minimum_required(VERSION 3.20)

# Before project(): the SDK selects the toolchain.
find_package(PebbleSDK REQUIRED HINTS $ENV{PEBBLE_SDK})

project(my-watchface C)

pebble_app(SOURCES src/c/main.c src/c/weather.c)
pebble_js(SOURCES src/pkjs/index.js)
```

`pebble build` builds it. Without the `pebble` tool, point CMake at the
SDK, the directory holding its `cmake/`, `common/` and platform
directories:

```shell
export PEBBLE_SDK=/path/to/pebble-sdk
cmake -B build -G Ninja
cmake --build build
```

The build writes `build/<project directory>.pbw`, and for each platform
`build/<platform>/pebble-app.elf`, where `pebble` looks for symbols.

Compile commands for editors are CMake's to write: configure with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (`pebble build --
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`) for `build/compile_commands.json`.

## How it works

`find_package(PebbleSDK)` reads `package.json` once, at configure time:
it validates it, numbers the message keys, picks each resource's file for
each platform (`image~color.png` over `image.png`), unpacks the Pebble
packages the project depends on, and writes what does not depend on the
code (`appinfo.json`, the per-platform `appinfo.auto.c`, the message key
sources, the linker scripts). Changing `package.json`, adding a file to
`resources/` or installing a package reconfigures the project.

One build directory builds every platform the project targets. The code is
described once, on an interface target, and built once per platform with
that platform's `pebble.h` and flags:

```{mermaid}
flowchart LR
  app["app (interface)"] --> app_basalt & app_emery & app_gabbro
  pj["package.json"] --> res_basalt & res_emery & res_gabbro
  res_basalt["basalt resources"] --> app_basalt
  res_emery["emery resources"] --> app_emery
  res_gabbro["gabbro resources"] --> app_gabbro
  app_basalt & app_emery & app_gabbro --> pbw["my-watchface.pbw"]
  js["PebbleKit JS"] --> pbw
```

Settings for all platforms go on the interface, those for one platform on
its target:

```cmake
target_compile_definitions(app INTERFACE SHOW_SECONDS=1)
target_include_directories(app INTERFACE src/c)
target_compile_options(app_aplite PRIVATE -DSMALL_FONTS)
```

Platform differences in the code itself use the `PBL_*` defines, as
before.

## Functions

`pebble_app(SOURCES ...)`
: The app. Creates the `app` interface and an `app_<platform>` executable
  for every platform. More sources can be added with
  `target_sources(app INTERFACE ...)`.

`pebble_worker(SOURCES ...)`
: The background worker, on `worker` and `worker_<platform>`.

`pebble_js(SOURCES ... [ENTRY file])`
: PebbleKit JS. With `enableMultiJS`, webpack bundles the sources into
  `pebble-js-app.js` starting from `ENTRY` (by default `main.pkjs` in
  `package.json`, else `src/pkjs/index.js`); without it, the files are
  copied into the pbw as they are. In a package, the JS it ships.

`pebble_package(SOURCES ... HEADERS ...)`
: A Pebble package (`"projectType": "package"`): a `lib<name>.a` per
  platform on `package` and `package_<platform>`, packed with the headers,
  resources and JS into `dist.zip` next to `package.json`.

These are called from the top-level `CMakeLists.txt`.

## Options

`PEBBLE_PLATFORMS`
: Build for some of the project's `targetPlatforms` only, e.g.
  `-DPEBBLE_PLATFORMS=emery` while developing.

`CMAKE_BUILD_TYPE`
: `Release` (the default) builds with `-Os` and defines `RELEASE`;
  `Debug` builds with `-O0` and defines `PBL_DEBUG`. `pebble build --debug`
  selects `Debug`.

`PEBBLE_WARNINGS_AS_ERRORS`
: Fail on compiler warnings. Off by default.

`CMAKE_C_STANDARD`
: Apps build as C99 unless the project sets another standard before
  `find_package(PebbleSDK)`.

`PEBBLE_TIMESTAMP`, `SOURCE_DATE_EPOCH`
: The timestamp recorded in the binaries and the pbw, the configure time
  by default.

## Pebble packages

Packages come from `npm install`, as listed in the `dependencies` of
`package.json`. A Pebble package's `dist.zip` is unpacked into the build
directory, never into `node_modules`; its headers are on the include path,
its archive is linked, its resources join the app's and its message keys
are numbered with the app's. Packages are linked as a group, so their order
does not matter. Plain npm packages are only seen by webpack.

## Porting from waf

A project whose `wscript` is the SDK's default ports by adding a
`CMakeLists.txt` like the one above, listing the C files under `src/c/`
(and `worker_src/c/`) and the JS under `src/pkjs/` that the `wscript`
built, and deleting the `wscript`. Customised wscripts translate to plain
CMake:

| `wscript` | `CMakeLists.txt` |
| --- | --- |
| `ant_glob('src/**/*.c')` | the sources, listed: `pebble_app(SOURCES src/main.c ...)` |
| `ant_glob(..., excl=['src/c/x.c'])` | the sources, without `src/c/x.c` |
| `ctx.env.append_value('CFLAGS', [...])` | `target_compile_options(app INTERFACE ...)` |
| `ctx.env.append_value('DEFINES', 'X=1')` | `target_compile_definitions(app INTERFACE X=1)` |
| `ctx.env.append_value('LINKFLAGS', [...])` | `target_link_options(app INTERFACE ...)` |
| `pbl_build(..., includes=[...])` | `target_include_directories(app INTERFACE ...)` |
| `pbl_bundle(js_entry_file='src/js/app.js')` | `pebble_js(SOURCES ... ENTRY src/js/app.js)` |
| a script run from `build()` that writes a resource | `add_custom_command(OUTPUT resources/... ...)`: the resource step waits for it |
| a file written from `build()` | `file(CONFIGURE ...)` or `add_custom_command()` |
| `pbl_suppress_newer_gcc_warnings()` | nothing: warnings are not errors unless asked for |

A watchface that builds part of its code as resources with a script of its
own, and its flags:

```cmake
set(blobs plan feast show)
list(TRANSFORM blobs REPLACE "(.+)" "${CMAKE_SOURCE_DIR}/resources/data/\\1_code.bin"
  OUTPUT_VARIABLE blob_outputs)
add_custom_command(
  OUTPUT ${blob_outputs}
  COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/build_blob.py --cc ${CMAKE_C_COMPILER}
  DEPENDS ${blob_sources}
  VERBATIM
)

pebble_app(SOURCES src/c/main.c)
target_compile_options(app INTERFACE -ffp-contract=off -flto)
target_link_options(app INTERFACE -flto -Os -Wl,-u,__pbl_app_info)
```

Projects still on `appinfo.json` move to `package.json` first, with
`pebble convert-project`.

### What changes

- An oversized `menuIcon` or `publishedMedia` image is a warning. waf
  compared the width and height as a pair, so it let most oversized images
  through and refused a few.
- A package whose resource has platform variants (`icon~color.png`)
  packages; waf failed to.
- `capabilities` in `appinfo.json` are sorted.
- Source maps name package JS by where it was unpacked, in the build
  directory.

Otherwise the pbw is the same: the projects used to test the port built to
the same binaries, resource packs and `appinfo.json` with both.
