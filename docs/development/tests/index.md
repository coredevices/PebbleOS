# Testing

PebbleOS has two test suites, which check different things:

| | Unit tests | Integration tests |
| --- | --- | --- |
| What | Individual modules, against fakes and stubs | The whole firmware, as it ships |
| Where | On the host | On the emulator, the native simulator or a watch |
| Written in | C, with [clar](https://github.com/clar-test/clar) | Python, with [pytest](https://docs.pytest.org) |
| Lives in | `tests/` | `tests/integration/` |
| Run with | {doc}`../../pbl/test` | {doc}`../../pbl/itest` |

Unit tests are fast and precise: they build a few firmware sources for the
host, so a module's behaviour can be checked in isolation, including
rendering against reference images. Integration tests drive the running
firmware from the outside, pressing buttons, comparing screenshots,
reading logs and measuring current, so they catch what only shows up once
everything runs together.

CI runs the unit tests on every change to the sources, and the integration
tests on the three emulator boards, for both the normal and the recovery
firmware. A lab watch runs them on real hardware as well.

```{toctree}
:hidden:

unit.md
integration.md
```
