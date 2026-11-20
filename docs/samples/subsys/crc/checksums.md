# CRC: Checksums

This sample computes the {doc}`CRC <../../../subsystems/crc>`-8 and CRC-32
of the usual check string, `123456789`, then the CRC-32 again in two chunks:
the running value carries over from one call to the next.

A receiver checks a frame that carries its own CRC-32 without splitting it:
the CRC-32 of the message and its little-endian CRC appended is the constant
`PBL_CRC32_RESIDUE` when nothing changed. The last check is the legacy
checksum of older on-flash formats, which keeps partial words between updates.

```{pebbleos-sample}
:path: samples/subsys/crc/checksums
:sources: src/main.c
:output:
```
