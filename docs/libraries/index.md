# Libraries

Libraries are generic code under `lib/`, with their API in `include/pbl/`.
They depend on no OS service, so the kernel, subsystems, drivers and
applications can all use them, and so can the bootloader, host tools and
unit tests, which link them directly. A library is built when the
application enables it, or when the code that uses it selects it.

```{toctree}
:maxdepth: 1

c.md
util.md
fuel_gauge/index.md
kraepelin.md
vanhees.md
shared.md
```
