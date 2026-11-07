# Fuel gauge watch logs

A watch's own battery measurements give a log in the
[characterization](characterization.md) format. They are the
best check of a model, since they come through the watch's own measurement
path, at the watch's currents. They are a coarse source for a fit, though:
one sample a minute of a bursty load, with a 5 mV voltage resolution.

## Capturing

Build the firmware with the open fuel gauge and verbose battery logs, and
record the console through a full charge and a full discharge:

```shell
pbl configure --board getafix@dvt2 \
    -DCONFIG_BATTERY_SOC_FUEL_GAUGE=y \
    -DCONFIG_SERVICE_BATTERY_LOG_LEVEL_DEBUG_VERBOSE=y
pbl build
script -q watch.log pbl console           # macOS
script -q -c "pbl console" watch.log      # Linux
```

The console's log level filters verbose messages out, so send
`log level pulse 255` after every boot to see the battery state lines.
Release builds have no console, so use a debug build. The backend logs every
sample as two lines, the measurements then the estimate:

```
Battery state: v_mv: 4012, i_ua: 1500, t_mc: 24500, td_ms: 60000, fg: 0
Battery estimate: soc: 80, tte: 0, ttf: 0
```

`extract` turns the `Battery state` lines into a CSV log, which `replay`
takes like a bench log:

```shell
M=fw/services/battery/fuel_gauge/models/getafix.c
python tools/fuel_gauge/fuel_gauge.py extract watch.log watch.csv
python tools/fuel_gauge/fuel_gauge.py replay $M watch.csv -o soc.csv
```

A full charge followed by a discharge to shutdown can also be fitted like
bench data, with `--start-soc` and `--end-soc` matching the capture.

## Checking a model

Compare the replayed SOC with the charge counted from a known point, such as
charger termination (full) or the shutdown at 0%. Things to look for:

- **Charging lag.** If the SOC lags the charged amount during
  constant-current charging, the model's $R_0$ is too high for the watch,
  e.g. because the bench fixture's resistance is in the fitted value.
- **Discharge.** At watch currents of a few milliamps, the resistances
  barely matter and the OCV curve dominates. A level stuck at 100% after
  unplugging means the curve tops out below the watch's full voltage, e.g.
  because the bench charge didn't complete.
- **Absolute current.** At a few milliamps, the PMIC's current readings are
  coarse and read high, so counting with them overestimates the charge
  drawn. Normalize to a known start and end rather than trusting the total.
