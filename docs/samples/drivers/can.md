# CAN: Echoed pings

This sample joins the board's CAN bus at 500 kbit/s and sends ten pings, 0x100
frames carrying a counter, each waiting for another node to acknowledge it.
Received frames are queued by the receive callback, which runs in the
controller's interrupt, and an acceptance filter only lets in 0x101 frames: the
main thread waits for each ping to come back as one, and prints how long it
took. At the end it prints the error counters and stops the controller.

The board needs a transceiver on its CAN pins and another node on the bus,
terminated at both ends. `echo.py` makes a host with a CAN adapter that node:
it sends each ping back as 0x101. It needs
[python-can](https://python-can.readthedocs.io), with the interface and channel
of the adapter:

```shell
pip install python-can
samples/drivers/can/echo.py --interface socketcan --channel can0
```

Without another node, the first ping is never acknowledged, and the sample says
so.

```{warning}
Some boards may suffer from this problem: while the board resets or powers up,
its CAN TX pin is low or floating, and the transceiver drives the bus dominant.
The other node then stays in error warning, and some host drivers drop the
frames it receives, so `echo.py` sees no pings. A pull-up on TX fixes it.
```

```{pebbleos-sample}
:path: samples/drivers/can
:sources: src/main.c echo.py
:output:
```
