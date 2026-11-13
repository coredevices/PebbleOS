# Crypto

`subsys/crypto/` provides the hashes and ciphers the firmware uses. It is
built with `CONFIG_CRYPTO`, which the code that uses it selects:

| Header | Functions | Algorithm |
| --- | --- | --- |
| `include/pbl/crypto/sha256.h` | `pbl_sha256_init()`, `pbl_sha256_update()`, `pbl_sha256_final()`, `pbl_sha256()` | SHA-256 (FIPS 180-4) |
| `include/pbl/crypto/aes.h` | `pbl_aes_expandkey()`, `pbl_aes_encrypt()`, `pbl_aes_decrypt()`, `pbl_aes_ecb_encrypt()`, `pbl_aes_ecb_decrypt()` | AES-128/192/256 (FIPS 197), single blocks or ECB |

Other AES modes of operation are left to their users.

## Layering

The public functions own the software implementations, which are always built,
and decide when to hand work to hardware:

```{mermaid}
flowchart TD
  api["pbl_sha256_update() / pbl_aes_ecb_encrypt()<br/>subsys/crypto"]
  hw["pbl_crypto_hw_*()<br/>driver, optional"]
  sw["mbedTLS"]
  api -- "len ≥ CONFIG_CRYPTO_*_HW_MIN_LEN" --> hw
  api -- "remainder" --> sw
  hw -. "what it did not process" .-> sw

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class api app
  class sw system
  class hw base
```

The subsystem keeps its own state: `struct pbl_sha256_ctx` holds the chaining
value, the length and a partial block, and `struct pbl_aes_ctx` the encryption
and decryption round keys, whose first words are the key itself. The subsystem
buffers and pads SHA-256 itself and hands runs of whole blocks, or runs of AES
blocks, to the hardware above a threshold and to mbedTLS otherwise. mbedTLS
only runs block functions, on a temporary context loaded with that state, so
its types never reach the public headers. Bluetooth already links all of
mbedTLS's AES, and SHA-256 is built with `MBEDTLS_SHA256_SMALLER`.

Hardware acceleration comes from a driver implementing
`include/pbl/drivers/crypto.h` (see the {apidoc}`API reference <drivers_crypto>`),
which may process any number of the blocks it is given: the subsystem
always finishes in software, so callers never see a failure.

## Self-test

`CONFIG_CRYPTO_SELFTEST` (off by default) checks the accelerators with known
answers fed through the hardware hooks, independently of any driver: the FIPS
180-4 one-block and two-block SHA-256 examples, the latter in one request and
resumed across two, and the FIPS 197 examples for every AES key size, both
ways. Each algorithm is checked before its first hardware use; one whose
accelerator returns a wrong result logs an error and stays in software from
then on.

It also provides `pbl_crypto_selftest()` (`include/pbl/crypto/selftest.h`), to
run the check on demand. It returns 0, `-EIO` on a wrong result or `-ENODEV`
without a ready accelerator.

## Thresholds

Programming an accelerator costs a fixed setup, so short runs stay in
software. `CONFIG_CRYPTO_SHA256_HW_MIN_LEN` and `CONFIG_CRYPTO_AES_HW_MIN_LEN`
set the shortest run handed to the hardware; the
{doc}`driver pages <../drivers/crypto/index>` give the measurements behind
their defaults.
