# PebbleOS

PebbleOS is the operating system running on Pebble smartwatches.

## Organization

- `docs`: project documentation
- `python_libs`: tools used in multiple areas, e.g. log dehashing, console, etc.
- `resources`: firmware resources (icons, fonts, etc.)
- `sdk`: application SDK generation files
- `src`: firmware source
- `tests`: tests
- `third_party`: third-party code in git submodules, also includes glue code
- `tools`: a variety of tools or scripts used in multiple areas, from build
  system, tests, etc.
- `waftools`: scripts used by the build system

## Code style

- clang-format for C code
- ruff for Python code

## Firmware development

- Configure: `./waf configure --board BOARD_NAME`

  - Board names can be obtained from `./waf --help`
  - `--release` enables release mode
  - `--mfg` enables manufacturing mode
  - `--variant=normal|prf` selects build variant (default: normal)

- Build firmware: `./waf build`
- Run tests: `./waf test`

## Adding a new SDK function

When exposing a new function to third-party apps (i.e. anything declared
in an `applib/` header that user apps can call), three things must change
together — the firmware build alone won't surface it to apps:

1. **Implement the applib wrapper and syscall** — add the function to the
   appropriate `src/fw/applib/.../<area>.c/.h`, declare the syscall in
   `src/fw/syscall/syscall.h`, and define it with `DEFINE_SYSCALL` in
   `src/fw/syscall/syscall_<area>.c`.
2. **Register the symbol** in
   `tools/generate_native_sdk/exported_symbols.json` under the matching
   group, with an `addedRevision` matching the new SDK revision.
3. **Bump the SDK revision** in
   `src/fw/process_management/pebble_process_info.h`: increment
   `PROCESS_INFO_CURRENT_SDK_VERSION_MINOR` and add a `// sdk.major:0xN
   .minor:0xM -- <description> (rev <N+1>)` comment line above the
   `#define`. The revision number in the comment must match
   `addedRevision` from step 2.

Forgetting steps 2 or 3 means the function compiles into the firmware
but is invisible to the app SDK build, so third-party apps can't link
against it.

## Git rules

Main rules:

- Commit using `-s` git option, so commits have `Signed-Off-By`
- Always indicate commit is co-authored by the current AI model
- Commit in small chunks, trying to preserve bisectability
- Commit format is `area: short description`, with longer description in the
  body if necessary
- Run `gitlint` on every commit to verify rules are followed

Others:

- If fixing Linear or GitHub issues, include in the commit body a line with
  `Fixes XXX`, where XXX is the issue number.
