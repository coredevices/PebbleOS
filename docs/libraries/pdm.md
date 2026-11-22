# PDM

PDM decimates the 1-bit stream of a PDM microphone to 16-bit PCM, for audio
interfaces that capture it raw, as the SAI of the STM32H7.

| | |
| --- | --- |
| Directory | `lib/pdm` |
| Header | `pbl/pdm/pdm.h` |
| Kconfig | `CONFIG_PDM`, selected by the code that uses it |
| API | {apidoc}`API reference <pdm>` |

## Filters

A fifth-order sinc filter decimates by 8, a byte of samples at a time
through one lookup table per byte of its span. An equiripple low-pass FIR
then decimates by 8 or 16, flat to 0.4 times the output rate and at least
70 dB down from 0.6 times it. A high-pass filter at 1/800 of the output
rate, 20 Hz at 16 kHz, removes the offset of the microphone, and a gain of
up to 42 dB brings speech up from the low levels microphones give it.

The first samples are silence, until the FIR has seen as many as it has
taps and for a settling time the caller gives. `tools/pdm/pdm_tables.py`
generates the tables.
