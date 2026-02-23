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

## Firmware development

- Configure: `./waf configure --board BOARD_NAME`

  - Board names can be obtained from `./waf --help`
  - `--release` enables release mode
  - `--mfg` enables manufacturing mode
  - `--qemu` enables QEMU mode

- Build main firmware: `./waf build`
- Build recovery firmware (PRF): `./waf build_prf`
- Run tests: `./waf test`

## Git rules

Main rules:

- Commit using `-s` git option, so commits have `Signed-Off-By`
- Always indicate commit is co-authored by Claude
- Commit in small chunks, trying to preserve bisectability
- Commit format is `area: short description`, with longer description in the
  body if necessary
- Run `gitlint` on every commit to verify rules are followed

Others:

- If fixing Linear or GitHub issues, include in the commit body a line with
  `Fixes XXX`, where XXX is the issue number.
