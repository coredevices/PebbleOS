<p align="center">
  <img src="docs/_static/images/logo.svg">
</p>

<p align="center">
 PebbleOS
</p>

<p align="center">
  <a href="https://github.com/coredevices/PebbleOS/actions/workflows/build-firmware.yml?query=branch%3Amain"><img src="https://github.com/coredevices/PebbleOS/actions/workflows/build-firmware.yml/badge.svg?branch=main"></a>
  <a href="https://pebbleos-core.readthedocs.io/en/latest"><img src="https://readthedocs.org/projects/pebbleos-core/badge/?version=latest&style=flat"></a>
  <a href="https://discordapp.com/invite/aRUAYFN"><img src="https://dcbadge.limes.pink/api/server/aRUAYFN?style=flat"></a>
</p>

## Resources

Here's a quick summary of resources to help you find your way around:

### Getting Started

- 📖 [Documentation](https://pebbleos-core.readthedocs.io/en/latest)
- 🚀 [Prerequisites Guide](https://pebbleos-core.readthedocs.io/en/latest/development/getting_started.html)

### Code and Development

- ⌚ [Source Code Repository](https://github.com/coredevices/PebbleOS)
- 🐛 [Issue Tracker](https://github.com/coredevices/PebbleOS/issues)
- 🤝 [Contribution Guide](CONTRIBUTING.md)

### Community and Support

- 💬 [Discord](https://discordapp.com/invite/aRUAYFN)
- 👥 [Discussions](https://github.com/coredevices/PebbleOS/discussions)

### Platform Types

SDK-level classifications that determine software compatibility:

| Platform    | MCU      | Display Type | Color Depth | Touch | Devices                  |
| ----------- | -------- | ------------ | ----------- | ----- | ------------------------ |
| **APLITE**  | STM32F2  | Rectangular  | B&W 1-bit   | No    | Pebble, Pebble Steel     |
| **BASALT**  | STM32F4  | Rectangular  | 8-bit color | No    | Pebble Time              |
| **CHALK**   | STM32F4  | Round        | 8-bit color | No    | Pebble Time Round        |
| **DIORITE** | STM32F4  | Rectangular  | B&W 1-bit   | No    | Pebble 2 SE, Pebble 2 HR |
| **EMERY**   | STM32F7  | Rectangular  | 8-bit color | No    | Pebble Time 2            |
| **FLINT**   | NRF52840 | Rectangular  | B&W 1-bit   | No    | Pebble 2 Duo             |
| **GABBRO**  | SF32LB52 | Round        | 8-bit color | Yes   | Pebble Round 2           |

### Devices

Consumer products and their hardware:

| Device                | Era         | Platform Type | Board        | Screen                      |
| --------------------- | ----------- | ------------- | ------------ | --------------------------- |
| **Pebble**            | Pebble      | APLITE        | tintin_bb2   | 144×168 B&W                 |
| **Pebble Steel**      | Pebble      | APLITE        | tintin_bb2   | 144×168 B&W                 |
| **Pebble Time**       | Pebble      | BASALT        | snowy_bb2    | 144×168 color               |
| **Pebble Time Round** | Pebble      | CHALK         | spalding_bb2 | 180×180 color round         |
| **Pebble 2 SE**       | Pebble      | DIORITE       | silk_bb2     | 144×168 B&W                 |
| **Pebble 2 HR**       | Pebble      | DIORITE       | silk_bb2     | 144×168 B&W                 |
| **Pebble Time 2**     | Pebble      | EMERY         | robert_bb2   | 200×228 color               |
| **Pebble 2 Duo**      | CoreDevices | FLINT         | asterix      | 144×168 B&W                 |
| **Pebble Round 2**    | CoreDevices | GABBRO        | spalding_bb2 | 260×260 color round + touch |
| **Pebble Round 2**    | CoreDevices | GETAFIX       | getafix_evt  | 260×260 color round + touch |

### Board Variants

Each platform family has multiple board variants representing different hardware revisions:

**Hardware Lifecycle Stages:**

- **EV/EVT** (Engineering Validation): Early prototype validation (5-50 units)
- **DVT** (Design Validation Test): Final design validation (50-500 units)
- **PVT** (Production Validation Test): Manufacturing process validation (500-5,000 units)
- **MP** (Mass Production): Full retail manufacturing (5,000+ units)
- **BB** (Bigboard): Development boards for engineers (not consumer devices)

| Platform     | Board Variants                                                                                                      | Protocol #      | Era                 |
| ------------ | ------------------------------------------------------------------------------------------------------------------- | --------------- | ------------------- |
| **Tintin**   | `tintin_bb`, `tintin_bb2`, `tintin_ev1`, `tintin_ev2`, `tintin_ev2_3`, `tintin_ev2_4`, `tintin_v1_5`, `tintin_v2_0` | 1-6, 254-255    | Pebble              |
| **Snowy**    | `snowy_bb`, `snowy_bb2`, `snowy_dvt`, `snowy_evt2`, `snowy_s3`, `snowy_emery`                                       | 7-8, 10, 253    | Pebble, CoreDevices |
| **Spalding** | `spalding_bb2`, `spalding_evt`, `spalding`                                                                          | 9, 11, 251      | Pebble              |
| **Silk**     | `silk_bb`, `silk_bb2`, `silk_evt`, `silk`, `silk_flint`                                                             | 12, 14, 248-250 | Pebble              |
| **Robert**   | `robert_bb`, `robert_bb2`, `robert_evt`                                                                             | 13, 247-249     | Pebble              |
| **Cutts**    | `cutts_bb`                                                                                                          | —               | Pebble              |
| **Asterix**  | `asterix`                                                                                                           | 15              | CoreDevices         |
| **Obelix**   | `obelix_bb`, `obelix_dvt`, `obelix_pvt`, `obelix_bb2`                                                               | 16-18, 243-244  | Pebble              |
| **Gabbro**   | `spalding_gabbro`                                                                                                   | —               | CoreDevices         |
| **Getafix**  | `getafix_evt`                                                                                                       | 19              | CoreDevices         |

### Resource Platform Directories

The `resources/normal/` directory contains platform-specific resources:

| Directory         | Screen Resolution          | Description                           |
| ----------------- | -------------------------- | ------------------------------------- |
| `tintin`          | 144×168, B&W 1-bit         | Original Pebble resources             |
| `snowy`           | 144×168, Color 8-bit       | Pebble Time resources                 |
| `snowy_emery`     | 200×228, Color             | Snowy with robert screen/resources    |
| `spalding`        | 180×180, Color 8-bit round | Pebble Time Round resources           |
| `spalding_gabbro` | 260×260, Color round       | GABBRO (PR2) resources                |
| `silk`            | 144×168, B&W 1-bit         | Pebble 2 + HR resources               |
| `robert`          | 200×228, Color 8-bit       | Pebble Time 2 prototype resources     |
| `asterix`         | 144×168, B&W               | Pebble 2 Duo resources                |
| `getafix`         | 260×260, Color round       | Pebble Round 2 resources              |
| `obelix`          | 368×448, Color             | Pebble Time 2 resources               |
| `base`            | -                          | Shared resources across all platforms |
| `calculus`        | -                          | Shared resources                      |

### Bluetooth Stack Compatibility

| Stack          | Platforms               | Proprietary | Notes                                                        |
| -------------- | ----------------------- | ----------- | ------------------------------------------------------------ |
| **TI CC2564X** | Tintin, Snowy, Spalding | Yes         | Requires `GAPAPI.h` and `GATTAPI.h` (proprietary TI headers) |
| **DA14681**    | Silk, Robert, Cutts     | Yes         | Dialog Semiconductor stack                                   |
| **NRF52**      | Asterix                 | No          | Nordic Semiconductor stack (open)                            |
| **SF32LB52**   | Obelix, Getafix         | Yes         | Sifli stack                                                  |
