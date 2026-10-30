# Syscalls

Threads can run unprivileged, confined by the MPU, and enter privileged code
only through syscalls. `include/pbl/kernel/syscall.h` is the interface;
`CONFIG_USERSPACE` turns it on, on architectures that select
`ARCH_HAS_USERSPACE` (Cortex-M).

## Defining a syscall

`PBL_SYSCALL()` defines the stub and its body in one go. Each argument is
declared with a kind that says how the stub checks it before the body runs:
passed through, copied in, copied in and back out, or a buffer, array or
string that must lie in the caller's memory. A caller that passes a bad
argument is killed. The body gets `bool from_user`, true when the call came
from unprivileged code, for checks no kind covers. {doc}`../sdk/exporting`
lists the kinds.

Bodies that check by hand use `pbl_user_access_ok()`,
`pbl_syscall_verify_user()`, `pbl_user_copy_from()`, `pbl_user_copy_to()`,
`pbl_user_strnlen()` and `pbl_syscall_fail()`.

## User threads

A thread's syscalls may only hand over memory between
`pbl_thread_attr.user_start` and `user_end`, and only above the stack
pointer the syscall was made from: below it lies the unused part of the
caller's stack and its guard. A thread created with a `syscall_stack` runs
syscall bodies there, so a thread that exhausts its own stack faults
unprivileged rather than in privileged code.

A thread created privileged drops to unprivileged mode for good with
`pbl_user_mode_enter()`. `pbl_user_call()` runs a callback unprivileged from
privileged code, for code that calls into untrusted callbacks.
`pbl_is_user_context()` tells whether the CPU runs unprivileged code.

## Hooks

What happens to a thread is left to the application:

- `pbl_kernel_syscall_exit()` runs on the way out of every syscall made
  from unprivileged code, and may hold the thread there.
- `pbl_kernel_syscall_fault()` kills the caller of a failed syscall.

## Descriptors

With `CONFIG_SYSCALL_DESC`, a hidden option for test tools to select, every
`PBL_SYSCALL()` also records its name and the kinds of its arguments as a
`struct pbl_syscall_desc` in `.syscall_desc`, with a wrapper that calls it
with arguments read from an array (`include/pbl/kernel/syscall_desc.h`). A
tool can then call every syscall in the image without a list of them.

Such a tool also needs to survive the calls it expects to be rejected.
`pbl_syscall_abort_to()`, called while a syscall fails, for example from
`pbl_kernel_syscall_fault()`, abandons the syscall and resumes the caller
unprivileged at a given function, on the stack it made the syscall from.

## Without `CONFIG_USERSPACE`

Every thread runs privileged, syscalls are plain calls with `from_user`
always false, `pbl_is_user_context()` is always false and the user-memory
helpers accept everything. Nothing then isolates one thread from another
or from the kernel.

## Cortex-M

`kernel/arch/arm/syscall.c` implements the stub, the privilege drop and
`pbl_user_call()`; {doc}`internals` describes the supervisor call.
