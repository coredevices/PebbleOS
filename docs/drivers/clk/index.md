# Clocks

A clock controller (`include/pbl/drivers/clk.h`) is a device that provides
a set of clocks, each named by a number from the controller's header. A
consumer holds a `struct pbl_clk` for each clock it uses.

## Gating

`pbl_clk_enable()` and `pbl_clk_disable()` take and drop a reference on a
clock. The controller ungates it for the first user and gates it after the
last, so drivers that share a clock never gate it under each other. Clocks
that always run leave the `enable` and `disable` operations unset.

## Rates

`pbl_clk_get_rate()` returns a clock's rate in millihertz, the resolution a
calibrated low-frequency oscillator needs: 1 mHz at 10 kHz is 0.1 ppm.
A consumer whose hardware depends on the rate reads it when it programs the
hardware. If the rate can change at run time and the consumer must follow
it, the consumer registers a `struct pbl_clk_notifier`. The provider reports
each change with `pbl_clk_rate_changed()`, which calls the notifiers in the
provider's context, possibly an interrupt, so they must not block.

## Calibrated clocks

Some oscillators are only known by measurement, such as an RC oscillator
standing in for a missing 32 kHz crystal. `include/pbl/drivers/clk/cal.h`
tracks the rate of such a clock: the provider implements two operations,
starting a gate of a given number of oscillator cycles counted against a
reference clock and reading its result, and the module does the rest. It
runs a gate every period from a kernel timer, lets the CPU sleep through
each one, filters the results and reports the new rate to the notifiers.
Until the first gate succeeds, the rate is the nominal one given by the
provider and gates are retried every second; the first result replaces it.

The filter is a library of its own, `include/pbl/util/freq_track.h`, with no
hardware dependencies. The rest of this section describes it and how to
choose its parameters for a new oscillator.

### Measurement model

A gate of $N$ cycles of an oscillator of frequency $f$ lasts $T = N / f$. If
the reference, of frequency $f_\mathrm{ref}$, counts $c$ cycles over it,

$$
\hat f = \frac{f_\mathrm{ref}\, N}{c}.
$$

The counter resolution, one reference cycle, is
$1 / (f_\mathrm{ref} T)$ in fractional terms, usually negligible. What is
not is the oscillator's own noise. Three parameters describe it, all
fractional:

- `wfm`, white frequency noise. The two-sample deviation of consecutive
  gates falls as $\sigma_y(T) = \mathrm{wfm} / \sqrt{T}$; averaging gates or
  lengthening them is equivalent.
- `wander`, the fluctuation on timescales between the gate and the period,
  which a short gate samples at one instant. From the filter's point of
  view, it is measurement noise.
- `rw`, the random walk density of the frequency itself: its drift with
  temperature, load and age, which the filter must follow.

### Estimator

The tracker is a scalar Kalman filter: the frequency random walks and each
gate observes it with white noise,

$$
f_k = f_{k-1} + w_k, \quad
w_k \sim \mathcal{N}\!\left(0,\ \mathrm{rw}^2 f^2\,\Delta t\right), \qquad
\hat f_k = f_k + v_k, \quad
v_k \sim \mathcal{N}\!\left(0,\ r\, f^2\right), \quad
r = \frac{\mathrm{wfm}^2}{T} + \mathrm{wander}^2 .
$$

Each gate updates the estimate with the gain

$$
K_k = \frac{P_{k-1} + \mathrm{rw}^2 f^2 \Delta t}
           {P_{k-1} + \mathrm{rw}^2 f^2 \Delta t + r f^2},
\qquad
f_k = f_{k-1} + K_k (\hat f_k - f_{k-1}),
\qquad
P_k = (1 - K_k)\left(P_{k-1} + \mathrm{rw}^2 f^2 \Delta t\right).
$$

A gate further than `reject_sigma` standard deviations from the prediction
is dropped, `max_rejects` in a row at most; then the filter restarts from
the next one, so a real step is never locked out. The state is fractional
and floating point: no dead band, no integer rounding.

### Steady state

For a fixed period $\Delta t$, the gain converges to the steady state of the
Riccati recursion. In fractional units, with $Q = \mathrm{rw}^2 \Delta t$:

$$
M = \tfrac{1}{2}\left(Q + \sqrt{Q^2 + 4 Q r}\right), \qquad
K = \frac{M}{M + r}, \qquad
\sigma = \sqrt{(1 - K) M}, \qquad
\tau = \frac{-\Delta t}{\ln(1 - K)} .
$$

$M$ is the prior variance, $\sigma$ the noise of the estimate and $\tau$ the
time constant with which it follows a step. The unit tests check the filter
against these values.

### Time error

The kernel's time error is the integral of the fractional rate error. Two
parts contribute differently:

- Estimate noise is correlated over about $\tau$, so the time error it
  accumulates over a duration $D \gg \tau$ grows as
  $\sqrt{2 \sigma^2 \tau D}$, slowly.
- A bias, a difference between the rate the gates see and the mean rate the
  clock runs at, accumulates linearly: 1 ppm is 86 ms a day. No filtering
  removes it.

### Characterizing an oscillator

The parameters come from measurements on the device. With
`CONFIG_CLK_SHELL`, the `clk cal` shell command takes them, and
`tools/clk_characterize.py` turns its output into parameters. A calibrated
clock is named after its controller.

1. **White noise.** Take raw gates of several lengths, then fit:

   ```
   clk cal sample <name> 200 100
   clk cal sample <name> 2000 100
   clk cal sample <name> 20000 30
   ```

   `clk_characterize.py noise <log>` fits $\sigma^2 = \mathrm{wfm}^2 / T$
   plus a floor, the fluctuations a longer gate does not average.
2. **Wander and drift.** Take short gates for half an hour or more while the
   load, the temperature and the supply change, e.g.
   `clk cal sample <name> 2000 1000 2000`. `clk_characterize.py drift <log>
   --wfm <wfm>` gives `wander`, the spread of single gates around their mean
   over one period, and `rw`, from the Allan deviation at longer averaging
   times.
3. **Bias.** Hold the rate with `clk cal hold <name> <mHz>` and let the device
   run as it normally does, reading `clk cal status`, which prints the
   uptime, now and then against the host clock. `clk_characterize.py bias
   <csv>` fits the true mean rate; its difference from the mean of the gates
   is the bias. `clk cal hold <name> 0` releases the rate.
4. **Choice.** `clk_characterize.py steady` gives the gain, time constant,
   estimate deviation and the time error they cause for a gate length and
   period. Replaying the estimator over the trajectory from step 2 shows how
   it follows steps.

The power cost is the reference clock and the counter running for the
duration of the gates: it is proportional to the duty cycle, gate length
over period, and the estimate noise to its inverse square root.

## Implementations

```{toctree}
:maxdepth: 1

sf32lb_rc10k.md
```

The {apidoc}`API reference <drivers_clk>` documents the interface, and
{apidoc}`tracker reference <util_freq_track>` the tracker.
