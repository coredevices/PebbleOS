# CRC

`subsys/crc/` computes every checksum the firmware stores or exchanges. Its
public API is `include/pbl/crc/crc.h`:

| Function | Algorithm | Used by |
| --- | --- | --- |
| `pbl_crc8()`, `pbl_crc8_reversed()` | CRC-8, polynomial 0x2F (CRC-8/OPENSAFETY) | PFS page chaining, settings file key hashes |
| `pbl_crc32()` | CRC-32/ISO-HDLC, zlib-compatible | PULSE2, flash CRC, shared PRF storage, boot bits |
| `pbl_crc32_legacy_*()` | CRC-32/MPEG-2 over little-endian words | PFS, app images, resources, data logging, PULSE, put bytes |

The legacy checksum reproduces what the STM32F2/F4 CRC unit computed when the
firmware fed it 32-bit words, including the zero padding of a trailing partial
word. It lives on in on-flash and on-wire formats, so its definition cannot
change; `crc.h` spells it out.

## Layering

The layering follows Linux's `lib/crc`. The public functions own the
software implementations, which are always built, and decide when to hand
work to hardware:

```
pbl_crc32() / pbl_crc32_legacy_update()      subsys/crc
  ├── len >= CONFIG_CRC_HW_MIN_LEN ──► pbl_crc_hw_*()   driver, optional
  └── remainder ──────────────────────► software, nybble-wide tables
```

A driver offering acceleration selects the hidden `CONFIG_CRC_HW_ACCEL`
symbol and implements the two functions in `include/pbl/drivers/crc.h`. Each
takes a running value and a buffer, processes a prefix of it that is a whole
number of 32-bit words, and returns its length in bytes. Returning less than
the full length is always allowed: the subsystem finishes in software. A
driver returns 0 whenever the hardware cannot be used — before it is ready,
when its self-test failed, or for any other reason — so callers never see a
failure.

`CONFIG_CRC_HW_MIN_LEN` keeps short buffers in software, where programming the
peripheral costs more than it saves. CRC-8 inputs are a few bytes long and
are never offloaded.

## SF32LB

`fw/drivers/crc/sf32lb.c` (`CONFIG_CRC_SF32LB`, default on SF32LB52) drives
the CRC1 unit. It programs the polynomial, initial value and reflection for
every burst of up to 256 bytes under `pbl_irq_lock()`, carrying the running
value between bursts, so the unit needs no owner. That works before the
scheduler starts and from interrupt context, and bounds the interrupt
latency it adds to a few microseconds.

At 240 MHz a call costs about 330 cycles of setup, then about 5.4 cycles per
byte against about 20 for the software tables. The unit breaks even around
24 bytes and is about 3.6 times faster on large buffers, which sets the
`CONFIG_CRC_HW_MIN_LEN` default of 32.

On first use the driver checks both algorithms against fixed test vectors,
in one burst and resumed across two, and disables itself on any mismatch,
logging an error. The software path then serves every request.
