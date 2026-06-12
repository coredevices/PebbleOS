# Testing locally

The CI test environment runs inside a Docker container
(`ghcr.io/coredevices/pebbleos-docker:v5`) to ensure consistent libc versions
and tooling. Host unit tests compile against the host libc, which differs
between macOS and the Linux CI environment, so running tests locally without the
container may produce different results.

To reproduce CI locally, use the container runners provided in `tools/ci/`:

- `./tools/ci/run_tests.sh` — run the host test suite (same as CI)
- `./tools/ci/build_firmware.sh` — build firmware in the CI container

The native `./waf test` and `./waf build` commands are faster for iteration but
can diverge from CI. Always run the container scripts before pushing to verify
consistency.
