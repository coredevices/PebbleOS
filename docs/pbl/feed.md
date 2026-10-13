# `pbl feed`

Write into the running emulator what the phone app would, over the Pebble
protocol serial port, so features that depend on the phone can be exercised
without one. Each kind of data is a subcommand:

```shell
pbl feed weather                         # a forecast for a few built-in cities
pbl feed weather Tokyo Sydney --seed 3   # a subset; the first is the current location
pbl feed weather --clear                 # remove every location
pbl feed music                           # a player and playlist; keeps serving until Ctrl-C
pbl feed music --title "Demo" --paused   # a single track of your own
pbl feed calendar                        # a day of events around now
pbl feed calendar --title "Dentist" --start 15 --location "Downtown"
pbl feed calendar --clear                # remove every pin
pbl feed notifications                   # a few messages from two senders
pbl feed notifications --sender Anna --body "Lunch?" --app WhatsApp
pbl feed notifications --clear           # remove every notification
```

`pbl feed <feed> --help` lists a feed's own options. A feed that writes a
database, like weather, exits once the data is stored. One that stands in
for the phone, like music, keeps running: it acts on the watch's controls,
advances playback and draws album art when asked (album art is off by
default, under Settings > Music). The emulator's port serves one host
client at a time, so run one such feed at once.

The feeds live in `tools/libs/pbl-cli/pbl/feeds/`, one module per kind of
data; {doc}`extending` shows how to add one.

## Options

```{pbl-command} feed
```
