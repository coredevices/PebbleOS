# Zephyr BR/EDR host simulation

Runs the Zephyr host as the firmware builds it (Zephyr, `port/` and `ctl/`
from `third_party/zephyr_bt`, the HCI router from `subsys/bluetooth`) on the
POSIX PebbleOS kernel of the unit tests, against a simulated controller and
phone:

    tests/zephyr_bt/host_sim/run.sh

It starts and stops the host six times and checks, each time, that it comes
up, connects to the phone and gets an encrypted link, becomes discoverable
and stops cleanly. The first connection pairs (numeric comparison, the watch
confirms the passkey) and the link key is saved; the next two reconnect with
the saved key, without pairing; after `zbt_clear_keys()`, the fourth pairs
again. For the last two, the phone has an LE Secure Connections pairing in
NimBLE's store (a stand-in): the watch derives the link key from its LTK and
the phone takes it, without pairing. Between runs the host's RAM is
overwritten, so each start has to set it up afresh.

The phone is also a hands-free gateway (`fake_ag.c`: L2CAP signalling, an
SDP record, RFCOMM with credits, the AT commands). Each round checks that
the service level connection comes up after encryption with the phone's
indicators; the last two also ring with a number, answer from the watch
(the phone then opens the voice link, which the watch accepts) and end the
call from the watch.

It is not part of `pbl test`: the host needs its own compiler options, a
32-bit build and linker script sections the unit test harness does not
provide. It needs clang with 32-bit (`-m32`) support. The full log is in
`build-host-sim/host_sim.log`.

The phone book client is built in but not exercised: the phone has no phone
book server, so the reading it starts after the service level connection
finds none.

What it does not cover: the watch's MPU and real stack use, timing, and how
a real controller and phone answer.
