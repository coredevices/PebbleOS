# Contribution Guidelines

Thank you for your interest in contributing to PebbleOS! We welcome contributions of all kinds, including bug fixes, new features, documentation improvements, and more. To ensure a smooth contribution process, please follow the guidelines outlined below.

See {doc}`../getting_started/index` to build the firmware and
{doc}`../development/emulation/emulator` to run it in QEMU.

## Developer Certification of Origin (DCO)

To make a good faith effort to ensure licensing criteria are met, this
project requires the Developer Certificate of Origin (DCO) process to be
followed.

The DCO is an attestation attached to every contribution made by every
developer. In the commit message of the contribution, (described more fully
later in this document), the developer simply adds a `Signed-off-by`
statement and thereby agrees to the DCO.

When a developer submits a patch, it is a commitment that the contributor has
the right to submit the patch per the license. The DCO agreement is shown
below and at http://developercertificate.org/.

```
Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

### DCO Sign-Off

The "sign-off" in the DCO is a "Signed-off-by:" line in each commit's log
message. The Signed-off-by: line must be in the following format:

```
Signed-off-by: Your Name <your.email@example.com>
```

For your commits, replace:

- `Your Name` with a known identity (sorry, no anonymous contributions.)

- `your.email@example.com` with the same email address you are using to
  author the commit (CI will fail if there is no match)

You can automatically add the Signed-off-by: line to your commit body using
`git commit -s`. Use other commits in the repository as examples.

Additional requirements:

- If you are altering an existing commit created by someone else, you must add
  your Signed-off-by: line without removing the existing one.

## Commit Requirements
- All commits must be atomic and self-contained. Each commit should represent a single logical change to the codebase. Avoid bundling multiple unrelated changes into a single commit.
- Each commit message must be in the format of "area: description", where "area" is a short identifier for the part of the codebase being changed (e.g., "services" or "applib") and "description" is a brief summary of the change.
- Each commit message must include the appropriate DCO sign-off as described above.
- If your commit addresses a specific issue, please include a reference to the issue number in the commit message (e.g., "Fixes #123").
- If your commit includes changes to documentation, please ensure that the documentation is updated accordingly and that the commit message reflects this.

## Pull Request Requirements
- Discuss large changes or new features with the maintainers before sending a big pull request.
- Changes impacting low-level components (e.g. hardware drivers, connectivity) are accompanied by
  proof of testing on real hardware, such as logs, measurements or captures. Testing on QEMU or the
  change compiling is not sufficient.
- Where applicable, changes show their impact on power and battery usage, measured with e.g. a power
  meter or detailed battery usage logs.
- User experience changes are accompanied by pictures or videos clearly showing the feature. When
  an existing feature changes, show it before and after the change.
- New behaviour comes with tests where feasible: unit tests, or integration tests for system-level
  behaviour. See {doc}`../development/tests/index`.
- Run the checks CI runs before submitting: `pbl test`, clang-format, ruff and gitlint. See
  {doc}`../development/conventions`.

## Generative AI usage
The use of large language models (LLMs) is acceptable **if and only if** a thorough manual review is performed. **By submitting a pull request, you are guaranteeing the following:**
- You have *personally* reviewed all generated code to make sure that it will not cause undue technical issues, including testing for edge cases, ensuring data integrity, and verifying that performance standards are maintained.
- You have ensured that PebbleOS's UX standards are not meaningfully degraded by this change.
- You have ensured that any changes to the user interface are in keeping with the design of the rest of the app, or have provided good reason to change them.
- You fully understand the content of the pull request and are prepared to explain it.
- You have verified that the change does not reduce overall code quality or maintainability.
- You own the rights to all content provided, or the license or attribution from the licensed content is properly included.

The following rules additionally apply to contributions made in whole or part using generative AI:
- The use of generative AI is disclosed with an `Assisted-by: LLM` trailer on every relevant commit,
  whatever the tool, model or vendor. Do not name them, and remove any trailer the tool adds by
  itself (e.g. `Co-Authored-By: <model> <vendor email>`).
- AI tools must not sign off in their own name. Only a human can certify the DCO: the
  `Signed-off-by` line is the author's own (e.g. via `git commit -s`), taking full responsibility
  for the change.
- Big pull requests are not allowed and will be ignored or closed, unless previously agreed with
  the maintainers. Reviewing big AI-assisted pull requests is time-consuming and not necessarily
  productive.
- You may review pull requests with AI assistance, but never publish the AI review comments. Keep
  them to yourself and push changes fixing the bugs you confirm.
- All submitted text is in English. We do not have the capacity to review text or translations for other languages at this time and we do not trust LLMs to handle this correctly.
- No AI-generated images, icons, SVG files, or other critically artistic works are included in the content of the pull request.

The code itself follows the {doc}`../development/conventions`.
