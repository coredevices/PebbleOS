---
name: pr-review
description: Use when reviewing a PebbleOS pull request (number, URL or branch). Checks contribution policy first (big AI-assisted PRs, hardware proof, UX media, tests), runs a security analysis on syscall changes, then reviews the code.
---

# Reviewing a PebbleOS pull request

The rules being enforced live in `docs/community/contributing.md` and
`docs/development/conventions.md`; read them before starting. This skill
says how to apply them, and what maintainers repeatedly catch in review.

## 1. Gather

```sh
gh pr view N --json number,title,author,body,labels,files,additions,deletions,commits,isDraft
gh pr diff N
gh pr view N --comments            # earlier review rounds, author replies
gh api repos/coredevices/pebbleos/pulls/N/comments   # inline threads
```

Maintainers are the owners in `.github/CODEOWNERS`.

## 2. Gate: big AI-assisted pull requests

Before reading the code, decide whether the PR is **big**, **AI-assisted**
and **not authored by a maintainer**:

- AI-assisted: an `Assisted-by: LLM` trailer, a leftover tool trailer
  (`Co-Authored-By: <model>`), a "Generated with ..." line in the body, or
  the usual tells (verbose comments restating the commit message, a long
  templated PR body, many near-duplicate code paths).
- Big: more than 1000 changed lines (ignore goldens, generated files and
  vendored code), or changes to multiple unrelated areas.
- Check the PR and linked issues for prior agreement with a maintainer.

If all three hold and there is no prior agreement, **stop**. Report the
size, the AI signals and the policy line ("Big pull requests are not
allowed ... unless previously agreed with the maintainers"), and ask the
user whether to review anyway. Only continue if they say so.

## 3. Policy checks

Report each missing item; none of these needs reading the code.

- **Hardware proof** for low-level changes: anything matched by
  `pending:hw-validation` in `.github/labeler.yml` (drivers, kernel, soc,
  Bluetooth, HALs, NimBLE), plus power management, flash/storage and
  connectivity code elsewhere. Logs, scope/logic captures or measurements
  from a named board. QEMU runs or "it builds" do not count.
- **Power impact** where the change can affect it (sleep, wake-ups,
  radios, timers, sensors, display refresh): numbers from a power meter or
  battery logs, ideally A/B against `main`.
- **UX media**: user-visible changes carry screenshots or a video; changes
  to an existing screen show before and after. Check every affected shape
  (rect/round, colour/B&W, touch/buttons) is covered or argued
  irrelevant. No AI-generated images, icons or SVGs.
- **Tests**: new behaviour has unit tests or integration tests
  (`docs/development/tests/`). Test the API, not the implementation.
- **Commits**: `area: description` matching `git log` for those files;
  one logical change each, bisectable; no fixups or reverts of something
  added in the same PR; `Signed-off-by` matching the author; `Assisted-by:
  LLM` where AI was used and no tool/vendor names. Run
  `gitlint --commits upstream/main..HEAD` on the checked-out branch.
- **Scope**: unrelated drive-by changes split out; English only.
- **Cross-repo**: changes needing a phone (CoreApp) side or a submodule
  PR link to it; submodule SHAs point at merged commits.

## 4. Syscall and trust-boundary security analysis

Run this whenever the PR adds or changes a `PBL_SYSCALL`/`DEFINE_SYSCALL`,
an applib function that reaches the kernel, `exported_symbols.json`, or
code parsing data from the phone/BLE. Read `docs/sdk/exporting.md` and
`include/pbl/kernel/syscall.h` first.

Syscalls — assume the caller is a hostile app or watchface:

- Every pointer argument uses the right kind (`IN`/`OUT`/`*_BUF`/
  `*_ARRAY`/`STR`/`STRN`); nothing is declared `VAL` that is then
  dereferenced. Lengths and counts come from the same checked argument.
- New syscalls use `PBL_SYSCALL`, never `DEFINE_SYSCALL` (only the ones in
  `fw/syscall/legacy_syscalls.txt` may).
- Non-copied buffers (`*_BUF`, `*_ARRAY`, `STR`) are app memory the app
  can change: no check-then-use on their contents (TOCTOU); copy first.
- Ownership checks the kinds cannot express go in the body under
  `from_user`: handles, sessions and IDs belong to the caller, the caller
  is in the foreground when that matters.
- Enum and index arguments are range-checked, including values the
  internal API accepts but apps must not (e.g. a new enum value placed
  before `...Count` silently becoming app-reachable).
- No `PBL_ASSERT`/`PBL_ASSERTN` reachable from app input: an app must not
  be able to reset the watch.
- Integer overflow in size arithmetic; `int16` coordinate wraparound in
  rects/points from app memory.
- Nothing kernel-private leaks back: kernel pointers, uninitialised
  padding in `OUT` structs, other apps' data.
- Resource exhaustion: kernel heap allocations, timers, flash writes or
  events an app can trigger in a loop; freed on every stop/kill path.
- Blocking, locks and flash I/O inside the body, and on which task.
- SDK side: `exported_symbols.json` revision bumped, declarations still
  C99, `pending:sdk-review` applies.

Phone/BLE input: records, blob DB entries and protocol messages are
range-checked before they reach cron, UI, persistence or array indexing;
size checks alone are not enough.

## 5. Code review

Check out the branch (use a git worktree; the main tree may be switching
branches), build the affected board and run the affected tests. Then look
for what review rounds on this repo keep finding:

- **Concurrency**: lock-order inversions (including `bt_lock`, driver
  mutexes), `event_put()` or blocking sends while holding a lock, heavy
  work on timer tasks, state published before it is fully set up, unlocked
  reads of shared state.
- **Lifecycle**: every exit path (stop, app kill, disconnect, phone app
  death, failed transfer, timeout) releases what start acquired; stale
  callbacks/session IDs from a previous run are rejected.
- **Persistence and sync**: persisted or synced structs have a fixed
  layout (`uint8_t` not enums/`bool`); migrations run once, handle default
  and partially customised values, and don't change semantics silently;
  legacy keys stay in sync in both directions with old phone apps; flash
  wear from frequent writes.
- **Compatibility**: old apps (SDK revisions, legacy framebuffer sizes),
  old phones, older firmware peers; capability bits gate new protocol
  fields.
- **Platforms**: B&W vs colour (`PBL_COLOR`/`PBL_BW`), round displays,
  rotation, touch, boards without the hardware (Kconfig capabilities, not
  stubs or `CONFIG_BOARD_*` checks).
- **Cost**: permanently resident RAM (`.bss`) for features that are
  rarely active, stack use on small task stacks, extra flash reads on hot
  paths, scans on every DB change.
- **Error handling**: `int 0/-errno` with early return; errors mapped to
  the right public meaning (a transient `ENOMEM` is not "unsupported");
  retries have backoff or a cap.
- **Dead and duplicate code**: APIs left with only test callers, unused
  resources, chains no longer invoked, the same gate or state machine
  written twice.
- **Product fit**: duplicates an existing setting, or a design maintainers
  already prefer; new settings default to the old behaviour.
- **i18n**: user-visible strings reachable by extraction (`i18n_noop`),
  `strncpy` with translated text NUL-terminated.
- **Tests**: catch the bug they claim to (no pass-by-construction, e.g.
  swapped `memset` arguments); cover the case actually fixed, not a
  neighbouring one.
- **Style**: comments short, no rationale that belongs in the commit
  message, no issue references in code; naming per conventions; logging
  levels per `CLAUDE.md`.

## 6. Verify, then report

Only report findings you can back with a concrete scenario: inputs or a
sequence of events, and what goes wrong. Reproduce where cheap (unit test,
QEMU, a quick patch). Drop anything speculative.

Report to the user first. Post to the PR only when the user asks, and
only the findings they kept: the comments are for the author, written as
the user's own review. Never reply to other AI review comments or bot
threads on the PR, and never post on your own pull request; there, fix
the confirmed bugs instead.

- One summary line: verdict, and which findings block merging.
- Policy gaps from section 3.
- Findings ranked most severe first, each with `file:line`, the scenario
  and a suggested fix. Mark minor ones `nit:`; mark issues the PR did not
  introduce as pre-existing.
- On a re-review, go through each earlier thread and say whether it is
  fixed in the new head, quoting the commit SHA reviewed.
