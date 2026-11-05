# SF32LB52 RC10K

The SF32LB52 can run its low-power clock from a 32.768 kHz crystal. Without
one, the low-power clock is the RC10K, an RC oscillator that keeps running
in deep sleep. Its real rate is only known by measurement. It drives the
system clock source (LPTIM1), the calendar RTC and the LPTIM PWM, so every
error in its assumed rate is an error in the kernel's time.

`drivers/clk/sf32lb_rc10k.c` provides it as a {doc}`calibrated clock
<index>`, `SF32LB_RC10K`. This page describes the hardware, the oscillator
as measured on a watch, the vendor algorithm it replaces and the results.

## Hardware

The Bluetooth controller's RCCAL counter counts 48 MHz crystal cycles over
$N$ RC10K cycles (`CONFIG_CLK_SF32LB_RC10K_CAL_GATE_CYCLES`, 2000 by default,
207 ms). The counter resolution is 0.1 ppm.

The LCPU calibrates the RC10K on its own with the same counter, for its
Bluetooth sleep timing, and never uses this estimate. A mailbox mutex
serializes the two: the driver retries a second later if the LCPU holds it,
and the LCPU, finding it taken, retries from its next idle hook. For the
duration of a gate the driver keeps the low-power domain awake and the
crystal requested, and lets the CPU sleep. Calibration does not depend on
Bluetooth: if the LCPU has not moved the low-power domain to the crystal, the
driver does, and leaves it there, as the LCPU may start during the gate.

The rate starts at a nominal 10 kHz and gates are retried every second until
one succeeds. That first gate replaces the nominal rate outright, however far
from it: the nominal rate is only a placeholder, and results are never judged
against it. On one watch, the RC10K ran at 9658 Hz before a power cycle and
9555 Hz after it. A gate is timed in kernel time, itself counted on the RC10K,
so it lasts the right number of cycles whatever rate is assumed.

The system clock source and the RTC follow the rate through notifiers; the
RTC prescaler can be rewritten while the calendar runs, so the calendar keeps
its time across updates. The PWM reads the rate when it programs its
prescaler.

## The oscillator

### White frequency noise

Consecutive gates scatter with a two-sample deviation of
$7.2\,\mathrm{ppm}\cdot\sqrt{\mathrm{s}} / \sqrt{T}$, white frequency noise:
50 ppm for a 200-cycle gate, 16 ppm for 2000 cycles.

```{figure} noise.png
:alt: Measurement noise against gate length, following 1/sqrt(T).
```

### Wander, load, temperature and supply

Averaged over 30 s, the frequency still wanders by about ±8 ppm on a
timescale of a minute or two. It also steps by about −20 ppm when the CPU
goes from idle to busy and back when it idles again. The die warms by only
1.2 °C meanwhile: the step follows the load, not the temperature. Moving the
battery voltage between 3.5 and 4.2 V has no visible effect.

```{figure} wander.png
:alt: RC10K offset over 36 minutes with a busy CPU period, die temperature below.
```

### Deep sleep

To see the frequency in deep sleep, the kernel rate was frozen and the watch
left idle, waking for a few seconds a minute to take one 4000-cycle gate,
with the CPU spinning, and report its time to the host. The slope of the
kernel's time against the host's is the true mean rate.

```{figure} sleep.png
:alt: Awake measurements scatter above the true mean rate when mostly asleep.
```

Mostly asleep, the true mean rate is 20 ppm below the mean of the awake
gates (±2 ppm over 60 minutes). With the CPU busy throughout, the two agree
within 6 ppm. The RC10K runs slower asleep and under load than in the
lightly loaded state a calibration runs in.

### Parameters

From these measurements: `wfm` 7.2 ppm·√s, `wander` 8 ppm and `rw`
1.1 ppm/√s. With a 2000-cycle gate once a minute, the steady state is
$K = 0.38$, $\tau = 2.1$ minutes and $\sigma = 10.9$ ppm:

| Gate | Period | $K$ | $\tau$ | Estimate σ | Average current |
| --- | --- | --- | --- | --- | --- |
| 200 cycles (vendor EWMA) | 60 s | 0.125 | 7.5 min | 13.1 ppm | |
| 2000 cycles | 60 s | 0.38 | 2.1 min | 10.9 ppm | 2.1 µA |
| 4000 cycles | 60 s | 0.46 | 1.6 min | 9.3 ppm | 4.3 µA, scaled |

A gate draws about 580 µA for its duration: 128 µC for 2000 cycles,
measured with a PPK2 at 3.8 V.

## The vendor algorithm

The vendor algorithm measures a 200-cycle gate once a minute, with the CPU
spinning for the 21 ms, and keeps an exponential average of the counts in
integer arithmetic:

$$
a_k = a_{k-1} + \tfrac{1}{8}(c_k - a_{k-1}),
$$

switching to a gain of $13/16$ after two consecutive samples beyond +250 or
−200 counts. With 50 ppm per sample, the average settles to 13 ppm of noise
with a time constant of 7.5 minutes. It also:

- never resets its failure counter, so its recovery from a stuck estimate
  works once per boot;
- rejects samples more than 5000 counts from the average, so a large enough
  step freezes the estimate for good;
- rounds the update to whole counts, a dead band of up to 4 ppm;
- starts each boot from one raw sample and, after a power loss, from a
  nominal 10 kHz, 3.5 % off.

## Results

### Replay

Replayed against the measured trajectory with one update a minute, the
native filter's time error stays within 6 ms over 35 minutes (median of 200
noise realizations; 10 ms at the 90th percentile), against 17 ms (37 ms) for
the vendor algorithm. Most of the difference is the load step, which the
vendor average takes a quarter of an hour to follow.

```{figure} replay.png
:alt: Vendor and native estimates following the measured frequency, and the resulting time error.
```

Over a day, the estimate noise accounts for 0.05 s with the 2000-cycle gate
and 0.12 s with the vendor algorithm. The sleep bias dominates both.

### On the device

An idle watch reported its time to the host every five minutes for two
hours, once with each algorithm. With the 2000-cycle gate once a minute, it
ran 9.9 ppm slow (0.85 s a day), within 2.7 ms rms of a straight line. With
the vendor algorithm, it ran 13.6 ppm slow (1.17 s a day), within 7.4 ms rms.

```{figure} device.png
:alt: Kernel time against the host over two hours, native and vendor calibration.
```

The CPU sleeps through the driver's gates, so they see less of the awake
bias than the spinning gates above: half of it remains. Correcting it needs
a reference while the watch runs, such as the phone's time.

Both algorithms cost about the same: the vendor gate is ten times shorter,
but the CPU spins through it, adding about 130 µC to the minute's activity.
