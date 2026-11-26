# Pebble protocol: PPoGATT

This sample is a peripheral that opens a Pebble protocol session with a phone
app over PPoGATT (`pbl/pp/transports/gatt.h`) and answers its pings, on the
protocol subsystem and NimBLE alone.

Once a phone connects, the sample asks it to secure the link, and then starts
the transport: it looks for the PPoGATT services of the phone, reads what each
says about its app, subscribes to its data and opens a session with it. The
sample prints the sessions as they open and close, and what the phone app
tells about itself.

## Main thread

The subsystem asks the application for the thread it runs on, with
`pbl_pp_main_call()` and `pbl_pp_main_is_current()` (`pbl/pp/main.h`). The
sample gives it a work queue of its own. It cannot be the system work queue:
the handlers of the endpoints run there, and may wait for the transport to
make room while they send.

What NimBLE reports on its host thread, the link being secured or gone, is
passed on to that thread, where the transport is started and stopped.

## Endpoint

The sample defines the ping endpoint: its handler prints the cookie of each
ping, and sends it back in a pong. The subsystem brings the endpoints every
session needs, to ask the phone app for its version and to refuse the
messages nothing handles.

## Transport callbacks

PPoGATT asks the application for the serial number it sends to the phone, and
to apply the connection response time a session asks for. The sample has a
made-up serial number, and leaves the connection as the phone set it.

## Bluetooth

The sample advertises with `pbl/bluetooth/adv.h`: one advertisement, on air
whenever there is no connection, at 100 ms for a minute and at 1 s from then
on.

The bond is kept by NimBLE's config store in RAM, its default without
settings: it is lost when the watch resets, and the phone, which keeps its
own, then has to forget the watch and pair again.

```{pebbleos-sample}
:path: samples/subsys/pp/gatt
:sources: src/main.c src/pp.c prj.conf
:output:
```
