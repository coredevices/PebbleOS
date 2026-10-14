# Architecture overview

PebbleOS is the operating system of the Pebble watches. It is a single
firmware image, built per board, that runs the watch's own UI and services
and hosts third-party watchfaces, apps and background workers. This page
gives the overall picture; each layer has its own section with the details.

## Layers

```{mermaid}
block-beta
  columns 2
  apps["Apps and workers"]:2
  applib["Application framework<br/>fw/applib"]:2
  services["Services<br/>fw/services"]:2
  subsys["Subsystems<br/>subsys/"] libs["Libraries<br/>lib/"]
  drivers["Drivers<br/>drivers/"]:2
  kernel["Kernel<br/>kernel/"]:2
  soc["SoC and HAL<br/>soc/, third_party/"]:2

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class apps,applib app
  class services,subsys,libs system
  class drivers,kernel,soc base
```

From the bottom up:

{doc}`Kernel <../kernel/index>`
: The kernel API the rest of the firmware is written against: threads,
  synchronization, timers, interrupts and memory. It hides the CPU
  architecture and the RTOS underneath.

{doc}`Drivers <../drivers/index>`
: One driver class per kind of peripheral (display, accelerometer,
  flash, …), each with a common interface and an implementation per part,
  built on a shared device model.

{doc}`Subsystems <../subsystems/index>` and {doc}`libraries <../libraries/index>`
: Self-contained building blocks used across the firmware, such as
  logging, the Bluetooth host integration, the debug shell, input
  handling and generic utilities.

{doc}`Services <../services/index>`
: The system's features: they own shared resources, persist state, talk
  to the phone and feed events to apps.

Application framework
: `fw/applib` is the API apps are written against, from windows and
  layers to graphics and text. Its exported part is the
  {doc}`Application SDK <../sdk/index>`.

Apps
: System apps (launcher, settings, notifications, health, …) are built
  into the firmware from `fw/apps`; third-party apps and workers are
  installed from the phone.

The public interfaces of each layer live under `include/pbl/`, documented
in the API reference. Code PebbleOS builds on but does not maintain is
described in {doc}`../third_party/index`.

## Execution model

The firmware runs a fixed set of tasks, each a kernel thread:

| Task | Role |
| --- | --- |
| KernelMain | The system's event loop: input, UI and most services |
| KernelBackground | Deferred work kept off KernelMain |
| App | The foreground app or watchface |
| Worker | The app's background worker, if it has one |
| Bluetooth host, controller, HCI | The Bluetooth stack |
| NewTimers | Timer callbacks |
| PULSE | The serial link to development tools |

Services and drivers post events to KernelMain, which handles them and
forwards what concerns the app to the app's own event queue. Only one app
and one worker run at a time.

Third-party apps and workers run unprivileged, confined by the MPU to
their own memory; whatever needs the system goes through a syscall. System
apps run privileged. {doc}`../sdk/how_it_works` describes the boundary.

## Boot

A bootloader, outside this tree, starts the firmware image in flash. The
kernel's reset handler sets up memory and the SoC, then calls `main()`
(`fw/main.c`), which brings up the drivers and starts KernelMain.
KernelMain initializes the services and launches the first app, normally
the watchface. {doc}`../kernel/boot` details the early steps.

## Firmware variants

The same tree builds three firmwares:

- **Normal**: the full firmware.
- **Recovery (PRF)**: a minimal firmware kept in its own slot, from which
  the phone can reinstall the normal one when it does not start. It has
  no third-party apps and no JavaScript engine.
- **Manufacturing**: the recovery firmware with the factory test
  functions.

The variant, and everything else that is optional, is chosen at build time
with Kconfig, from the board's defconfig and the variant's configuration;
see {doc}`../development/build_system/index`.

## Hardware support

A **board** describes one watch, or one revision of it: its SoC, its
peripherals and the drivers they need. A **platform** is what apps see:
display size, shape and colors, and capabilities. Boards of the same watch
model share a platform, and the emulator provides a board for each
platform. {doc}`../boards/index` lists them.
