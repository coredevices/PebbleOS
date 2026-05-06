# Running Tests

## Cross-Platform Test Fixtures

Graphics test fixtures are platform-specific due to differences in:
- Font rendering libraries (FreeType, HarfBuzz)
- Standard library implementations
- ARM toolchain behavior

Test fixtures are named with the format: `test_name~platform-os.pbi`
- `~spalding-linux.pbi` - Generated on Linux (CI environment)
- `~spalding-darwin.pbi` - Generated on macOS (local development)

## Local Development

### macOS Developers

**Option 1: Use Docker (Recommended)**

Run tests in Docker to match the CI environment exactly:

```bash
# Run all tests
./tests/run-tests-docker.sh

# Run specific tests
./tests/run-tests-docker.sh -M "test_kickstart"

# Use specific board
TEST_BOARD=snowy_bb2 ./tests/run-tests-docker.sh
```

This ensures your test results match CI exactly.

**Option 2: Generate macOS Fixtures**

If you prefer to run tests natively on macOS:

```bash
# Configure and build
./waf configure --board=snowy_bb2
./waf test

# This will generate macOS-specific fixtures (~spalding-darwin.pbi)
# which will be used instead of the Linux fixtures
```

Note: macOS-generated fixtures will differ from Linux fixtures. This is expected
and doesn't indicate a problem with your changes. Use Docker to verify against CI.

### Linux Developers

Run tests normally - your environment matches CI:

```bash
./waf configure --board=snowy_bb2
./waf test
```

## Updating Fixtures

When you intentionally change rendering behavior:

1. **Run tests in Docker** to generate new Linux fixtures:
   ```bash
   ./tests/run-tests-docker.sh
   ```

2. **Copy the generated fixtures** from the failed test directory:
   ```bash
   cp build/test/tests/failed/*-expected.pbi tests/fixtures/graphics/
   ```

3. **Update filenames** to include the `-linux` suffix if needed:
   ```bash
   # Rename from ~spalding.pbi to ~spalding-linux.pbi
   ```

4. **Commit and push** the updated fixtures

## CI Environment

- Container: `ghcr.io/coredevices/pebbleos-docker:v3`
- OS: Ubuntu 24.04 (Linux)
- Board: snowy_bb2
- Compiler: arm-none-eabi-gcc 14.2.Rel1

## Troubleshooting

### Tests pass locally but fail on CI

Run tests in Docker to reproduce CI results:
```bash
./tests/run-tests-docker.sh
```

### Tests fail locally but pass on CI

Generate macOS-specific fixtures or use Docker for local development.

### Fixture naming confusion

The test framework automatically selects the correct fixture based on your OS:
- On Linux: Uses `~spalding-linux.pbi`
- On macOS: Uses `~spalding-darwin.pbi`
- Falls back to `~spalding.pbi` if OS-specific doesn't exist
