# Compiler abstraction

`pbl/kernel/compiler.h` is the only place the tree may spell compiler
specifics: attributes (`PBL_PACKED`, `PBL_WEAK`, `PBL_NAKED`,
`PBL_SECTION()`, ...) and builtins (`PBL_LIKELY()`, `PBL_RETURN_ADDRESS()`,
`PBL_CLZ()`, ...). It only covers what C23 does not: use `[[noreturn]]`,
`[[maybe_unused]]`, `[[deprecated]]`, `unreachable()` and
`ckd_add()`/`ckd_mul()` directly. Every public macro is declared and documented once in the
frontend and expands to a `*_IMPL` counterpart from the backend selected by
the predefined macros: `compiler/gcc.h` for GCC and `compiler/clang.h` for
Clang, which reuses the GCC definitions and blanks the attributes Clang does
not implement. Supporting another compiler means adding a backend that
defines the same `*_IMPL` set and a branch in the frontend; callers stay
untouched. The unit-test hooks built on these (`PBL_T_STATIC`,
`PBL_T_MOCKABLE`) live in `pbl/util/testing.h`. Code outside
`include/pbl/kernel/compiler/` must not use `__attribute__` or `__builtin_*`
directly. Exported headers follow the same rule: the SDK generator rewrites
the macros into their GCC spellings, so the SDK ships no firmware headers;
see {doc}`../sdk/building`.
