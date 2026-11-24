# Architecture overview

PebbleOS is a general purpose RTOS for embedded devices. It is the kernel,
the drivers, the subsystems and the libraries; what runs on top of it is an
application, built against PebbleOS from its own directory into a single
image for one board. The Pebble watch firmware under `pebble/` is one such
application, and so are the {doc}`samples <../samples/index>`. This page gives
the overall picture; each layer has its own section with the details.

## Layers

```{mermaid}
block-beta
  columns 2
  app["Application<br/>e.g. pebble/, samples/"]:2
  subsys["Subsystems<br/>subsys/"] libs["Libraries<br/>lib/"]
  drivers["Drivers<br/>drivers/"]:2
  kernel["Kernel<br/>kernel/"]:2
  soc["SoC and architecture<br/>soc/, kernel/arch/"]:2

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class app app
  class subsys,libs system
  class drivers,kernel,soc base
```

From the bottom up:

SoC and architecture
: Each supported SoC (`soc/`) provides its early init, clocks, interrupts
  and memory layout. The kernel has a Cortex-M port and a host port, which
  runs PebbleOS as a native program for simulation and tests.

{doc}`Kernel <../kernel/index>`
: PebbleOS's own preemptive kernel: threads, synchronization, work queues,
  timers, interrupts, memory protection and syscalls.

{doc}`Drivers <../drivers/index>`
: Many device classes (display, flash, sensors, buses, …) on top of a
  common device model. Each class has one interface and a driver per part.

{doc}`Subsystems <../subsystems/index>` and {doc}`libraries <../libraries/index>`
: Self-contained building blocks, such as Bluetooth LE, file systems,
  logging, settings, the shell, input handling and generic utilities.

Application
: The code that provides `main()`, in C or in {doc}`Rust <../development/build_system/rust>`;
  see {doc}`../development/build_system/applications`.

The public interfaces of each layer live under `include/pbl/`, documented
in the API reference. Code PebbleOS builds on but does not maintain is
described in {doc}`../third_party/index`.

## Configuration

Everything is chosen at build time; see {doc}`../development/build_system/index`.

- **Kconfig** selects what is built. A board's defconfig only selects its
  SoC, boot chain and console; driver classes, subsystems and optional
  kernel features are enabled by the application.
- **Devicetree** describes the hardware of a board, and of the
  {doc}`add-ons <../addons/index>` plugged into it. Its devices decide which
  drivers are built and their instances.
- **CMake** builds every enabled directory as a library and links them,
  with the application, into one image.

A **board** describes one device, or one revision of it: its SoC and its
peripherals. {doc}`../boards/index` lists them, from real hardware to
emulated and host boards.

## Execution model

Everything runs as kernel threads, scheduled preemptively by priority.
Interrupts hand off to threads, often through a work queue. The system work
queues run deferred work for the drivers and subsystems; an application
creates its own threads and queues as it needs.

With `CONFIG_USERSPACE`, threads can run unprivileged, confined by the MPU
to their own memory, and reach the system through
{doc}`syscalls <../kernel/syscalls>`. Without it, every thread runs
privileged.

## Boot

The kernel owns the reset vector. It sets up memory and the SoC, prints the
boot banner, and starts the scheduler with the main thread. Before `main()`
runs on it, logging and the task watchdog are running, when enabled, and
every device the board defines has been initialized. {doc}`../kernel/boot`
details the steps.

A build may flash other images with the application, such as a bootloader;
see {ref}`application-images`.

## The Pebble firmware

The watch firmware is the largest application. On top of PebbleOS it adds:

{doc}`Services <../services/index>`
: The watch's features: they own shared resources, persist state, talk to
  the phone and feed events to apps.

Application framework
: `pebble/applib` is the API watch apps are written against, from windows
  and layers to graphics and text. Its exported part is the
  {doc}`Application SDK <../sdk/index>`.

Watch apps
: System apps (launcher, settings, notifications, health, …) are built
  into the firmware from `pebble/apps`; third-party watchfaces, apps and
  background workers are installed from the phone. They run unprivileged;
  {doc}`../sdk/how_it_works` describes the boundary.

Its main thread runs the event loop: services and drivers post events to
it, and it forwards what concerns the app to the app's own thread. Only one
app and one worker run at a time.

It builds in two variants: **normal**, the full firmware, and **recovery
(PRF)**, a minimal one kept in its own slot, from which the phone can
reinstall the normal one. A **platform** is what watch apps see: display
size, shape and colors, and capabilities; boards of the same watch model
share one.
