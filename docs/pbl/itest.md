# `pbl itest`

Run the pytest integration tests in `tests/integration` against the build,
on the emulator or a real device. Anything `pbl itest` does not recognize
is passed to pytest.

```shell
pbl itest
pbl itest -k settings
pbl itest --device-serial /dev/ttyACM0
```

{doc}`../development/tests/integration` covers the backends, markers and
writing tests.

## Options

```{pbl-command} itest
```
