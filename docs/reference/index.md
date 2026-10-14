# Reference

Specifications of the protocols and file formats used by the firmware and
its tooling.

## PULSEv2 protocol suite

PULSE is the serial protocol spoken between the firmware and the host
tooling (`pbl console`, flash imaging, `tools/pulse/`). These pages
specify the wire format and its transports.

```{toctree}
:maxdepth: 1

pulse2/pulse2.md
pulse2/reliable-transport.md
pulse2/flash-imaging.md
pulse2/history.md
```

## Pebble Protocol endpoints

Wire formats of the Bluetooth protocol spoken between the firmware and
the phone apps.

```{toctree}
:maxdepth: 1

blob-db.md
```

## Binary formats

The file and resource formats produced by the firmware tooling. These
pages summarize the layouts; the packed structs in the referenced
sources stay authoritative.

```{toctree}
:maxdepth: 1

formats/font.md
formats/pbi.md
formats/pdc.md
formats/timezone.md
```
