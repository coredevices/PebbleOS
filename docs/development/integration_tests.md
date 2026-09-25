# Integration tests

Integration tests run the firmware as it ships, on the emulator or on a
real watch, and drive it from the outside: pressing buttons, comparing
screenshots, reading logs and measuring current. They are written in
Python with [pytest](https://docs.pytest.org) and live in
`tests/integration`, next to the harness that runs them
(`tests/integration/harness`). Unit tests, which build parts of the
firmware for the host, are described in [](testing.md).

## Running

`pbl itest` runs the suite against the configured build. On an emulated
board it builds the flash images, boots QEMU headless on a private copy of
them, and shuts it down when the tests are done:

```shell
pbl configure --board qemu_emery
pbl build
pbl itest
```

Anything `pbl itest` does not recognize goes straight to pytest, which it
runs from `tests/integration`, so the usual selection options work:

- `-m smoke`, `-m "ui and not slow"`: by category
- `-k settings`: by name
- `ui/test_navigation.py::test_settings`: a single file or test
- `--collect-only -q`: list the tests without running them

`pbl itest` is only a convenience; the same run is

```shell
cd tests/integration
pytest --build-dir=../../build
```

When calling pytest directly, pass paths as `--option=value`: pytest
reads a bare path argument (a build directory, a tty) as a test path before
it has loaded the harness's options. `pbl itest` loads the harness up front
and has no such limitation.

### On a real watch

Give the debug console's serial port, and optionally have the harness flash
the build first (with `pbl flash --resources`, or `--flash-command`):

```shell
pbl -b build-obelix itest --device-serial /dev/tty.usbserial-1 \
    --ppk2 auto --flash-before
```

The device type follows the board (`qemu` for emulated boards, `hardware`
otherwise), and can be forced with `--device-type`.

Flash with a PPK2 powering the watch (see [](#current-measurement)): the
harness then repowers the watch and flashes it right away, well within a
second, before the firmware can deep sleep, which leaves its debug UART
reachable only at random.

`--erase-fs` erases the watch's filesystem first (bondings, settings, apps
and data) and the bonding kept for PRF, for a known starting point; it
needs a board flashed with sftool.

The integration tests run debug builds, including the current measurements
(see below): release builds have no usable console.

### Setups and lab files

The tests run with a setup: the device, how it is reached and powered, and
what plays the phone. The same tests run on any of:

| Watch | Phone | How |
|---|---|---|
| Emulator | Bumble, software controllers | a `CONFIG_BT_HCI_UART` build, by default |
| Emulator | Bumble, on a dongle | `--qemu-bt-hci lab`: the lab's first dongle for the watch, its second for the phone |
| Watch, serial console | Bumble, on a dongle | the lab's watch, and its first dongle for the phone |
| Watch, serial console | CoreApp | `--phone coreapp` (not supported yet) |

A lab file describes the hardware wired to this host, and nothing else:
watches (board, serial port, the supply powering them), power supplies and
Bluetooth dongles. Pass it with `--lab` (or `$PBL_ITEST_LAB`), and the
harness takes the setup for the build from it: the watch running its
board (`--lab-watch` picks another), its supply, and a dongle for the
phone. Options on the command line override it.
`tests/integration/lab.example.yaml` describes the format:

```shell
pbl -b build-getafix itest --lab ~/pebble-lab.yaml --flash-before
```

The report starts with the setup, e.g. `setup: getafix, serial
/dev/tty.wchusbserial1, PPK2 auto at 3800 mV, phone bumble on
/dev/cu.usbmodem1101`. Tests that need what the setup lacks are skipped
with the reason, without touching the device: a test needs a phone when it
takes `phones`, and a power supply when it takes `power`.

### Connections

The harness reaches the watch through one or more connections, each
offering some of: the debug prompt, the log stream, and the Pebble protocol
(what the phone app speaks, used for input, screenshots and blob DB). Each
is served by the first connection that has it. By default the harness uses
the debug console, over PULSE when `CONFIG_PULSE_EVERYWHERE` is set and as
the legacy text console otherwise; `--connection` replaces the default and
may be repeated:

| Connection                  | Prompt | Logs     | Pebble protocol |
| --------------------------- | ------ | -------- | --------------- |
| `pulse:TTY` / `pulse:socket://HOST:PORT` | yes | yes | yes (tunneled) |
| `serial:TTY[@BAUD]`         | yes    | yes      | no              |
| `ble:WATCH[@CONTROLLER]`    | no     | no       | yes             |
| `devconn:HOST[:PORT]`       | no     | app only | yes             |
| `qemu:HOST:PORT`            | no     | no       | yes             |

For example, a watch over Bluetooth through the phone app's developer
connection, with logs and the prompt over serial:

```shell
pbl itest --connection serial:/dev/tty.usbserial-1 --connection devconn:192.168.1.20
```

Tests that need something no connection offers are skipped, e.g. the
`prompt` fixture without a prompt.

### Bluetooth

Bluetooth goes through nRF52840 dongles running Zephyr's `hci_uart`
sample, an H4 controller on the dongle's USB serial port; see [](qemu.md)
for building and flashing it. On macOS use the `cu.*` device.

`ble:` connects to the watch as the phone app would: it pairs (or encrypts
with the bond it keeps, in `~/.cache/pbl-itest`), then speaks the Pebble
protocol over reversed PPoGATT, through a controller
[Bumble](https://google.github.io/bumble/) drives. `WATCH` is the watch's
address, its advertised name, or `auto` for the first watch advertising;
the controller is a dongle's serial port, or `--ble-controller`:

```shell
pbl itest --connection ble:auto@/dev/cu.usbmodem1101 \
    --connection pulse:/dev/tty.usbserial-1
```

Pairing has to be confirmed on the watch; the harness does it with the
prompt when another connection offers one, so list the serial connection
too. A phone bonded to the watch reconnects to it before the harness can:
turn its Bluetooth off. The harness keeps the connection parameters it
opens the link with and declines the watch's requests to change them:
those updates stall the watch's sending for seconds, and some fail and
drop the link.

An emulator built with `CONFIG_BT_HCI_UART` needs a controller of its own.
By default it gets Bumble's software controllers (`virtual`), two linked
in memory, one for the watch and one for the harness: they cover the host
stacks and the protocols above them, not a radio, and are what CI uses.
With real ones it takes two dongles, one for the watch and one for the
harness: the lab's (`--qemu-bt-hci lab`), or given on the command line:

```shell
pbl configure --board qemu_emery -DCONFIG_BT_HCI_UART=y
pbl itest --qemu-bt-hci /dev/cu.usbmodem1101 --ble-controller /dev/cu.usbmodem1201
```

Tests that need a phone take the `phones` fixture: `phones()` makes the
setup's phone, and `connect()` pairs and opens the Pebble protocol session
(`phone.pebble`). A Bumble phone can also be another phone to the watch,
`phones(address=...)`, with a bond of its own, and host the PPoGATT
service itself, `phones(ppogatt="forward")`, instead of using the one the
watch hosts; tests that ask for these skip on other phones.
`harness.helpers.firmware` installs a firmware bundle through a phone, as
the phone app does:

```python
def test_version(phones):
    phone = phones().connect()
    assert phone.watch_version().version_tag
```

### Results

Everything a run produces goes to `BUILD/itest` (or `--results-dir`):

- `junit.xml`: the report
- `device.log`: the dehashed log of the whole session, with a header at
  the start of each test
- `<test>/device.log`: each test's slice of it
- `<test>/*.actual.png`, `*.diff.png`: screenshots and, on a mismatch, the
  differences
- `<test>/failure.png`: the screen when a test failed; the tail of the log
  is added to the failure report too
- `<test>/<name>.json`, `<name>.csv`: current measurements
- `qemu.log`, `uart1.log`, `flash.log`: the emulator's and the flasher's
  output

## Selecting tests by device

Tests declare where they apply with markers, and those that do not apply to
the build under test are deselected:

- `boards("obelix", "qemu_emery")`: the board, without revision
- `platforms("emery")`: the platform, covering its emulated board too
- `device_types("hardware")`: `qemu` or `hardware`
- `requires_config("CONFIG_TOUCH")`: Kconfig symbols that must be set
- `variants("prf")`: the firmware variant, `normal` or `prf`; unmarked
  tests are for `normal`

The names are also keywords, so `-k obelix` selects tests declared for
obelix. `--board` selects for another board than the build's, e.g. to see
what would run on one without a build for it:

```shell
pbl itest --collect-only -q --board obelix --device-type hardware
```

What a test covers is a category marker: `smoke`, `ui`, `notifications`,
`power` and `slow`. The full list is in `harness/plugin.py`; markers are
strict, so a new one must be added there.

## Writing a test

Tests are grouped by area in subdirectories of `tests/integration`
(`system`, `ui`, `notifications`, `power`). A test asks for the fixtures it
needs:

```python
import pytest

from harness.helpers.ui import Button

pytestmark = pytest.mark.ui


def test_settings(ui, snapshot):
    ui.press(Button.SELECT)
    ui.wait_idle()
    ui.press(Button.SELECT)
    image = ui.wait_idle()
    assert ui.top_window() == "Settings"
    snapshot.assert_match(image, "settings")
```

The fixtures:

- `dut`: the launched device. `dut.prompt(cmd)` runs a prompt command,
  `dut.protocol` is a libpebble2 connection, `dut.wait_for_log(regex,
  since=dut.logs.mark())` waits for a log line, and `dut.reset()` restarts
  the firmware.
- `ui`: input and screen helpers, starting from the watchface: `press`,
  `long_press`, `hold`, `swipe`, `tap` (QEMU only), `screenshot`,
  `wait_idle` (until the screen stops changing), `window_stack`,
  `modal_stack`, `top_window`, `launch_app`, `set_time` and `go_home`.
- `prompt`: `dut.prompt`, skipping the test when there is no prompt.
- `snapshot`: screenshot comparison, below.
- `power`: current measurement, below.
- `build`: the build under test (`board`, `platform`, `config`).

`harness.helpers.notifications` inserts notifications as the phone app
would.

The launched device is shared by the whole session by default; for a fresh
boot per test (or module) pass `--dut-scope function` (or `module`). The
emulator's clock starts at a fixed time (`--qemu-rtc`) so that screens are
reproducible.

## Screenshot comparison

`snapshot.assert_match(image, name)` compares a screenshot with
`golden/<board>/<test module>/<name>.png`. Screenshots are the framebuffer
as the firmware renders it, taken over the Pebble protocol when a
connection carries it and from the emulator's display otherwise.

To create or update golden images, run the tests with `--update-golden` and
review the new images before committing them:

```shell
pbl itest --update-golden -k settings
```

Golden images are per board, and a real watch renders what its own
settings say: e.g. the status bar clock follows its timezone and 12/24h
preference. Parts of the screen that legitimately change can be left out
with `mask`,
a list of `harness.helpers.snapshot.Region(x, y, w, h)`; `tolerance` allows
small per-channel differences and `max_diff_pixels` a number of differing
pixels.

(current-measurement)=

## Current measurement

Power tests use a Nordic Power Profiler Kit II as a source meter: it
replaces the battery and supplies VBAT, so the harness can also power the
watch on and off. Connect the PPK2's VOUT and GND to the battery terminals,
and pass its port and the voltage to supply. A PPK2 shows up as two serial
ports of which only one answers; `auto` finds it:

```shell
pbl -b build-obelix itest --device-serial /dev/tty.usbserial-1 \
    --ppk2 auto --ppk2-voltage 3800 -m power
```

With a PPK2 the harness powers the watch on before the session, and
`dut.reset()` power-cycles it. `power.measure_idle(name)` measures the watch
as if it were unplugged: the console stops listening
(`console disable rx`), which lets the firmware sleep as a release build
would, and the harness drops its connections for the duration. The watch
is left to settle, then measured for 60 s, then the harness reconnects:

```python
def test_idle(ui, power):
    ui.go_home()
    m = power.measure_idle("idle")
    assert m.mean_ua < LIMIT_UA, m
```

`power.measure(name)` measures the duration of a `with` block instead,
connections and all. A measurement has `mean_ua`, `min_ua`, `max_ua`,
`percentile_ua(q)`, `charge_uah` and `energy_uwh`, and is saved in the
test's results as `<name>.json` (the summary) and `<name>.csv` (1 ms
averages); record them with `record_property` to have them in the JUnit
report. A measurement that lost samples fails rather than report a wrong
figure.

The power tests measure TicToc idling while advertising fast and slow for
discovery, and in airplane mode. They wipe the watch first (`dut.wipe()`),
so run them on a test watch, with no phone bonded to it nearby. Each passes
within 10% of the board's nominal figure in `power/test_idle.py`, measured
at 3.8 V; update the nominals when a change moves them on purpose. Boards
without nominals record their figures with a warning.

## Recovery firmware

The tests in `prf/` cover what a PRF release is checked for: the Getting
Started screen and the phone's name on it, pairing and the Pebble protocol
over reversed PPoGATT, a second phone taking over the single bond,
installing the normal firmware from the phone and "Reset to PRF" from it,
the backlight timeout, turning off after 10 minutes unplugged and
unconnected (not while a phone or a charger is connected), the low battery
screen, and the current advertising, connected and off. On the emulator
the screens are compared with golden images, the backlight is timed on the
display, and `dut.set_battery()` sets the emulated battery's charge and
charger; on a watch the current is measured with a PPK2, the backlight
timed on it, and the low battery screen shown by powering it at 3.5 V.

Installing the firmware takes a normal build of the same board, bundled:

```shell
pbl -b build-main configure --board qemu_emery
pbl -b build-main build bundle
pbl -b build-prf configure --board qemu_emery --variant prf -DCONFIG_BT_HCI_UART=y
pbl -b build-prf build qemu_image_micro qemu_image_spi
pbl -b build-prf itest --no-build --qemu-bt-hci virtual --main-build "$PWD/build-main"
```

The emulator has no bootloader, so it only checks the transfer. A watch
installs it, boots it, and goes back to PRF when the phone asks; it has to
be running PRF to start with, as set up for a release check (the
bootloader and PRF alone on the flash). `slow` covers the idle shutdown,
about 12 minutes each way.

Left for a person: the Back+Up+Select hold that reboots into PRF,
charging a watch, the MFG menu's tests (checked in the factory), and the
phone apps themselves.

## Extending the harness

- A new connection backend is a `Connection` subclass in a module of
  `harness/connections`, with its `scheme` and `capabilities`; it is picked
  up by `--connection SCHEME:ADDRESS`.
- A new kind of device is a `DeviceAdapter` subclass in `harness/device`,
  registered in `harness/device/factory.py`.
- Helpers built on the device go in `harness/helpers`, with a fixture in
  `harness/fixtures.py`.
