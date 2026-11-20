# Bluetooth: iOS clients

This sample is a peripheral that shows the notifications of an iOS device
with the ANCS client (`pbl/bluetooth/clients/ancs.h`) and what it plays with
the AMS client (`pbl/bluetooth/clients/ams.h`), on NimBLE alone.

It advertises a solicitation for the Apple Notification Center Service, so
that iOS lists it among the accessories to pair with. Once a device connects,
the sample asks it to secure the link, since the services are only there for
a paired accessory, and then starts the clients: each looks for its service
and subscribes to it when the device has it.

Each client is in a file of its own, and the sample has an option for each:
`CONFIG_SAMPLE_ANCS` and `CONFIG_SAMPLE_AMS`, both on by default. With only
the second, the sample solicits the Apple Media Service instead.

## Notifications

The ANCS client subscribes to the Data Source and the Notification Source, in
that order so that no response is missed. Each notification the device adds,
modifies or removes is printed as the client reports it. For the first two
the sample then asks the client for the title and the message, and prints
them once the client has the whole response. The client runs one request at a
time, so the attributes of a notification that comes while another is being
asked about are not got.

## Media

The AMS client subscribes to the Remote Command, which tells the commands the
media player takes, and to the Entity Update, with which it then registers
for the playback info and the volume of the player, and for the artist, the
album, the title and the duration of the track. Their values are printed as
the device tells them: once registered, and whenever they change. A value
that did not fit comes cut, and the sample then reads the whole of it.

## Bluetooth

The sample handles the GAP events of its connection itself, and so hands the
notifications it receives to the listeners with `pbl_bt_gattc_notify_rx()`,
which is how the clients get them. The clients call back on the NimBLE host
thread.

The bond is kept by NimBLE's config store in a settings file: the board
overlays give each board a filesystem partition.

```{pebbleos-sample}
:path: samples/subsys/bluetooth/ios_clients
:sources: src/main.c src/ancs.c src/ams.c Kconfig prj.conf
:output:
```
