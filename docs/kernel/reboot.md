# Reboot and power-off

`pbl/kernel/reboot.h` restarts or powers off the system:

- `pbl_reboot()` restarts in order;
- `pbl_power_off()` powers off in order, and restarts if that fails;
- `pbl_reboot_emergency()` resets at once, for contexts where nothing else can
  be trusted, e.g. a fault handler or the reset-buttons back door.

Code that must shut down before the system goes away registers a handler with
`PBL_SYS_OFF_HANDLER_DEFINE()`, for one of four modes:

| Mode | Runs | Role |
|------|------|------|
| `PBL_SYS_OFF_RESTART_PREPARE` | before a restart | shut down, may block |
| `PBL_SYS_OFF_POWER_OFF_PREPARE` | before a power-off | shut down, may block |
| `PBL_SYS_OFF_RESTART` | after the prepare handlers | restart the system |
| `PBL_SYS_OFF_POWER_OFF` | after the prepare handlers | power off the system |

Handlers of a mode run highest priority first. A restart or power-off handler
that returns has declined or failed, and the next one runs; once none is left,
the kernel resets the SoC.

Prepare handlers only run on the first request, made from a thread with
interrupts and the scheduler unlocked. A request from an interrupt, with
interrupts locked or from within a handler skips them and goes straight to the
restart or power-off handlers, which therefore run in the context of the
request and must check it before blocking.

The final reset is the SoC's `pbl_soc_reset()` when it selects
`CONFIG_SOC_HAS_RESET`, e.g. SF32LB52 resets through its PMU and the POSIX host
restarts the process; otherwise the architecture's, the NVIC system reset on
Cortex-M.

The firmware's handlers:

| Handler | Mode | Priority |
|---------|------|----------|
| services to `RunLevel_BareMinimum` (Bluetooth off) | prepare | high |
| PULSE link close | prepare | default |
| wait for flash erases, then hold flash | prepare | default |
| mark the reboot reason as a safe restart | prepare | low |
| core dump after a software failure | restart | high |
| nPM1300 ship mode | power-off | default |

Reboot reasons and boot bits stay firmware policy: callers set them before
calling into the kernel.
