---
name: release
description: Use when cutting a PebbleOS release, either a new version from main or a fix release from a release branch (vX.Y-branch). Covers the hardware itest gate, tag naming, signed annotated tags and what CI does on tag push.
---

# Releasing PebbleOS

A release is a signed, annotated `v*` tag pushed to `coredevices/PebbleOS`
(the `upstream` remote). Pushing the tag is the release: `release.yml`
builds everything, publishes the GitHub release and uploads to eng-dash.
There is no version file to bump; the firmware version is `git describe`.

## Tagging scheme

| Release | Tagged from | Tag |
|---|---|---|
| New version | `main` | `vX.Y.0`, Y one above the latest `vX.Y.*` |
| Fix release | `vX.Y-branch` | `vX.Y.Z`, Z one above the latest on that branch |

Check the latest tags first:

```
git fetch upstream --tags
git tag --sort=-v:refname | head
```

## Where the release comes from

- **New versions are tagged from `main`**, never from a branch. The release
  job marks a release as GitHub's "latest" only when the tag is on `main`.
- **Fix releases are tagged from release branches** `vX.Y-branch`.
  - If the branch does not exist yet, create it from the release tag and
    push it upstream:
    `git push upstream vX.Y.0:refs/heads/vX.Y-branch`.
  - Fixes land on `main` first. They get to the branch as `cherry-pick -x`
    commits, ideally by putting a `backport-vX.Y-branch` label on the merged
    `main` PR. `backport.yml` then opens the backport PR.
  - Only fixes go on a release branch. Features do not.

## 1. Hardware integration tests must be green

Before tagging, run the hardware integration test workflow
(`hw-itest.yml`) on the exact commit you will tag, and wait until every job
passes. A green nightly run also counts if it tested the same SHA you will
tag.

```
gh workflow run hw-itest.yml -R coredevices/PebbleOS --ref main      # or vX.Y-branch
gh run list -R coredevices/PebbleOS --workflow hw-itest.yml -L 3     # find the run
gh run watch -R coredevices/PebbleOS <run-id> --exit-status
```

- Check that the run's `headSha` is the commit you will tag. If the branch
  moves while the tests run, run them again or tag the commit that was
  tested.
- Any failure stops the release. Find out why before tagging. Do not rerun
  until it goes green. A flake you have already seen elsewhere is the one
  exception, and you have to say so.

## 2. Tag

Tags must be **annotated and signed**. The message is the tag name.

```
git fetch upstream
git tag -s vX.Y.Z -m vX.Y.Z <tested-sha>
git verify-tag vX.Y.Z
git push upstream vX.Y.Z
```

- Tag the tested SHA explicitly, not whatever a local branch points to.
- Never move or delete a tag after you push it. If something is wrong, make
  a new release.
- Pushing a tag cannot be undone. Get the user's confirmation before
  pushing.

## 3. After the tag push

- Watch the `Release` workflow:
  `gh run list -R coredevices/PebbleOS --workflow release.yml -L 1`. It
  builds the firmware, the PRF, the QEMU and native builds, then makes the
  GitHub release with all artifacts and publishes to eng-dash.
- Check that the GitHub release has the files and that "latest" is set only
  for releases from `main`.
- Post release notes to Slack `#releases` when the user asks:
  - The main message has simple notes for users.
  - The technical notes go in a thread reply.
  - Draft both from `git log <previous-tag>..vX.Y.Z`.
