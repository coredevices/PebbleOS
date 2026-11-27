# CANopen: Drive

This sample runs a CiA 402 drive, node 6 at 500 kbit/s, on the board's CAN
bus, with a simulated motor: its velocity follows the target with a 200 ms time
constant while the drive is operation enabled, coasts down once disabled and
ramps down faster on a quick stop, which it then reports done. The model is
the control loop: it runs on its own every millisecond, in the tick
interrupt, and shares only atomics with the node, so it never waits for the
stack. In profile velocity, the write callback hands it a new target as soon
as it is written, and the main thread reports its velocity every 10 ms. In
cyclic synchronous velocity, the SYNC callback hands it the target RPDO 2
applies and reports its velocity, which TPDO 2 then sends with the
statusword. TPDO 1 sends the statusword and the mode on every change.

The board needs a transceiver on its CAN pins and a CANopen master on the
bus, terminated at both ends. `master.py` makes a host with a CAN adapter that
master: it resets and starts the node, enables the drive in cyclic synchronous
velocity with SYNC at 100 Hz, checks that the velocity follows two targets,
quick stops it, then enables it in profile velocity and waits for the target
to be reached. It needs [python-can](https://python-can.readthedocs.io) and
[canopen](https://canopen.readthedocs.io), with the interface and channel of
the adapter and the EDS file the build generates:

```shell
pip install python-can canopen
samples/subsys/canopen/drive/master.py --interface socketcan --channel can0 \
    --eds build/canopen/od.eds
```

```{pebbleos-sample}
:path: samples/subsys/canopen/drive
:sources: od.yaml src/main.c master.py
:output:
```
