```{raw} html
<div style="display: none;">
```

# PebbleOS documentation

```{raw} html
</div>
```

:::::{div} home-hero

<img class="home-logo" src="_static/images/logo.svg" alt="PebbleOS" />

<img class="home-watches" src="_static/images/watches.webp" alt="Pebble Time 2 and Pebble Round 2" />

```{div} home-title
Welcome to the PebbleOS documentation site!
```

PebbleOS is a lightweight, power-efficient general purpose RTOS for embedded devices, grown out of the [open-sourced](https://opensource.googleblog.com/2025/01/see-code-that-powered-pebble-smartwatches.html) Pebble smartwatch firmware.
It is built with CMake and Kconfig, supports devicetree, and offers many device classes on top of a robust device model, along with subsystems such as Bluetooth LE, file systems and logging.

````{div} home-actions

```{button-ref} getting_started/index
:ref-type: doc
:color: primary
:class: sd-rounded-pill

Get started
```

```{button-ref} playground
:ref-type: doc
:color: primary
:outline:
:class: sd-rounded-pill

Try it in the browser
```

````

:::::

::::{grid} 1 1 2 3
:gutter: 3

:::{grid-item-card} 🚀 Getting started
:link: getting_started/index
:link-type: doc

From a fresh machine to PebbleOS running in the emulator or on a board.
:::

:::{grid-item-card} 🛠️ Development
:link: pbl/index
:link-type: doc

The `pbl` CLI and every command, the build system, tests, QEMU and debugging.
:::

:::{grid-item-card} 🧪 Samples
:link: samples/index
:link-type: doc

Small applications showing the kernel, subsystems and drivers in use.
:::

:::{grid-item-card} 🏛️ Architecture
:link: architecture/index
:link-type: doc

How PebbleOS is layered, configured and booted, and how applications sit on top.
:::

:::{grid-item-card} ⚙️ Kernel
:link: kernel/index
:link-type: doc

Threads, synchronization, interrupts and scheduling.
:::

:::{grid-item-card} 🔌 Drivers
:link: drivers/index
:link-type: doc

The device model and every hardware driver class.
:::

:::{grid-item-card} 🧩 Subsystems
:link: subsystems/index
:link-type: doc

Bluetooth, logging, input, the shell and the other OS subsystems.
:::

:::{grid-item-card} ⌚ Pebble Firmware
:link: pebble/index
:link-type: doc

The watch firmware built on PebbleOS: a playground and the application SDK.
:::

:::{grid-item-card} 🔧 Boards
:link: boards/index
:link-type: doc

Supported hardware, emulated and host boards, and how to program them.
:::

::::

```{toctree}
:hidden:
:caption: Start here

getting_started/index.md
```

```{toctree}
:hidden:
:caption: Pebble Firmware

Overview <pebble/index.md>
playground.md
sdk/index.md
```

```{toctree}
:hidden:
:caption: Development

pbl/index.md
development/build_system/index.md
samples/index.md
development/emulation/index.md
development/tests/index.md
development/debugging/index.md
development/conventions.md
```

```{toctree}
:hidden:
:caption: System

architecture/index.md
kernel/index.md
drivers/index.md
subsystems/index.md
libraries/index.md
third_party/index.md
```

```{toctree}
:hidden:
:caption: Hardware

boards/index.md
addons/index.md
programmers/index.md
```

```{toctree}
:hidden:
:caption: Community

community/contributing.md
```

```{toctree}
:hidden:
:caption: Reference

reference/index.md
reference/kconfig.md
API reference <apidoc://>
```
