# CANopen: Device

This sample runs a CANopen device, node 5 at 500 kbit/s, on the board's CAN
bus. Its object dictionary comes from `od.yaml`: a counter the main thread
increments every 500 ms, sent in TPDO 1 on every change, at most every 10 ms
and at least every second, while the node is operational; an output written
by RPDO 1 or an SDO; and a label to read and write with segmented SDOs. The
write callback prints every value the bus writes, and the NMT callback every
state change. While operational, the SYNC callback measures how long after its reception it runs, and the sample prints the
worst case every 5 seconds.

The board needs a transceiver on its CAN pins and a CANopen master on the
bus, terminated at both ends. `master.py` makes a host with a CAN adapter
that master: it resets the node, reads its identity and label, writes a new
label, starts it, checks that the counter increases and writes the output.
`--sync` then sends SYNC at a given rate, for `--duration` seconds, for
the sample to report its latency. It needs [python-can](https://python-can.readthedocs.io) and
[canopen](https://canopen.readthedocs.io), with the interface and channel of
the adapter and the EDS file the build generates:

```shell
pip install python-can canopen
samples/subsys/canopen/device/master.py --interface socketcan --channel can0 \
    --eds build/canopen/od.eds
```

```{pebbleos-sample}
:path: samples/subsys/canopen/device
:sources: od.yaml src/main.c master.py
:output:
```
