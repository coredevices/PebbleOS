# RNG: Random words

This sample prints a few words from the hardware random number generator with
`rng_rand()`, then draws a thousand more and counts how many repeat the one
before and how many of their bits are set: about 500 per mille, with no
repeats, from a good source.

The emulator has no generator: its driver returns a fixed word, so every word
repeats.

```{pebbleos-sample}
:path: samples/drivers/rng
:sources: src/main.c
:output:
```
