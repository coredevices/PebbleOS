# CRC

CRC drivers accelerate the {doc}`CRC subsystem <../../subsystems/crc>` with
a hardware CRC unit. They are optional: without one, the subsystem computes
every CRC in software.

## Interface

A driver selects the hidden `CONFIG_CRC_HW_ACCEL` symbol and implements the
two functions in `include/pbl/drivers/crc.h`, one per algorithm. Each takes
a running value and a buffer, processes a prefix of it that is a whole
number of 32-bit words, and returns its length in bytes.

Returning less than the full length is always allowed: the subsystem
finishes in software. A driver returns 0 whenever the hardware cannot be
used, before it is ready, when its self-test failed, or for any other
reason, so callers never see a failure.

## Implementations

```{toctree}
:maxdepth: 1

sf32lb.md
```

The {apidoc}`API reference <drivers_crc>` documents the interface.
