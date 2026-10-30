# Services

System services sit between the drivers and the applications: they own
shared hardware, persist state, talk to the phone and feed events to apps.
They live in `fw/services/`, with their interfaces in `include/pbl/services/`.

## Storage

Services keep their files on the root (`/`) file system of the
{doc}`file system subsystem <../subsystems/fs>`.

## Service modules

```{toctree}
:maxdepth: 1

activity/index.md
```

The {apidoc}`API reference <services>` documents every service.
