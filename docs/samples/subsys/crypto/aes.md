# Crypto: AES

This sample encrypts the example block of FIPS 197 with
{doc}`AES <../../../subsystems/crypto>`-128 and AES-256, from a key schedule
expanded once with `pbl_aes_expandkey()`, and decrypts it back.

It then encrypts three blocks in ECB mode. ECB encrypts each block on its
own, so the first and last blocks, which are equal, encrypt to equal blocks:
a pattern in the plaintext shows through, which is why ECB is a building
block for other modes rather than a way to encrypt messages. Lengths that are
not whole blocks are refused.

```{pebbleos-sample}
:path: samples/subsys/crypto/aes
:sources: src/main.c
:output:
```
