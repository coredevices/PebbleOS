# Bluetooth: ANCS client

This sample is a peripheral that shows the notifications of an iOS device
with the ANCS client (`pbl/bluetooth/clients/ancs.h`), on NimBLE alone.

It advertises a solicitation for the Apple Notification Center Service, so
that iOS lists it among the accessories to pair with. Once a device connects,
the sample asks it to secure the link, since the service is only there for a
paired accessory, and then starts the client: it looks for the service and,
when the device has it, subscribes to the Data Source and the Notification
Source, in that order so that no response is missed.

Each notification the device adds, modifies or removes is printed as the
client reports it. For the first two the sample then asks the client for the
title and the message, and prints them once the client has the whole response.
The client runs one request at a time, so the attributes of a notification
that comes while another is being asked about are not got.

The sample handles the GAP events of its connection itself, and so hands the
notifications it receives to the listeners with `pbl_bt_gattc_notify_rx()`,
which is how the client gets them. The client calls back on the NimBLE host
thread.

The bond is kept by NimBLE's config store in a settings file: the board
overlays give each board a filesystem partition.

```{pebbleos-sample}
:path: samples/subsys/bluetooth/ancs_client
:sources: src/main.c prj.conf
:output:
```
