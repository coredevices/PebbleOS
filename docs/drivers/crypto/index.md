# Crypto

Crypto drivers accelerate the {doc}`crypto subsystem <../../subsystems/crypto>`
with a hardware hash or cipher engine. They are optional: without one, the
subsystem computes everything in software.

## Interface

A driver sits under `CONFIG_CRYPTO_HW`, which the application enables, and
implements the functions in `include/pbl/drivers/crypto.h` for what its
hardware covers, selecting the matching hidden symbol:

| Symbol | Functions |
| --- | --- |
| `CONFIG_CRYPTO_SHA256_HW_ACCEL` | `pbl_crypto_hw_sha256_blocks()` |
| `CONFIG_CRYPTO_AES_HW_ACCEL` | `pbl_crypto_hw_aes_ecb_encrypt()`, `pbl_crypto_hw_aes_ecb_decrypt()` |

The SHA-256 function compresses whole 64-byte blocks into a running
chaining value; the AES functions take the raw key and a run of 16-byte
blocks. Each returns how many blocks it processed.

Returning fewer blocks than given is always allowed: the subsystem finishes
in software. A driver returns 0 whenever the hardware cannot be used, such as
before its device is ready, so callers never see a failure. Drivers do not
check their own results: the subsystem's
{doc}`self-test <../../subsystems/crypto>` does. The subsystem only calls a driver for runs of at least
`CONFIG_CRYPTO_SHA256_HW_MIN_LEN` or `CONFIG_CRYPTO_AES_HW_MIN_LEN` bytes.

## Implementations

```{toctree}
:maxdepth: 1

sf32lb.md
```

The {apidoc}`API reference <drivers_crypto>` documents the interface.
