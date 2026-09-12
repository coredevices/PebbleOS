---
name: pr-review
description: Use when asked to review, triage, or risk-classify a PebbleOS pull request. Classifies the PR as pr:risk-low|medium|high, applies the label, and reviews with depth matched to the risk.
---

# PR review

Input: a PR number or URL (`/pr-review 1234`). All `gh` commands target
`coredevices/pebbleos`; pass `--repo coredevices/pebbleos` when the checkout's
`origin` is a fork.

## 1. Gather

```sh
gh pr view N --json number,title,body,author,assignees,baseRefName,labels,additions,deletions,changedFiles,commits
gh pr view N --json files -q '.files[] | "\(.additions)\t\(.deletions)\t\(.path)"'
```

Do not fetch the diff yet. The file list and line counts decide whether a
diff is read at all.

## 2. Classify

Evaluate the rules in order; the first match wins. A PR gets exactly one of
`pr:risk-low`, `pr:risk-medium`, `pr:risk-high`.

### High

- More than 100 changed files (reformatting, mass renames, tree moves).
  **Skip the review entirely**: label, and report only the file count and
  what kind of change it appears to be.
- Changed files in the hundreds, or a diff larger than 1000 lines
  (additions + deletions).
- Touches driver code: `src/fw/drivers/`, `soc/`, `boards/`,
  `third_party/hal_*`, or `src/fw/board/`.
- Touches the connectivity layer: `src/fw/comm/`, `src/bluetooth-fw/`,
  `src/fw/services/bluetooth/`, `src/fw/services/comm_session/`,
  `lib/btutil/`, `third_party/nimble/`, or anything else Bluetooth,
  PPoGATT, or GATT related.

### Medium

- Any third-party module update: a submodule pointer change under
  `third_party/`, or a vendored dependency bump (`requirements*.txt`,
  `flake.lock`, `.gitmodules`).
- Multiple areas changed. Areas are the top-level buckets: `src/fw/applib`,
  `src/fw/apps`, `src/fw/services`, `src/fw/kernel`, `kernel/`, `lib/`,
  `subsys/`, `sdk/`, `tools/`, `tests/`, `docs/`, `resources/`, `.github/`,
  plus the build system (`CMakeLists.txt`, `cmake/`, `Kconfig`,
  `prj*.conf`). A PR whose files land in two or more areas is medium, even if
  each part is small.
- Anything that is not clearly low.

### Low

- A simple fix whose commit message or PR body explains the root cause and
  why the change is correct. A one-line fix with no reasoning is medium.
- Documentation-only changes, including source code comments and strings.
- A new, small, self-contained UX feature: one app or one UI component, a
  short diff, no new services or driver work.

Tests alongside the change do not count as a second area. CI-only changes
(`.github/`) are one area.

## 3. Label

Apply exactly one label, replacing any stale one:

```sh
gh pr edit N --add-label pr:risk-X --remove-label pr:risk-Y --remove-label pr:risk-Z
```

## 4. Review

Depth follows the risk. Always state the classification and the rule that
triggered it first.

- **>100 files**: no review. Report the count, the apparent nature of the
  change, and whether it looks mechanical. Stop.
- **Low**: read the diff once. Confirm the change does what the description
  says and that commit format follows `docs/development/contributing.md`
  (`area: short description`, `Signed-off-by`, AI co-author trailer when
  applicable). Report a one-paragraph verdict.
- **Medium**: read the full diff. For a submodule bump, run
  `git diff <old>..<new> --stat` inside the submodule and list what changed
  in the parts PebbleOS uses. For multi-area PRs, check that each area's
  change is in its own commit and bisectable. Look for correctness issues
  and for glue code affected by an updated dependency.
- **High**: full review. Read every changed file in context, not only the
  hunks. For drivers, check register sequencing, error paths, locking, and
  power states, and note which boards are affected (`boards/*`). For
  connectivity, check state machines, connection lifecycle, buffer
  ownership, and iOS/Android behaviour differences. Build the affected
  boards with `pbl configure --board B && pbl build`, run `pbl test` when
  tests are touched, and say which boards you built. Flag that on-device
  validation is required and suggest `pending:hw-validation` when the PR
  has no evidence of it.

Findings ranked most severe first, each with file:line, what fails, and how
to trigger it. Do not pad with style nits.

## 5. Report

Print the full report to the user, then post a final comment on the PR with
`gh pr comment N --body-file`:

- First line: the risk label and the rule that triggered it.
- A short summary of what the PR does and the review highlights: the
  findings, ranked as above, or a statement that nothing was found. For a
  skipped (>100 files) review, say so and give the file count.
- If anything deserves special attention (a correctness finding, a missing
  hardware validation, a submodule bump touching code PebbleOS uses, a
  bisectability problem), open the comment by tagging the PR assignees
  (`@login` from the `assignees` field) and say why in one sentence. Do not
  tag anyone for a clean low-risk review.

The comment must disclose the AI model that performed the review, e.g. a
closing line "Reviewed by Claude Fable 5.1 via the pr-review skill."

Never approve or request changes; the comment is the deliverable and the
decision stays with the maintainers.
