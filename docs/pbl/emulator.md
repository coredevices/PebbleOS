# `pbl emulator`

Interact with the emulator {doc}`run` is running: QEMU on a `qemu_*` board,
or the {doc}`native simulator <../development/emulation/native_simulator>`
on a `native_*` one. Each action is a subcommand.

On QEMU, screenshots go through the socket monitor and touches through the
QMP socket, on the emulated touch controller's input path. The native
simulator has neither: both go over its Pebble protocol port, to the
firmware's screenshot and remote input endpoints. That port serves one
client at a time, so they fail there while a feed holds it.

## Screenshots

```shell
pbl emulator screenshot                       # build/screenshot.png
pbl emulator screenshot --output /tmp/foo.png
```

```{pbl-command} emulator screenshot
```

## Touch

On touch-capable boards, tap or swipe the screen. Coordinates are screen
pixels. A tap is a finger down then up; a swipe moves the finger from the
first point to the second through intermediate steps, so drag gestures are
seen as continuous.

```shell
pbl emulator touch 130 130
pbl emulator swipe 130 220 130 40                  # swipe up
pbl emulator swipe 130 220 130 40 --duration 0.4
```

```{pbl-command} emulator touch
```

```{pbl-command} emulator swipe
```

## Microphone

Play audio into the emulator's microphone, which captures silence
otherwise: a tone, white noise, a WAV file (any rate and channel count is
converted) or your computer's microphone. Audio only goes in while something
captures, dictation or an app, so start it before or while it listens.

```shell
pbl emulator mic                              # a 440 Hz tone until Ctrl-C
pbl emulator mic --tone 1000 --duration 5
pbl emulator mic --file speech.wav --loop
pbl emulator mic --live                       # needs the sounddevice package
```

The audio goes over the Pebble protocol port, which serves one host client
at a time: it cannot play while a feed or the `pebble` tool is connected.

```{pbl-command} emulator mic
```

## Phone data

Write into the running emulator what the phone app would, over the Pebble
protocol port, so features that depend on the phone can be exercised
without one. Each kind of data is a subcommand:

```shell
pbl emulator feed weather                         # a forecast for a few built-in cities
pbl emulator feed weather Tokyo Sydney --seed 3   # a subset; the first is the current location
pbl emulator feed weather --clear                 # remove every location
pbl emulator feed music                           # a player and playlist; keeps serving until Ctrl-C
pbl emulator feed music --title "Demo" --paused   # a single track of your own
pbl emulator feed calendar                        # a day of events around now
pbl emulator feed calendar --title "Dentist" --start 15 --location "Downtown"
pbl emulator feed calendar --clear                # remove every pin
pbl emulator feed notifications                   # a few messages from two senders
pbl emulator feed notifications --sender Anna --body "Lunch?" --app WhatsApp
pbl emulator feed notifications --clear           # remove every notification
```

`pbl emulator feed <feed> --help` lists a feed's own options. A feed that
writes a database, like weather, exits once the data is stored. One that
stands in for the phone, like music, keeps running: it acts on the watch's
controls, advances playback and draws album art when asked (album art is
off by default, under Settings > Music). The emulator's port serves one
host client at a time, so run one such feed at once.

The feeds live in `tools/libs/pbl-cli/pbl/feeds/`, one module per kind of
data; {doc}`extending` shows how to add one.

```{pbl-command} emulator feed
```
