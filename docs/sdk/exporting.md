# Exporting a function

Exporting a function makes it callable from third-party apps. The function
lives in `fw/applib/`, and three things must change together; the firmware
build alone does not surface it to apps:

1. **Implement the applib wrapper and syscall** — add the function to the
   appropriate `fw/applib/.../<area>.c/.h`, declare the `sys_*` syscall in
   `fw/syscall/syscall.h`, and define it with `PBL_SYSCALL` (see
   [](#defining-the-syscall)), either in the matching
   `fw/syscall/<area>_syscalls.c` or alongside the implementation it
   wraps.
2. **Register the symbol** in
   `tools/generate_native_sdk/exported_symbols.json` under the matching
   group, with an `addedRevision` matching the new SDK revision.
3. **Bump the SDK revision** in
   `fw/process_management/pebble_process_info.h`: increment
   `PROCESS_INFO_CURRENT_SDK_VERSION_MINOR` and add a comment line above the
   `#define` following the existing pattern, e.g. `// sdk.major:0x5
.minor:0x66 -- <description> (rev 105)`. The `rev` number in the comment
   must match `addedRevision` from step 2.

Forgetting steps 2 or 3 means the function compiles into the firmware but is
invisible to the app SDK build, so third-party apps can't link against it.

```{warning}
It is **not** possible to add publicly exposed functions to an already
released firmware/SDK combination. The generated `pebble.auto.c`
function-pointer table must be compiled into the firmware the SDK targets:
an app built against a newer SDK calls a trampoline that indexes past the
end of an older firmware's table and crashes. New exports always ship as a
new firmware plus a new SDK build.
```

(defining-the-syscall)=

## Defining the syscall

`PBL_SYSCALL` (`include/pbl/kernel/syscall.h`) takes the kind of each
argument, and checks the arguments before the body runs. An argument that
fails its check terminates the app:

```c
PBL_SYSCALL(int, persist_read_data, (VAL, const uint32_t, key),
            (OUT_BUF, void *, buffer, buffer_size), (VAL, const size_t, buffer_size)) {
  // buffer is known to lie in the caller's memory here
}
```

| Kind | Check |
|---|---|
| `VAL` | none, passed through |
| `IN`, `IN_OPT` | copied in; the body gets a kernel copy |
| `OUT`, `OUT_OPT`, `INOUT`, `INOUT_OPT` | copied in, then back out after the body returns |
| `IN_BUF`, `OUT_BUF` | `[ptr, ptr + len)` in the caller's memory; not copied |
| `IN_ARRAY`, `OUT_ARRAY` | `count` elements in the caller's memory; not copied |
| `STR`, `STRN` | NUL-terminated within the caller's memory (`STRN`: at most `max` characters); not copied |

A syscall that never returns uses `PBL_SYSCALL_NORETURN(name, args...)`.
`_OPT` kinds accept `nullptr`. Copied arguments are capped at
`PBL_SYSCALL_COPY_MAX` bytes, since they live on the syscall stack.
Privileged callers skip all checks and get the pointers they passed. Checks
no kind covers, such as whether a session belongs to the caller, go in the
body, which gets `bool from_user`: true for a checked call from unprivileged
code.

The syscalls listed in `fw/syscall/legacy_syscalls.txt` predate
`PBL_SYSCALL` and still check their arguments by hand with
`DEFINE_SYSCALL`. The firmware build rejects any other syscall defined that
way; a converted syscall must also be removed from the list.

### Fuzzing

Every syscall is fuzzed. `CONFIG_SYSCALL_FUZZ` selects the kernel's syscall
descriptors ({doc}`../kernel/syscalls`), so `PBL_SYSCALL` also records the
syscall and the kinds of its arguments, and the Syscall Fuzz app
(`fw/apps/demo/syscall_fuzz`) calls each one unprivileged: once with valid
arguments, then once per argument and bad value. Pointer arguments are given
addresses outside the app (kernel RAM, the syscall stack, flash, peripherals,
`nullptr`, ranges that straddle or wrap the app's memory) and must be
rejected; other values must leave the kernel running and its memory intact.

`system/test_syscall_fuzz.py` runs it on QEMU (`qemu_flint` for the ARMv7-M
MPU, `qemu_emery` for ARMv8-M) and fails if any syscall in the image was not
fuzzed. A new syscall needs nothing more, unless it takes an app callback
that the system calls later: give it a `RULE_CALLBACK` in the app, so that
the fuzzer passes a valid one.

## `exported_symbols.json`

```json
{
  "revision": "<exported symbols revision number>",
  "version": "x.x",
  "files": ["<files to parse>"],
  "exports": ["<symbols to export>"]
}
```

Each exported symbol has a `type` of `function`, `define`, `type`,
`forward_struct`, or `group`:

```json
{
  "type": "function",
  "name": "<symbol name>",
  "sortName": "<sort order>",
  "addedRevision": "<revision number>"
}
```

A `group` nests further `exports` under a `name`. Functions support
additional flags (`internal`, `removed`, `deprecated`, `appOnly`,
`workerOnly`, `implName`, `skipDefinition`) — see existing entries and
`tools/generate_native_sdk/exports.py` for their meaning.

Notes:

- Functions are sorted by `addedRevision`, then alphabetically (by `sortName`
  if present, else `name`) within a revision. This ordering is the ABI — it
  is why new functions must use a new revision: it guarantees new firmware
  stays backwards compatible with apps compiled against an older
  `libpebble.a`.
- `types` are emitted in the order listed; put typedefs after the typedefs
  they depend on (`includeAfter` is the escape hatch for ordering
  exceptions).
- The generator errors out on exports it cannot find in the parsed headers
  and on inconsistent revision numbers, but it does not verify that the
  resulting `pebble.h` compiles — review its output.
- The comment ledger above `PROCESS_INFO_CURRENT_SDK_VERSION_MINOR` is the
  only mapping between SDK revisions and version minors: no formula relates
  them (the minor once jumped `0x19` → `0x20` between revs 35 and 36, and a
  few minors and revs are skipped or doubled up). Treat the ledger as
  append-only history.
