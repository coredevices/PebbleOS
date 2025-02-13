# Contribution Guidelines

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

- `Your Name` with your legal name (pseudonyms, hacker handles, and the
  names of groups are not allowed)

- `your.email@example.com` with the same email address you are using to
  author the commit (CI will fail if there is no match)

You can automatically add the Signed-off-by: line to your commit body using
`git commit -s`. Use other commits in the repository as examples.

Additional requirements:

- If you are altering an existing commit created by someone else, you must add
  your Signed-off-by: line without removing the existing one.
