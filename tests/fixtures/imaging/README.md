# Album-art protocol fixtures

`gabbro.packets` and `random.packets` were emitted by CoreApp's
`TiledImageEncoder` and `ImagingResponse` implementations. Each file is a
sequence of little-endian `uint16` body lengths followed by endpoint response
bodies; the command byte (`0x02`) is omitted. Both request 260 × 260 pixels,
use 16 palette entries, and carry format `0x03` tiles. Their matching `.raw`
files are the exact 4-bpp packed pixels before compression, 33,800 bytes each.

`gabbro` uses the Gabbro album-art PBI test cover from this repository. `random`
uses the deterministic incompressible 4-bpp pattern from the CoreApp codec
test vectors (seed recorded in its Kotlin test). The firmware test feeds every
packet through the imaging endpoint and compares all 26 decoded tiles byte for
byte with the raw source. The random vector also exercises raw tile fallback.
