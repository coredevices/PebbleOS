# Reboot and power-off

`pbl/kernel/reboot.h` restarts or powers off the system:

- `pbl_reboot()` restarts in order;
- `pbl_power_off()` powers off in order;
- `pbl_power_cut()` cuts the power of the whole system in order, if the
  power-cut handlers can do it now, and restarts if that fails after all;
- `pbl_reboot_emergency()` resets at once, for contexts where nothing else can
  be trusted, e.g. a fault handler or the reset-buttons back door.

Code that must shut down before the system goes away registers a handler with
`PBL_SYS_OFF_HANDLER_DEFINE()`, for one of five modes:

| Mode | Runs | Role |
|------|------|------|
| `PBL_SYS_OFF_RESTART_PREPARE` | before a restart | shut down, may block |
| `PBL_SYS_OFF_POWER_OFF_PREPARE` | before a power-off | shut down, may block |
| `PBL_SYS_OFF_RESTART` | after the prepare handlers | restart the system |
| `PBL_SYS_OFF_POWER_OFF` | after the prepare handlers | power off the system |
| `PBL_SYS_OFF_POWER_CUT` | after the power-off prepare handlers | cut the power of the system |

Power-cut handlers, defined with `PBL_SYS_OFF_POWER_CUT_HANDLER_DEFINE()`,
may also have a check, which `pbl_power_cut()` calls before tearing anything
down: if a handler can't cut the power now, e.g. a PMIC whose ship mode would
end at once with VBUS present, `pbl_power_cut()` returns its error and the
caller keeps running.

Handlers of a mode run highest priority first. A restart, power-off or power-cut
handler that returns has declined or failed, and the next one runs; once none is
left, the kernel powers off or resets the SoC. A failed power cut always resets,
never powering off instead.

Prepare handlers only run on the first request, made from a thread with
interrupts and the scheduler unlocked. A request from an interrupt, with
interrupts locked or from within a handler skips them and goes straight to the
restart, power-off or power-cut handlers, which therefore run in the context of
the request and must check it before blocking.

The final reset is the SoC's `pbl_soc_reset()` when it selects
`CONFIG_SOC_HAS_RESET`, e.g. SF32LB52 resets through its PMU and the POSIX host
restarts the process; otherwise the architecture's, the NVIC system reset on
Cortex-M.

The final power-off is the SoC's `pbl_soc_power_off()` when it selects
`CONFIG_SOC_HAS_POWER_OFF`, which leaves on only the wake sources drivers have
armed; otherwise the SoC is reset.

The firmware's handlers:

| Handler | Mode | Priority |
|---------|------|----------|
| services to `RunLevel_BareMinimum` (Bluetooth off) | prepare | high |
| wait for flash erases, then hold flash | prepare | default |
| clear and disable the displays | power-off prepare | default |
| mark the reboot reason as a safe restart | prepare | low |
| core dump after a software failure | restart | high |
| nPM1300 ship mode | power cut | default |
| arm the GPIO wake lines | power-off | low |

Reboot reasons and boot bits stay firmware policy: callers set them before
calling into the kernel.
