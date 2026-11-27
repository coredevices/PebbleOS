# Patches to the upstream bindings

Patches to the upstream bindings in `devicetree-rebasing`, next to this directory,
for what they lack: a compatible, a property. One patch per change, as mails
`git format-patch` writes, applied in the order of their names, which number
them:

```
0001-dt-bindings-arm-cpus-Add-Cortex-M33.patch           submitted
0002-dt-bindings-arm-cpus-Add-Arm-China-STAR-MC1.patch   submitted
0003-dt-bindings-arm-cpus-Add-Cortex-M7.patch            not submitted yet
```

Each carries its message: the one it was submitted with, or the one it is to be
submitted with. A posted patch is taken as posted, from the commit sent; one not
submitted yet has no `Signed-off-by`, which its author adds when sending it. Its
diff, against the bindings directory, names the bindings it patches. A new patch
takes the next number, after the patches to the same binding it builds on.

`patches.yaml` classifies every patch, by its name:

| Key | Values |
| --- | --- |
| `type` | `temporary`, to go upstream and be dropped once there, or `permanent` |
| `status` | For a temporary one: `not-submitted`, `submitted`, `applied` (to a maintainer tree) or `merged` (in a release the submodule is not on yet) |
| `link` | Once submitted: the link to the latest submission |
| `reason` | For a permanent one: why upstream will never take it |
| `note` | Optional: the review state |

Taking a submitted patch as posted lets the check tell, once upstream has it,
that it is to be dropped rather than rebased.

`tools/devicetree/validate.py` applies the patches to its copy of the upstream
bindings. It fails on a patch missing from `patches.yaml` or an entry without a
patch, on one without a message, on a patch that no longer applies, to rebase,
and on one upstream took, to drop.
