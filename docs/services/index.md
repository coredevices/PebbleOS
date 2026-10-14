# Services

System services sit between the drivers and the applications: they own
shared hardware, persist state, talk to the phone and feed events to apps.
They live in `fw/services/`, with their interfaces in `include/pbl/services/`.

## Storage

PFS, the Pebble File System, lives in `fw/services/filesystem/`. The API
and on-flash layout are documented in `pfs.h`; the wear-leveling strategy
(round-robin page allocation tracking the last written page) is described
alongside the allocator in `pfs.c`.

## Service modules

```{toctree}
:maxdepth: 1

activity/index.md
```

The {apidoc}`API reference <services>` documents every service.
