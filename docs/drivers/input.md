# Input

Input drivers report key and touch events to the
{doc}`input subsystem <../subsystems/input>`, which hands them to its
listeners. Keys have no class of their own: their drivers are plain devices.
Touchscreens have the {doc}`touchscreen <touch>` class.

| | |
| --- | --- |
| Directory | `drivers/input` |
| Kconfig | `CONFIG_INPUT`, enabled by the application |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| ADC keys | `adc-keys-irq` | `CONFIG_INPUT_ADC_KEYS` |
| GPIO keys | `gpio-keys` | `CONFIG_INPUT_GPIO_KEYS` |
| SDL keyboard | — | `CONFIG_INPUT_SDL` |
