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

## Naming

Functions and types are prefixed by who may use them:

- Public API, declared in `include/pbl/`, takes `pbl_` and the name of its
  module: `pbl_bt_start()`, `struct pbl_bt_addr`.
- Functions shared between the files of a module but private to it,
  declared in a header inside the module, take a double underscore after
  the module name: `pbl_bt__services_init()`.
- Functions local to a file are `static` and take `prv_`, and file-scope
  variables `s_`: `prv_comm_start()`, `s_comm_is_running`.

## Comments

Keep code comments short and concise; extended rationale belongs in the Git
commit message. Do not reference issue numbers in code, only in commit
messages.

## Git workflow

### Commits

Each commit is one logical change that builds and works on its own, so
the history stays bisectable. Split unrelated changes, and fold fixes to
a change into the commit that introduced it rather than adding fixup
commits.

The title has the form `area: description`:

- `area` names the part of the tree the change touches, in lowercase, as
  a path when that is clearer, e.g. `drivers/imu/mmc5603nj` or
  `include/pbl/kernel`. Follow what `git log` shows for the same files.
- `description` says what the change does, in the imperative, without a
  trailing period.

The body explains why the change is needed and anything a reviewer would
otherwise have to work out, wrapped at 100 columns like the title. An
issue the commit fixes is referenced there, as `Fixes #123`, never in the
code.

Every commit carries a `Signed-off-by` line, certifying the Developer
Certificate of Origin, that matches the commit's author; `git commit -s`
adds it. The author name must be a real name, and GitHub `noreply`
addresses are not accepted. Work produced with generative AI is disclosed
with an `Assisted-by: LLM` line, following the {doc}`../community/contributing`.

```text
services/battery: hide the percentage while charging starts

The fuel gauge reports a stale value for a few seconds after the charger
is connected. Show the charging icon alone until the first new reading
instead of a percentage that jumps.

Signed-off-by: Jane Doe <jane@example.com>
```

### Pull requests

Open pull requests from a branch of your fork against `main`. Keep the
branch rebased on `main` rather than merging it in, and rework commits
with interactive rebase and a force push when addressing review, so the
pull request always shows the history that will land. Pull requests are
merged without merge commits.

CI checks every commit of a pull request with gitlint, against the rules
in `.gitlint` and `tools/gitlint/`, besides building and testing it.

Labels are added from the paths a pull request touches. Some block
merging until they are removed:

- `DNM`: do not merge;
- `pending:hw-validation`: waiting for testing on real hardware;
- `pending:sdk-review`: waiting for a review of an SDK change;
- `pending:submodule-merge`: a submodule points at a commit not merged
  in its own repository yet.

A pull request with no activity for 30 days is marked stale, and closed
7 days later unless someone comments.

### Checking commits locally

Run gitlint over the commits of a branch before pushing:

```shell
gitlint --commits main..HEAD
```

### Backports

To backport a merged pull request to a release branch, add a
`backport-<branch>` label (e.g. `backport-4.36-branch`) to it, either
before or after merging. A workflow cherry-picks the pull request's commits
onto `<branch>` and opens a new pull request against it; on conflicts it
comments on the original pull request instead.

The workflow authenticates as a GitHub App (`BACKPORT_APP_ID` /
`BACKPORT_APP_KEY` repository secrets) so that backport pull requests
trigger CI, which the default `GITHUB_TOKEN` cannot do.
