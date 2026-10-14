# Activity

The activity service counts steps and tracks sleep from the
accelerometer. It lives in `fw/services/activity/`, and apps reach it
through the Health API. This page describes the algorithms, called
Kraepelin, and how they are integrated into the firmware. The plots below
are drawn from the step samples the unit tests use.

## Step counting

The accelerometer measures acceleration on three axes. A watch lying flat
and still reads 1 G on the z axis, from gravity, and 0 G on x and y;
tilting it moves that 1 G to another axis. While the watch moves, all
three readings vary with its orientation and with its own acceleration,
and the pattern of those variations reveals whether, and how fast, the
user is stepping.

Walking produces two dominant signals: a spike each time a foot hits the
ground, more or less pronounced depending on the shoes and the floor, and
the swing of the arm, which depends on the user's walking style and on
whether their hand is in a pocket or carrying something. The foot fall is
the reliable one, as users do not always swing their arms, so the
algorithm isolates it from arm swings and other movements:

1. It splits the accelerometer readings into 5 second epochs.
2. For each epoch, it computes an FFT to get the energy of the signal at
   each frequency, its spectral density.
3. It looks for the foot fall signal in the spectral density with a set
   of heuristics, and finds its frequency.
4. The foot fall frequency gives the number of steps in the epoch: a
   signal at 2 Hz is 10 steps in 5 seconds.

### Signals

The following epoch of a walk shows the raw readings of each axis. Its
dominant rhythm is the arm swing: there is one arm swing cycle for every
two steps, and here the arm swing is strong enough to hide the foot falls.

```{eval-rst}
.. plot::
   :filename-prefix: activity-raw

   from activity_plots import plot_raw
   fig, ax = plt.subplots(figsize=(8, 3.2))
   plot_raw(ax, "walk_100_pbl_25663_6", 7)
```

The spectral density of the same walk, averaged over the whole sample to
be less noisy than a single epoch, shows the energy at each frequency.
Each axis, and their combination, is plotted relative to its own average
power. The combined signal is the magnitude of the three axes at each
frequency $f$:

```{math}
S[f] = \sqrt{X[f]^2 + Y[f]^2 + Z[f]^2}
```

```{eval-rst}
.. plot::
   :filename-prefix: activity-spectral-density

   from activity_plots import plot_spectral_density
   fig, ax = plt.subplots(figsize=(8, 3.2))
   plot_spectral_density(ax, "walk_100_pbl_25663_6")
```

The arm swing dominates at 1 Hz, five swings in five seconds. The steps
show at 2 Hz, together with the second harmonic of the arm swing, and the
energy below 0.5 Hz comes from random arm movements rather than stepping.

### Vector magnitude counts

Vector magnitude counts (VMC) measure the overall amount of movement over
a period: 0 for a still watch, and more the more it moves, so running
yields more than walking. They were developed with the Stanford Wearables
Lab and calibrated against the
[ActiGraph](https://ametris.com/actigraph-leap) wrist-worn device, which is
widely used in medical research.

Each axis is first band-pass filtered between 0.25 Hz and 1.75 Hz. For $N$
filtered samples $\tilde{x}_n$, $\tilde{y}_n$ and $\tilde{z}_n$, with a
calibrated scaling factor $k$:

```{math}
\mathrm{VMC} = k \sqrt{\Big(\sum_{n=1}^{N} |\tilde{x}_n|\Big)^2
  + \Big(\sum_{n=1}^{N} |\tilde{y}_n|\Big)^2
  + \Big(\sum_{n=1}^{N} |\tilde{z}_n|\Big)^2}
```

The accelerometer is sampled at 25 Hz, so one second of data is 25 samples
per axis. The step algorithm computes a VMC over each 5 second epoch. It
also computes one per minute, which it stores and sends to data logging,
so it reaches the phone; apps read the minute values through the Health
API.

### Step identification

#### Spectral density

Each 5 second epoch holds 125 samples per axis. For the FFT, each axis has
its mean subtracted and is zero-padded to 128 samples. The FFT of 128 real
samples gives 64 frequencies; the amplitude of axis $a$ at frequency index
$k$ is

```{math}
A_a[k] = \Big|\sum_{n=0}^{127} a_n\, e^{-2\pi i k n / 128}\Big|,
\qquad k = 0, \dots, 63
```

and the three axes combine into the energy at that frequency:

```{math}
E[k] = \sqrt{A_x[k]^2 + A_y[k]^2 + A_z[k]^2}
```

Index $k$ is $k$ cycles per epoch, $k / 5$ Hz: index 0 is the DC
component, and a user walking 9 steps every 5 seconds shows as a peak at
index 9, 1.8 Hz. This epoch of a walk with little arm swing has a clear
peak there:

```{eval-rst}
.. plot::
   :filename-prefix: activity-fft-walk

   from activity_plots import plot_fft
   fig, ax = plt.subplots(figsize=(8, 3))
   plot_fft(ax, "walk_100_pbl_25655_4", 7)
```

#### Stepping frequency

The naive approach takes the frequency with the most energy, which works
for the epoch above. For some users, though, the arm swing is as strong as
the steps or stronger, at half their frequency, so a quick walk can be
mistaken for a slow one. In this epoch, the steps show at indices 9 and
10, but the arm swing at index 5 is larger:

```{eval-rst}
.. plot::
   :filename-prefix: activity-fft-arm-swing

   from activity_plots import plot_fft
   fig, ax = plt.subplots(figsize=(8, 3))
   plot_fft(ax, "walk_100_pbl_25663_6", 7)
```

The VMC narrows down where the steps can be. Based on it, the algorithm
searches one of three frequency ranges, from a slow walk for a low VMC to
a run for a high one, and takes the frequency with the most energy in it
as the candidate.

Noise can put the maximum one or two indices off, or make neighbours
nearly equal, as at indices 9 and 10 above. The algorithm refines the
candidate with the harmonics, which carry significant energy for both the
steps and the arm swing: for each stepping frequency $s$ within two
indices of the candidate, it adds up the energy of the steps, of the arm
swing at half their frequency, and of their second to fifth harmonics,

```{math}
C(s) = \sum_{h=1}^{5} \Big( E[h s] + E\big[h \tfrac{s}{2}\big] \Big)
```

and keeps the frequency with the highest total.

#### Stepping or not

That gives the stepping frequency if the user is walking; four checks
decide whether they are. With $\mathcal{S}$ the frequencies of the steps,
the arm swing and their harmonics:

1. The _walking score_, the share of the energy in $\mathcal{S}$, must be
   high:

   ```{math}
   W = \frac{\sum_{k \in \mathcal{S}} E[k]}{\sum_{k} E[k]}
   ```

2. The VMC must be above a _minimum stepping VMC_, which is higher for a
   faster stepping rate.

3. The energy at very low frequencies must be low relative to the
   signal:

   ```{math}
   R_\mathrm{low} = \frac{\sum_{k=0}^{4} E[k]}{\sum_{k \in \mathcal{S}} E[k]}
   ```

   must be below the _low frequency ratio_ threshold. Many non-stepping
   epochs fail this check, like this one:

   ```{eval-rst}
   .. plot::
      :filename-prefix: activity-fft-non-walk

      from activity_plots import plot_fft
      fig, ax = plt.subplots(figsize=(8, 3))
      plot_fft(ax, "non_walk_pbl_25664_14", 38)
   ```

4. The energy at high frequencies must be low relative to the signal:

   ```{math}
   R_\mathrm{high} = \frac{\sum_{k \ge 50} E[k]}{\sum_{k \in \mathcal{S}} E[k]}
   ```

   must be below the _high frequency ratio_ threshold. This keeps driving
   from counting as steps, as the engine's vibration shows at those
   frequencies:

   ```{eval-rst}
   .. plot::
      :filename-prefix: activity-fft-driving

      from activity_plots import plot_fft
      fig, ax = plt.subplots(figsize=(8, 3))
      plot_fft(ax, "driving_1", 9)
   ```

#### Partial epochs

An epoch in which the user starts or stops walking usually fails the
checks above, and its steps would be lost. A _partial epoch_ only needs a
walking score and a VMC above lower thresholds of their own, with no
frequency ratio checks. When a partial epoch is next to a stepping epoch,
it adds half the steps of that epoch, which evens out the undercounting at
the start and end of a walk. A walk shorter than two epochs can still go
uncounted.

## Sleep tracking

Sleep tracking works on two values per minute: the VMC, and the average
orientation of the watch. The orientation is quantized to 8 bits, computed
from the average reading of each axis over the minute: the angle in the
X-Y plane in the lower 4 bits, and the angle of that vector with the Z
axis in the upper 4.

### Sleep detection

A minute is a _sleep minute_ when its _filtered VMC_, a convolution of the
VMC over the 4 minutes before and after it, is below the sleep threshold.

1. Sleep starts after at least 5 sleep minutes in a row.
2. Sleep continues until at least 11 non-sleep minutes in a row.
3. A sleep session must last at least 60 minutes.

With some exceptions:

- A minute with an exceptionally high filtered VMC ends the session
  immediately.
- In the first 60 minutes of a session, 14 non-sleep minutes in a row are
  needed to end it instead of 11.
- If at least 80% of the minutes show slight movement, even below the
  non-sleep threshold, the user is considered awake.
- If the watch was not worn, the session is discarded; see below.

### Restful sleep

A second pass over a sleep session finds its _restful sleep_. A _restful
sleep minute_ has a filtered VMC below the restful sleep threshold, lower
than the sleep one.

1. Restful sleep starts after at least 20 restful sleep minutes in a row.
2. It continues until a minute that is not a restful sleep minute.

### Not worn

A watch that is not worn looks asleep: it barely moves, and its VMC stays
near 0. Every possible sleep session therefore goes through not-worn
detection.

A _not worn minute_ is one where either:

- the raw VMC is below the not-worn threshold and the orientation is the
  same as in the previous minute; or
- the watch is charging.

The watch is considered not worn when both:

1. there are at least 100 not worn minutes in a row in the session; and
2. that run starts within 20 minutes of the start of the session, and ends
   within 10 minutes of its end.

Restful sleep can approach 100 minutes, so the run has to be that long.
The orientation check handles a watch on a table that picks up the
occasional vibration: the VMC is then not zero, like the small movements
of sleep, but a sleeping user is likely to change position, and so the
watch's orientation.

## Implementation

### Code organization

```text
fw/services/activity/
  activity.c
  activity_insights.c
  kraepelin/
    kraepelin_algorithm.c
    activity_algorithm_kraepelin.c
```

`activity.c`
: The service's API, and the glue around the algorithms. It holds no
  algorithm-specific code, so another step or sleep algorithm could
  replace Kraepelin with few changes here.

`activity_insights.c`
: The Health timeline pins and notifications.

`kraepelin/kraepelin_algorithm.c`
: The step and sleep algorithms themselves. It depends on almost nothing
  outside itself, and originates from open source code provided by the
  Stanford Wearables Lab.

`kraepelin/activity_algorithm_kraepelin.c`
: Adapts the algorithms to the algorithm interface `activity.c` expects,
  and handles their memory, persistent storage and the rest of their
  integration. Another algorithm would implement the same interface.

The Health API for apps is `fw/applib/health_service.c`, which runs
unprivileged and calls into the activity service for the step and sleep
data.

### Steps

`activity.c` asks the algorithm for its accelerometer sampling rate and
subscribes to the accel service with it. All step and sleep processing
runs on the KernelBackground task: the subscription is made from a
KernelBackground callback.

The accel service calls `activity.c` once a second with 25 samples, which
it passes to the algorithm's handler in `activity_algorithm_kraepelin.c`.
That handler runs the step algorithm and adds the steps it returns to the
total. Since the algorithm works on 5 second epochs, it only returns steps
on every fifth call.

The running total is kept by `activity_algorithm_kraepelin.c`, which
returns it without calling into the algorithm, and is reset at midnight.

### Sleep

The sleep algorithm takes an array of VMC and orientation values, one pair
per minute, and returns the sleep sessions as indices into it.
`activity_algorithm_kraepelin.c` builds that array: it periodically stores
each minute's VMC and orientation in persistent storage, where apps can
also read them through the Health API, and reads them back to run the
algorithm.

`activity.c` recomputes sleep every 15 minutes. Each time, the last 36
hours of minute data are passed to the algorithm. Sleep for the current
day includes every session that ends after midnight, even if it started
before, so 36 hours reach back to a session that started at 6 PM the day
before when sleep is recomputed at 11:59 PM.

To keep memory low, each minute's 16-bit stored VMC is compressed to one
byte by taking its square root, and the orientation is one byte as well:
36 hours take 36 × 60 × 2 = 4320 bytes. The algorithm only knows indices;
`activity_algorithm_kraepelin.c` translates them into timestamps.

## Testing

The unit tests in `tests/fw/services/activity/` run recorded data through
`kraepelin_algorithm.c` and check its results.

### Step samples

`tests/fixtures/activity/step_samples/` holds raw accelerometer recordings,
as x, y and z tuples. The files have C syntax but are parsed by the tests
at run time. Each sample carries the expected number of steps and the
accepted range, and the tests also report the overall error across all
samples, to compare changes when tuning the algorithm.

Samples are captured with a special mode of the activity service, turned
on from the debug menu of the `Activity Demo` app. In that mode the
service sends raw samples to data logging and also writes them, base64
encoded, to the logs, so they come with a support request from the mobile
app. `tools/activity/parse_activity_data_logging_records.py` extracts them
from a log or from data logging records into a file the tests can read.

### Sleep samples

`tests/fixtures/activity/sleep_samples/` holds minute data, with bounds
for each sleep metric the algorithm computes: total sleep, restful sleep,
start and end times. The `Activity Demo` app can dump the last 36 hours of
minute data to the logs, and the same script extracts it.

Minute data can be captured up to 36 hours after a sleep problem shows,
as the watch always stores it. Raw accelerometer data, too expensive in
memory and power to record all the time, has to be captured before the
activity it is about.
