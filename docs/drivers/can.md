# CAN

A CAN controller sends and receives CAN and CAN FD frames. It is configured
and given its acceptance filters while stopped, then joins the bus; received
frames are handed to a callback from its interrupt, and frames are sent
waiting for their transmission or with a callback for its outcome. The class
computes the bit timing from the bit rates and sample points.

A controller that goes bus off leaves the bus, completing the frame in flight
with `-ENETDOWN`, and rejoins it after the recovery sequence ISO 11898-1
specifies: 128 occurrences of 11 recessive bits. With
`PBL_CAN_MODE_MANUAL_RECOVERY`, it stays bus off until `pbl_can_recover()`
starts that sequence.

With `PBL_CAN_MODE_ONE_SHOT`, a frame is sent once: if it meets an error or
loses arbitration, it completes with `-EIO` instead of being sent again.

The controller counts the errors it sees, by kind (bit, stuff, form,
acknowledgment, CRC), the arbitrations it lost, the frames it dropped for lack
of room and the times it went bus off; `pbl_can_get_stats()` returns them.

| | |
| --- | --- |
| Directory | `drivers/can` |
| Header | `pbl/drivers/can.h` |
| Kconfig | `CONFIG_CAN`, enabled by the application |
| API | {apidoc}`API reference <drivers_can>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Fraunhofer IPMS CAN-CTRL | `sifli,sf32lb57-can` | `CONFIG_CAN_IPMS_CANFD` |

The IPMS CAN-CTRL controllers of the SF32LB57 support neither loopback nor CAN
FD.

## Shell

With `CONFIG_CAN_SHELL`, the `can` command configures and starts a
controller, sends frames, in the `cansend` syntax, and shows the frames it
receives:

```text
can config can1 500000
can start can1
can send can1 123#DEADBEEF
can send can1 12345678##1000102030405060708090a0b
can dump can1 10
can stats can1
```

A data bit rate after the nominal one enables CAN FD, as in
`can config can1 500000 2000000`. Modes follow the bit rates, as in
`can config can1 500000 one-shot manual-recovery`; `can recover can1` then
starts the recovery from bus off.
