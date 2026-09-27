```{include} ../../CONTRIBUTING.md

```

## Repository conventions

- C code is formatted with clang-format, Python with ruff.
- Keep code comments short and concise; extended rationale belongs in the Git
  commit message. Do not reference issue numbers in code, only in commit
  messages.
- To backport a merged pull request to a release branch, add a
  `backport-<branch>` label (e.g. `backport-4.36-branch`) to it, either
  before or after merging. A workflow cherry-picks the pull request's commits
  onto `<branch>` and opens a new pull request against it; on conflicts it
  comments on the original pull request instead. The workflow authenticates
  as a GitHub App (`BACKPORT_APP_ID` / `BACKPORT_APP_KEY` repository secrets)
  so that backport pull requests trigger CI, which the default
  `GITHUB_TOKEN` cannot do.
