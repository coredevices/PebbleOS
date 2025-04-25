```{raw} html
<div style="display: none;">
```
# PebbleOS documentation
```{raw} html
</div>
```

::::{grid} 2
:reverse:

:::{grid-item}
:columns: 4
:class: sd-m-auto

<img src="_static/images/logo.svg" />

:::

:::{grid-item}
:columns: 8

```{div} sd-fs-3 home-title
Welcome to the PebbleOS documentation site!
```

PebbleOS is a lightweight, power-efficient operating system originally developed for Pebble smartwatches and now [released open source](https://opensource.googleblog.com/2025/01/see-code-that-powered-pebble-smartwatches.html).
It supports custom watchfaces and apps using C and JavaScript.
Optimized for memory-in-pixel (MIP) displays and long battery life, it features Bluetooth sync, a timeline interface, and a strong developer ecosystem through an open SDK.

```{button-ref} getting_started
:ref-type: doc
:color: primary
:class: sd-rounded-pill float-left

Get started
```

```{only} html
[![](https://img.shields.io/github/stars/pebble-dev/pebble-firmware?style=social)](https://github.com/pebble-dev/pebble-firmware)
```

:::

::::

::::{grid} 1 1 2 3
:class-container: text-center
:gutter: 3

:::{grid-item-card}
:link: getting_started
:link-type: doc
:class-header: bg-light

🚀 Getting Started
^^^

Learn how to build PebbleOS from source!
:::

:::{grid-item-card}
:link: https://github.com/pebble-dev/pebble-firmware
:class-header: bg-light

⌚ Browse the sources
^^^

Browse the PebbleOS sources!
:::

:::{grid-item-card}
:link: reference/external
:link-type: doc
:class-header: bg-light

📖 Reference
^^^

Learn more about PebbleOS: podcasts, developer documents and more!
:::

::::

```{toctree}
:hidden:
getting_started.md
```

```{toctree}
:hidden:
:caption: 🛠️ Development
development/options.md
development/prf.md
development/qemu.md
```

```{toctree}
:hidden:
:caption: 📖 Reference 
reference/external.md
```
