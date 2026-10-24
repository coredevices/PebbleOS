# Unit tests

Unit tests live under `tests/` and run on the host (not on device or QEMU;
for those, see [](integration.md)), using a vendored copy of the
[clar](https://github.com/clar-test/clar) test framework in `tools/clar/`. Code under test is compiled for the host together
with fakes and stubs that replace hardware and OS dependencies.

## Running tests

The tests are a CMake project of their own, separate from the firmware
build because they build for the host rather than for the watch. `pbl
test` configures `build-test/`, builds every test and runs them under
ctest:

```shell
pbl test
```

Anything `pbl test` does not recognise is passed straight to ctest, so
its selection and reporting options are all available:

- `-R REGEX`: run the tests whose name matches, e.g. `pbl test -R animation`
- `-L LABEL`: run by label; every test is labelled with its directory
  (`tests/fw/ui`) and its platform (`emery`, `gabbro`, `flint`)
- `-N`: list the tests that would run instead of running them
- `--rerun-failed`: re-run only what failed last time
- `-j N`: run N tests at once (the default is one per core)
- `--stop-on-failure`: stop at the first failure instead of running on

and `pbl test`'s own options:

- `-C` / `--coverage`: collect coverage and generate an lcov HTML report at
  `build-test/lcov-html/index.html`
- `--no-images`: skip the image fixtures, which only some tests need and
  which are the slow part of a clean build
- `--build-only`: build the test binaries without running them

Each test is a standalone binary under `build-test/`, so a failing one can
be run directly or under a debugger:

```shell
gdb build-test/fw/ui/test_animation/runme
```

Results are written as JUnit XML to `build-test/junit.xml`, and a
`compile_commands.json` for the tests lands in `build-test/`. The `Test`
GitHub Actions workflow (`.github/workflows/test.yml`) runs the whole suite
on pull requests that touch source, test or tooling paths.

Test binaries link against the DUMA memory checker by default, so
out-of-bounds heap accesses abort the test immediately.

## Writing a test

A test is a C file named `test_<suite>.c` under a `tests/` subdirectory,
plus a `pbl_clar_test()` entry in that directory's `CMakeLists.txt`. The clar
generator (`tools/clar/clar.py`) scans the file for functions named
`test_<suite>__<case>` — no manual registration is needed:

```c
#include <clar.h>

#include <pbl/crc/crc.h>

void test_crc32__initialize(void) {
  // optional: runs before each test case
}

void test_crc32__cleanup(void) {
  // optional: runs after each test case
}

void test_crc32__empty(void) {
  cl_assert_equal_i(pbl_crc32(0, NULL, 0), 0);
}
```

The available assertion macros (`cl_assert`, `cl_assert_equal_i`,
`cl_assert_equal_s`, `cl_must_pass`, ...) are defined in
`tests/test_includes/clar_asserts.h`.

The `CMakeLists.txt` entry declares which product sources, fakes and test
files to compile:

```cmake
pbl_clar_test(test_crc32)
```

`pbl_crc32()` lives in `libcrc`, one of the static libraries
`pbl_clar_test()` always links along with `libutil`, `libbtutil` and the
stubs library (see below). A test or stub defining one of their functions,
weak or not, replaces the library copy, since the linker only pulls an
archive member for undefined symbols.
Code that is not part of the always-linked libraries — and any fakes from
`tests/fakes/` — is listed explicitly under `SOURCES`, as paths relative to
the repository root:

```cmake
pbl_clar_test(test_pfs
  MODULES pfs
  SOURCES
    fw/system/hexdump.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)
```

Sources that travel together are grouped into modules in
`tests/modules.cmake` and pulled in by name with `MODULES`. A module may
`REQUIRES` others, whose sources come along:

```cmake
pbl_test_module(settings
  REQUIRES pfs
  SOURCES
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
)
```

A module's sources are compiled with the flags of the test using it, so
platform expansion and header overrides apply as if they were listed under
`SOURCES`.

`pbl_clar_test()` (defined in `cmake/modules/pbl_test.cmake`) builds one
binary per test file/platform combination and registers it with ctest. Its
arguments:

- `SOURCE`: the test file, when it is not `<name>.c` next to the
  `CMakeLists.txt`
- `MODULES`: groups of sources declared with `pbl_test_module()`
- `SOURCES`: code under test plus any fakes from `tests/fakes/`;
  `@PLATFORM@` and `@BITDEPTH@` in a path expand per platform
- `PLATFORMS`: build the test once per listed platform (e.g.
  `emery gabbro`); the default is a single generic platform
- `DEFINES`, `INCLUDES`, `LIBS`: extra compile defines, include paths and
  link libraries
- `OVERRIDES`: opt-in header override directories (see below)
- `DEPENDS`: targets to build first, for a test that needs a generated
  header
- `TEST_IMAGES`: the test renders against the image fixtures

A source needed by several tests is compiled once and shared between them
whenever they agree on every compile flag, so adding a test that matches an
existing one costs a single translation unit.

## Fakes, stubs and header overrides

These mechanisms replace dependencies of the code under test:

- **Fakes** (`tests/fakes/`): functional host implementations, e.g.
  `fake_rtc.c`, `fake_spi_flash.c`. Compile them in via `sources_ant_glob`.
- **Stubs** (`tests/stubs/default/`): no-op implementations of
  dependencies the test never exercises, archived into a
  `pbl_test_stubs_*` library compiled with each test's own flags and
  linked into every test. Every definition there is weak, so a test or fake
  providing its own copy takes precedence. The library is linked ahead of
  `libutil`, `libbtutil` and `libcrc`, so it must not stub their functions
  or it would shadow them; the assert stubs are linked as an object so they
  also win over the weak `util_assertion_failed()` in `libutil`. A test
  reaching into a stub's state, e.g. `s_syscall_did_fail`, includes the
  matching `tests/stubs/stubs_*.h` for its declaration. The few stubs left
  outside the library, e.g. `stubs_uuid.h` for `libutil` or the
  `tests/stubs/stubs_*.c` sources, are included or listed by the tests
  that need them.
- **Spies** ([FFF](https://github.com/meekrosoft/fff), in
  `third_party/fff/fff`): a test checking how its code calls a dependency
  declares a fake with `FAKE_VOID_FUNC()` or `FAKE_VALUE_FUNC()` instead of
  hand-writing counters and argument copies. Each fake records its
  `call_count` and arguments (`argN_val`, `argN_history`) and returns
  `return_val`, or runs `custom_fake` when it must write through a pointer or
  copy something off the caller's stack. Being a strong definition, it
  replaces the default stub. List the fakes once and reset them in
  `initialize()`:

  ```c
  #include <fff.h>

  FAKE_VOID_FUNC(settings_file_close, SettingsFile *);
  FAKE_VALUE_FUNC(status_t, settings_file_open, SettingsFile *, const char *, int);

  #define FAKES(FAKE) FAKE(settings_file_close) FAKE(settings_file_open)

  void test_foo__initialize(void) {
    FAKES(RESET_FAKE);
    FFF_RESET_HISTORY();
  }
  ```

- **Header overrides** (`tests/overrides/`): directories mirroring the
  source tree whose headers shadow the real ones on the include path.
  `tests/overrides/default/` is always active for every test; other
  directories are opted into per test via `override_includes`. Read
  `tests/overrides/README.md` and `tests/overrides/default/README.md`
  before adding one — in particular, avoid adding to `default/`.

## Resource snapshots

Tests that load system resources, e.g. to draw text, read each platform's
pack from `tests/fixtures/resources/system_resources_<platform>.pbpack`,
with matching `resource_ids.auto.h` and `resource_version.auto.h` under
`tests/overrides/default/resources/`. These are copies of firmware build
outputs, so after changing anything in `resources/`, regenerate them and
then the reference images that change:

```shell
tools/test_resource_snapshots.py
```

It configures each board in a temporary directory, so it needs the firmware
toolchain. CI runs it with `--check` and fails if the copies are out of date.

## Broken tests

`pbl_test_broken()` in `tests/CMakeLists.txt` lists tests that are not
built. Their `pbl_clar_test()` declarations stay where they are, so what
the test needs is still on record when someone picks it back up.
