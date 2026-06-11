# Native platform-agnostic host tests

## Goal

Host unit tests — those compiled for and run on the developer's own machine
rather than on a target board or under QEMU — should produce identical
results on macOS and Linux without requiring a container or cross-compilation
environment. A developer running `./waf test` on an Apple silicon MacBook and
a colleague running the same command on an Ubuntu x86-64 box should see the
same pass/fail outcomes, the same assertion values, and the same binary output
from every test that exercises firmware logic.

That goal has two parts. The first is eliminating host-toolchain divergence:
differences in how macOS Clang and Linux GCC/Clang handle tentative
definitions, floating-point contraction, and excess precision. The second is
eliminating host-libc divergence: differences in how Apple's libm and glibc
implement functions such as `log` and `pow` that affect correctness assertions
in tests of pblibc's own implementations.

## Work landed in this track

Five changes narrowed the gap between hosts.

`-fno-common` was added to the host-test CFLAGS. macOS ld64 silently merges
tentative definitions (bare enumerations or object declarations at file scope
in a header included by more than one translation unit), whereas GNU ld rejects
the resulting duplicate symbol. Without `-fno-common` a collision that is
harmless on macOS is a linker error on Linux, meaning some bugs only surface
on one host. With the flag, both toolchains reject the collision at compile
time, so the error appears on whichever host runs first.

`pblibc_private.h` include ordering was fixed in `src/libc/math/floor.c`.
`pblibc_private.h` emits `#undef floor; #define floor pblibc_floor` so that
the function definition below it provides the pblibc implementation rather
than shadowing the system symbol. On glibc, if `<math.h>` is included *after*
`pblibc_private.h`, glibc sees `floor` already redefined when it tries to emit
its `__DECL_SIMD_*` SIMD-variant declarations, and the translation unit fails
to compile. The fix is to place all system headers (`<math.h>`, `<stdbool.h>`,
`<stdint.h>`) before `pblibc_private.h`, which is the pattern already used
by `pow.c`, `scalbn.c`, and `sqrt.c`. With the include order corrected,
`test_floor.c` and `test_pow.c` were removed from `BROKEN_TESTS`.

Floating-point determinism flags were added to the host-test CFLAGS:
`-ffp-contract=off` and `-fexcess-precision=standard`. macOS Clang contracts
multiply-add pairs into FMA instructions by default; Linux Clang does not.
FMA changes intermediate precision and is the classic source of last-ULP
differences between platforms. `-ffp-contract=off` disables contraction on
both hosts so the same intermediate values are produced everywhere.
`-fexcess-precision=standard` is a no-op on SSE-only x86-64 but prevents
80-bit x87 register intermediates on any 32-bit or legacy host path. Together
these flags make floating-point computations deterministic across all supported
host toolchains.

The `test_log` and `test_pow` suites replaced their host-libm oracle pattern
with precomputed reference tables. Both tests previously called the host's own
`log` or `pow` at runtime to generate expected values, then compared pblibc's
result within one ULP. glibc and Apple's libm disagree in the last ULP on some
inputs, so the expected values were host-dependent and the tests were
inherently non-portable. The replacement approach evaluates each input offline
in Python's `decimal` module at 80 significant digits and rounds to the nearest
representable `double` (round-half-to-even), producing a correctly-rounded
reference that is the same on every host. The reference values and their
corresponding inputs are stored as IEEE-754 bit patterns in a checked-in
header, and the generator script is included so the table can be regenerated if
the test inputs change.

These five changes together remove the known sources of host-toolchain and
host-libm divergence from the currently active test suite. CI on both macOS
and Linux should now see identical outcomes.

## The remaining boundary: host-libc inclusion

The changes above are sufficient to make the existing tests deterministic, but
they do not sever the dependency on the host's standard library. Every host
unit test still compiles against the host libc and libm. That means:

- The `pblibc_private.h` rename shim is necessary. Without it, a test that
  includes `<string.h>` and then calls `memcpy` calls the host `memcpy`, not
  pblibc's. The shim papers over this by macro-renaming every covered symbol
  before the function definitions are reached.
- The `__DECL_SIMD_*` issue is structural, not just an include-order problem.
  Those macros appear because glibc injects SIMD-variant attribute declarations
  into its own `<math.h>` that assume the standard symbol names are available.
  As long as `<math.h>` must be included to satisfy other headers in the
  compilation unit, the tension between glibc's expectations and pblibc's
  renaming can recur.
- Any new pblibc function that needs to be tested — or any test that exercises
  a firmware module pulling in pblibc headers — risks the same class of
  collision unless every author remembers to get the include order right.

The root cause is that host tests live in two worlds simultaneously: they
include firmware headers that expect pblibc's symbols, and they compile against
a host libc that provides its own versions of those symbols. The shim bridges
the gap, but it is fragile and must be maintained as pblibc's symbol set grows.

## Next step: freestanding compilation against pblibc

The proper fix is to compile host unit tests under `-nostdinc`, with the
include search path limited to pblibc's own headers plus a small set of
compiler built-in headers, so the host libc is never in scope. Tests would
then see exactly the same symbols, with exactly the same definitions, as
firmware does. The `pblibc_private.h` rename shim becomes unnecessary because
there is nothing to rename away from: there is only one `memcpy`, and it is
pblibc's. The `__DECL_SIMD_*` corruption disappears because glibc's `<math.h>`
is never reached.

### Shape of the change

The change has three parts.

**A host-abstraction port.** A thin shim library — roughly a hundred lines of
C — provides the symbols that pblibc and clar need in order to run on a hosted
OS but that pblibc itself does not implement. Under firmware these come from the
RTOS or the hardware; under a freestanding host build they come from the shim.
The minimum set is:

- `write` (or equivalent) for clar's output. clar uses `printf` and
  `fputs`; those need to ultimately call the host's `write(2)`.
- `malloc` / `free` for clar's bookkeeping and for any test fixture that
  allocates memory. pblibc does not provide a heap allocator.
- `abort` for assertion failure.
- A clock source, if any test uses `time(3)` or `clock(3)`.
- `exit`, since clar calls it to report the final result.

Everything else — string functions, math functions, printf formatting — is
provided by pblibc and exercised by the tests rather than assumed from the
host.

**Freestanding CFLAGS for test compilation.** The waf test environment gains a
new flag set:

```
-nostdinc
-isystem <path-to-pblibc-include>
-isystem <path-to-compiler-clang-or-gcc-built-ins>
```

The compiler built-in headers (`stddef.h`, `stdint.h`, `stdarg.h`, `limits.h`,
`stdbool.h`) live in the compiler's own resource directory
(`clang -print-resource-dir` or `gcc -print-file-name=include`) and are
independent of the host libc. They are the only system headers that should
remain in scope. All of `<string.h>`, `<math.h>`, `<stdlib.h>`, `<stdio.h>`,
and `<unistd.h>` come from pblibc's `src/libc/include/` instead.

**Tests that cannot move.** Some tests have a genuine host dependency that
cannot be replaced by pblibc. Graphics snapshot tests compare pixel buffers
against PNG reference images and use libpng. Tests that exercise file I/O
(fixture loading, PFS tests) call POSIX file operations directly. Any test
that uses `popen`, `system`, or spawns subprocesses is similarly anchored to
the host. These tests should remain in a separate compilation group that keeps
`-nostdinc` off and continues to link the host libc. The goal is not to
eliminate that group but to shrink it to only tests that genuinely require
host facilities, so the boundary is explicit rather than accidental.

The libc/math tests — `test_log`, `test_pow`, `test_floor`, `test_round`,
`test_sqrt`, and so on — are the natural first target for the freestanding
group. They already include `pblibc_private.h` and have no POSIX dependencies.
The libc/string and libc/printf tests follow immediately behind them.

### Open questions

Several points need resolution before the migration can be completed.

Which libm-only symbols still need a host backing? pblibc covers the functions
that firmware uses, but `fesetround` and `fegetenv` (needed by `test_log` and
`test_pow` to set rounding mode for the suite setup) come from the host's
`<fenv.h>`. pblibc does not implement `<fenv.h>`. Either the rounding-mode
setup moves into the host-abstraction shim, or pblibc gains minimal fenv
stubs, or the tests stop calling `fesetround` and rely instead on the
determinism flags to guarantee round-to-nearest.

How does clar's `main` entry point interact with a freestanding build? clar
generates a `main` function that calls `exit`. A freestanding binary needs a
real `main` that the host linker can resolve, so clar's generated harness
continues to work as-is. The complication is that clar also uses `printf`,
`snprintf`, `fputs`, `fflush`, and `stderr`/`stdout`. `printf` and `snprintf`
are covered by pblibc's `vsprintf`. `fputs` and `fflush` are not; the
host-abstraction shim must provide them, or clar's print back-end must be
patched to call `write(2)` directly, bypassing stdio buffering.

Are there firmware modules whose headers pull in non-pblibc system headers
transitively? Some tests of non-libc firmware code include headers that
eventually reach `<sys/types.h>` or `<pthread.h>`. Those tests need either
a stub header placed ahead of the system path, or they remain in the hosted
group. A sweep of transitive includes across the tests directory is needed
before the split can be made cleanly.

### Why this is a separate piece of work

The freestanding migration is a larger undertaking than the changes landed in
this track. It touches the waf build system (new compilation group, new
toolchain flag set), the clar harness (print back-end), and potentially dozens
of test files that need their includes audited. It also requires a decision
about `fenv.h` before the math tests can fully move.

More importantly, it changes what "compiles" means for a host test, and any
mistake in the shim or the include path configuration will produce confusing
link errors or silent wrong behaviour. That risk is better managed as a focused
change with its own review, rather than bundled with the toolchain hygiene work
that preceded it.

The changes already landed establish the invariants that the freestanding
migration depends on: reference-table tests that do not call the host libm,
deterministic FP flags that remove cross-platform ULP disagreement, and a
correct include order in pblibc source files. The next step is to make those
invariants structural by removing the host libc from the picture entirely.
