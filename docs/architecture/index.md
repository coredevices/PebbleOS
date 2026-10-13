# Architecture overview

A guided tour of how PebbleOS is put together, deep-linking to the design
prose that lives in the source tree. The in-source comments are canonical —
these pages summarize and point, they do not duplicate.

## Layering

PebbleOS runs on its own [kernel API](../kernel/index.md) (`include/pbl/kernel`,
implemented under `kernel/`). The main source layers, as described on the
<a href="../apidoc/index.html">API reference</a> main page:

- `fw/applib` — application framework and UI, the API surface exposed to
  watchapps ([SDK export](../development/sdk_export.md) describes how
  functions get there).
- `fw/services` — system services (Bluetooth, filesystem, activity, …).
- `fw/kernel` — task management, events, memory.
- `drivers/` — hardware drivers (public interfaces under
  `include/pbl/drivers`), built on the [device model](../drivers/device_model.md).
- `subsys/` — OS subsystems shared beyond the firmware tree; currently
  logging, cron, the Bluetooth backends, [CRC](../subsystems/crc.md), [input](../subsystems/input.md),
  the [debug shell](../subsystems/shell.md) and the [task watchdog](../subsystems/task_watchdog.md),
  included via the `pbl/logging/`, `pbl/cron/`, `pbl/bluetooth/`, `pbl/crc/`,
  `pbl/input/`, `pbl/shell/` and `pbl/task_wdt/` header paths.

Alongside these sit `fw/shell` (launcher/watchface UX flow),
`fw/process_management` (app lifecycle) and `fw/comm` (phone
communication).

## Task model

The firmware runs a fixed set of kernel threads, enumerated in
`fw/kernel/pebble_tasks.h`: KernelMain, KernelBackground, Worker, App,
the Bluetooth tasks (host, controller, HCI), NewTimers and
[PULSE](../reference/pulse2/pulse2.md). `main()`
(`fw/main.c`) performs SoC init and spawns KernelMain, which brings up
the rest of the system.

## Boot and firmware variants

The bootloader is not part of this tree; it selects which firmware image to
launch, coordinated through boot bits (see `BOOT_BIT_*` usage in
`fw/main.c`) and the slot metadata in `fw/system/firmware_storage.h`.

Normal firmware and PRF (Pebble Recovery Firmware — the minimal fallback
image used to reinstall the main firmware) are separate compile-time
variants: `pbl configure --variant=prf` (see
[build options](../getting_started/configuration.md)) applies `fw/prj_prf.conf` on
top of the base config, disabling the JS engine and marking the image as
recovery firmware.

## Processes and apps

- **App identity** — an installed app is referred to by several identifiers
  (`AppInstallId`, `AppInstallEntry`, UUID, `PebbleProcessMd`); the comment
  at the top of `fw/process_management/app_install_manager.h` explains
  which to use where and which are deprecated.
- **Shell flow** — which app launches at startup and what happens when an
  app exits is a deliberately flat state machine rooted in the launcher and
  the watchface; see the diagrammed comment at the top of
  `fw/shell/normal/system_app_state_machine.c`.
- **Privilege boundary** — third-party app and worker code runs unprivileged
  (processes built into the firmware stay privileged); anything
  touching OS state crosses into the kernel through a `sys_*` syscall
  (declared in `fw/syscall/syscall.h`) defined with `DEFINE_SYSCALL`
  from `fw/syscall/syscall_internal.h`, which raises privileges on
  entry and drops them on return unless the caller was already privileged.

## Memory layout

`fw/linker/pebbleos.ld` ("Section concepts!" comment) documents the
flash/RAM picture: VMA vs LMA, the kernel data/bss/stack/heap region, and
the fixed app region where third-party app code, data and heap live.
`fw/linker/memory.ld` explains how SRAM is carved between kernel, app
and worker regions, and `fw/linker/regions.ld` notes the MPU
power-of-two constraints.

## Coredumps

The on-flash coredump image format (header plus chunked records, including
per-thread register sets) is documented in `fw/kernel/core_dump.c`.

## Components

Each layer with a public interface under `include/` has its own section,
pairing hand-written guides with the generated API reference:
{doc}`../kernel/index`, {doc}`../subsystems/index`,
{doc}`../drivers/index`, {doc}`../services/index` and
{doc}`../libraries/index`.
