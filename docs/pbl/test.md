# `pbl test`

Build and run the unit tests on the host. They are a CMake project of their
own, configured into `build-test/`; anything `pbl test` does not recognize
is passed to ctest.

```shell
pbl test
pbl test -R animation
pbl test --coverage
```

{doc}`../development/tests/unit` covers selecting, writing and debugging
tests.

## Options

```{pbl-command} test
```
