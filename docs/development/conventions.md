# Conventions

The conventions code in this repository follows. CI checks the ones a tool
can, on every pull request.

## License headers

Every new C, assembly, Python and shell source file starts with SPDX
headers naming its copyright holder and the Apache-2.0 license:

```c
/* SPDX-FileCopyrightText: $YEAR $AUTHOR */
/* SPDX-License-Identifier: Apache-2.0 */
```

CI rejects a pull request that adds one of these files without the license
identifier in its first ten lines. Files in submodules keep their own
headers.

## Formatting

C code is formatted with clang-format, following `.clang-format`; CI fails
on any code clang-format 22 would change, except the files listed in
`.clang-format-ignore`. Python code follows ruff, configured in
`.ruff.toml`; CI fails on anything `ruff check` reports.

## C language

Firmware and unit tests are C23:

- Use `bool`, `static_assert`, `alignas`, `typeof` and `nullptr` as
  keywords, without their headers.
- Zero-initialize with `{}`.
- Use `#elifdef` and `#elifndef`.
- Use the standard attributes, `[[fallthrough]]`, `[[noreturn]]` and
  `[[maybe_unused]]`, rather than `pbl/kernel/compiler.h` equivalents.

Declarations exported to the SDK must still build as C99, which apps use.

## Comments

Keep code comments short and concise; extended rationale belongs in the Git
commit message. Do not reference issue numbers in code, only in commit
messages.

## Backports

To backport a merged pull request to a release branch, add a
`backport-<branch>` label (e.g. `backport-4.36-branch`) to it, either
before or after merging. A workflow cherry-picks the pull request's commits
onto `<branch>` and opens a new pull request against it; on conflicts it
comments on the original pull request instead.

The workflow authenticates as a GitHub App (`BACKPORT_APP_ID` /
`BACKPORT_APP_KEY` repository secrets) so that backport pull requests
trigger CI, which the default `GITHUB_TOKEN` cannot do.
