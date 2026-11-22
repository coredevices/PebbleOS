# Boards

| Board | Revisions | Cores | Variants | Device | Platform |
| --- | --- | --- | --- | --- | --- |
| [`asterix`](asterix/index.md) | | | | Pebble 2 Duo | Flint |
| [`obelix`](obelix/index.md) | `bb2`, `dvt`, `pvt` | | | Pebble Time 2 | Emery |
| [`getafix`](getafix/index.md) | `dvt`, `dvt2` | | | Pebble Round 2 | Gabbro |
| [`sf32lb57_dpi_hdk`](sf32lb57_dpi_hdk/index.md) | | | | SiFli SF32LB57x-DPI-HDK | |
| [`stm32h747i_disco`](stm32h747i_disco/index.md) | | | | STMicroelectronics STM32H747I-DISCO | |
| `qemu_flint` | | | | Emulated Pebble 2 Duo | Flint |
| `qemu_emery` | | | | Emulated Pebble Time 2 | Emery |
| `qemu_gabbro` | | | | Emulated Pebble Round 2 | Gabbro |
| `native_flint` | | | `wasm` | Host program | Flint |
| `native_emery` | | | `wasm` | Host program | Emery |
| `native_gabbro` | | | `wasm` | Host program | Gabbro |

Hardware plugged into a connector of a board, such as a display module, is
an add-on; see {doc}`addons/index`.

```{toctree}
:hidden:

asterix/index.md
obelix/index.md
getafix/index.md
sf32lb57_dpi_hdk/index.md
stm32h747i_disco/index.md
addons/index.md
```
