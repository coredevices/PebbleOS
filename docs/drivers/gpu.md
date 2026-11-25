# GPU

A GPU draws into pixel buffers owned by the caller: it fills rectangles and
blits buffers, converting formats, blending and, where the hardware can,
rotating and scaling. It refuses what it cannot do before drawing anything,
for the caller to draw it in software.

| | |
| --- | --- |
| Directory | `drivers/gpu` |
| Header | `pbl/drivers/gpu.h` |
| Kconfig | `CONFIG_GPU`, enabled by the application |
| Role | `GPU` |
| Sample | {doc}`../samples/drivers/gpu` |
| API | {apidoc}`API reference <drivers_gpu>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB57 ePIC | `sifli,sf32lb57-epic` | `CONFIG_GPU_SF32LB57_EPIC` |
