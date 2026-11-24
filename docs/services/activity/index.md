# Activity

The activity service counts steps and tracks sleep from the
accelerometer. It lives in `pebble/services/activity/`, and apps reach it
through the Health API. The algorithms it runs are libraries:
{doc}`../../libraries/kraepelin` counts steps and builds sleep sessions, and
{doc}`../../libraries/vanhees` tells sleep minutes apart. The sleep threshold
of Kraepelin can be selected instead of van Hees in Settings > System >
Debugging > Sleep Algorithm; both see every sample, so the setting applies
from the next minute on, to a sleep session in progress too.

## Implementation

### Code organization

```text
pebble/services/activity/
  activity.c
  activity_insights.c
  kraepelin/
    activity_algorithm_kraepelin.c
lib/kraepelin/
  kraepelin.c
lib/vanhees/
  vanhees.c
```

`activity.c`
: The service's API, and the glue around the algorithms. It holds no
  algorithm-specific code, so another step or sleep algorithm could
  replace Kraepelin with few changes here.

`activity_insights.c`
: The Health timeline pins and notifications.

`lib/kraepelin/kraepelin.c`, `lib/vanhees/vanhees.c`
: The algorithms themselves; see {doc}`../../libraries/kraepelin` and
  {doc}`../../libraries/vanhees`.

`kraepelin/activity_algorithm_kraepelin.c`
: Adapts the algorithms to the algorithm interface `activity.c` expects,
  and handles their memory, persistent storage and the rest of their
  integration. Another algorithm would implement the same interface.

The Health API for apps is `pebble/applib/health_service.c`, which runs
unprivileged and calls into the activity service for the step and sleep
data.

### Steps

`activity.c` asks the algorithm for its accelerometer sampling rate and
subscribes to the accel service with it. All step and sleep processing
runs on the SysWorkq task: the subscription is made from a
SysWorkq callback.

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

The unit tests in `tests/pebble/services/activity/` run recorded data through
`lib/kraepelin/kraepelin.c` and check its results. The van Hees classifier
is tested with generated samples: the recorded sleep samples are minute
data, which it cannot use.

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
