# CANopen

`subsys/canopen/` runs a CANopen device, as CiA 301 specifies it, on a
{doc}`CAN controller <../drivers/can>`. The object dictionary is generated from a YAML file, along
with the EDS file the tools on the other end of the bus read.

| | |
| --- | --- |
| Directory | `subsys/canopen` |
| Header | `pbl/canopen/canopen.h` |
| Kconfig | `CONFIG_CANOPEN`, enabled by the application; needs `CONFIG_CAN` |
| API | {apidoc}`API reference <subsys_canopen>` |

## Overview

A node is one device on the bus, with a node-ID from 1 to 127. The controller's
receive interrupt only queues frames; the services run on one thread, read and
write the object dictionary, and call the application back. The application does its work
in the callbacks, a thread of its own or an interrupt, as its timing needs.

```{mermaid}
flowchart TB
  master["NMT master<br/>SDO client, SYNC producer"]
  can["CAN controller<br/>pbl_can"]
  services["CANopen thread<br/>NMT · heartbeat · SDO · PDO · SYNC · EMCY"]
  od[("Object dictionary<br/>generated from YAML")]
  app["Application<br/>callbacks"]
  loop["Application work<br/>thread or interrupt"]
  master <-->|"CAN bus"| can
  can <-->|"RX ISR queues, TX queue drains"| services
  services <--> od
  services -->|"callbacks"| app
  app <-.->|"pbl_canopen_od_*()"| od
  app <-.->|"as it needs"| loop

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class app,loop app
  class services,od system
  class master,can base
```

It runs these services:

| Service | Objects | Notes |
| --- | --- | --- |
| NMT slave | | Boot-up message, start, stop, pre-operational, reset node and reset communication |
| Heartbeat producer | 0x1017 | |
| Heartbeat consumer | 0x1016 | Monitoring starts with the first heartbeat of each node |
| SDO server | 0x1200 | Expedited and segmented transfers |
| RPDOs | 0x1400, 0x1600 | Event-driven and synchronous, deadline monitoring |
| TPDOs | 0x1800, 0x1a00 | Event-driven with inhibit time and event timer, synchronous cyclic and acyclic, SYNC start value |
| SYNC consumer | 0x1005 | With or without a counter |
| EMCY producer | 0x1001, 0x1003, 0x1014, 0x1015 | Error register, error history, inhibit time |

The node does not run SDO block transfers, an SDO client, an NMT master, a
SYNC producer, LSS, node guarding, RTR-triggered PDOs, multiplexed PDOs or
parameter storage (0x1010 and 0x1011).

## Quick start

A device with a counter it sends and an output the master sets. The YAML file
describes the device, `pbl_canopen_od()` generates the dictionary from it, and
the application starts a node on it:

:::::{tab-set}

::::{tab-item} od.yaml

```yaml
device:
  name: Example device
  product_code: 1

heartbeat:
  producer: 1000            # ms

tpdos:
  - transmission: 254       # on change
    inhibit: 100            # 10 ms
    mapping: [counter]

rpdos:
  - mapping: [output]

objects:
  - index: 0x2000
    name: counter
    type: u32
    access: ro
    pdo: true
  - index: 0x2001
    name: output
    type: u8
    pdo: true
```

::::

::::{tab-item} CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
project(example)

target_sources(app PRIVATE src/main.c)
pbl_canopen_od(od.yaml)
```

::::

::::{tab-item} prj.conf

```
CONFIG_CAN=y
CONFIG_CANOPEN=y
```

::::

::::{tab-item} src/main.c

```c
#include "od.h"

#include <errno.h>

#include <pbl/canopen/canopen.h>
#include <pbl/drivers/can.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

static struct pbl_canopen s_co;

/* Every write from the bus goes through here first. */
static int prv_write(struct pbl_canopen *co, uint16_t index, uint8_t subindex, const void *data,
                     size_t len, void *arg) {
  return index == OD_OUTPUT && *(const uint8_t *)data > 100U ? -ERANGE : 0;
}

static const struct pbl_canopen_callbacks s_cb = {.write = prv_write};

int main(void) {
  const struct pbl_canopen_config cfg = {
    .can = CAN_BUS,
    .od = &od,
    .node_id = 5,
    .bitrate = 500000,
    .cb = &s_cb,
  };
  uint32_t counter = 0U;
  int ret;

  ret = pbl_canopen_init(&s_co, &cfg);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_canopen_start(&s_co);
  if (ret != 0) {
    return ret;
  }

  for (;;) {
    pbl_thread_sleep(PBL_MSEC(500));
    counter++;
    /* A new value sends the TPDO that maps it. */
    (void)pbl_canopen_od_write(&s_co, OD_COUNTER, 0U, &counter, sizeof(counter));
  }
}
```

::::

:::::

`pbl_canopen_od()` names the dictionary after the YAML file, `od` for
`od.yaml`, unless `NAME` gives another symbol, and adds it to the `app`
target unless `TARGET` names another one. The generated header defines a
macro for the index of every application object, and for the sub-index of
every record member: `OD_COUNTER`, `OD_CONFIG_GAIN`.

The node configures and starts the controller, restores every default, sends
its boot-up message and goes pre-operational, where the master configures it
over SDO before starting it. `pbl_canopen_od_read()` returns the size it read,
and `pbl_canopen_od_write()` writes any entry, whatever its access over the
bus; a value that changes triggers the event-driven and acyclic TPDOs that map
it. `pbl_canopen_emcy_raise()` and `pbl_canopen_emcy_clear()` report the
application's errors: each one raised sets its error register bits, goes into
the error history and sends an emergency message, and each one cleared sends
an error reset message with the bits of the errors left.

From a host with a CAN adapter, the [canopen](https://canopen.readthedocs.io)
Python package drives the node with the EDS file the build writes,
`build/canopen/od.eds`:

```python
import canopen

network = canopen.Network()
network.connect(interface="socketcan", channel="can0", bitrate=500000)
node = network.add_node(5, "build/canopen/od.eds")

node.nmt.state = "OPERATIONAL"
node.sdo["output"].raw = 42
print(node.sdo["counter"].raw)
```

## NMT states

The master moves the node between states with NMT commands, and the node
reports its state in its heartbeat. Each state allows some services only:

```{mermaid}
flowchart TB
  any(["Any state"])
  init["Initializing"]
  preop["Pre-operational<br/>SDO, SYNC, EMCY"]
  op["Operational<br/>SDO, SYNC, EMCY, PDOs"]
  stopped["Stopped<br/>NMT and heartbeat only"]
  any -.->|"reset node,<br/>reset communication"| init
  init -->|"boot-up message"| preop
  preop -->|"start"| op
  op -->|"enter pre-operational"| preop
  preop -->|"stop"| stopped
  stopped -->|"enter pre-operational"| preop
  op -->|"stop"| stopped
  stopped -->|"start"| op

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class op app
  class preop system
  class init,stopped,any base
```

Reset node and reset communication bring the node back to initializing from
any state. In detail:

| Service | Pre-operational | Operational | Stopped |
| --- | :---: | :---: | :---: |
| NMT, heartbeat | ✓ | ✓ | ✓ |
| SDO | ✓ | ✓ | |
| EMCY | ✓ | ✓ | |
| SYNC | | ✓ | |
| PDOs | | ✓ | |

A reset from the master restores the defaults of the communication objects,
and with reset node those of the application objects too, then calls the
reset callback before the boot-up message, to set the values that differ from
them, e.g. a serial number in 0x1018. The NMT callback reports every change of
state.

## Threading

Nodes run on one thread, `CANopen`, with `CONFIG_CANOPEN_THREAD_PRIO` and
`CONFIG_CANOPEN_THREAD_STACK_SIZE`. The controller's receive interrupt only
queues frames, up to `CONFIG_CANOPEN_RX_QUEUE_SIZE` per node, and every timer
is delayed work on the same thread, so the services never run concurrently.
Frames go through a queue of `CONFIG_CANOPEN_TX_QUEUE_SIZE` that the
controller drains as it sends.

The callbacks run on that thread, with the node locked; they may call the API,
whose lock is recursive. They hold up every service of every node while they
run. The write callback is called for every application entry an SDO or an
RPDO writes, before the value is stored, and refuses it with a negative errno:
an SDO is then aborted, `-EINVAL` and `-ERANGE` as an invalid value, `-EBUSY`
as not possible in the present state.

On a SYNC, the synchronous RPDOs are applied, the SYNC callback runs, and the
synchronous TPDOs go once it returns: the callback is where the application
takes its new values and reports its own.

```{mermaid}
sequenceDiagram
  participant M as Master
  participant I as RX interrupt
  participant T as CANopen thread
  participant A as Application
  M->>I: RPDO (synchronous)
  I->>T: queue
  Note over T: buffered until SYNC
  M->>I: SYNC
  I->>T: queue, with pbl_cycle_get_32()
  T->>T: apply synchronous RPDOs
  T->>A: sync callback
  A->>T: pbl_canopen_od_read(), pbl_canopen_od_write()
  T->>M: synchronous TPDOs
```

The SYNC callback gets the value of `pbl_cycle_get_32()` when the controller
handed the SYNC over, so it can tell how late it runs.

(canopen-work)=

## Where the work runs

What the application does with the values it gets depends on how long it takes
and how much timing matters. A few strategies:

| Strategy | Fits | Data exchange | Timing |
| --- | --- | --- | --- |
| In the callbacks | Short actions: an LED, a relay, a slow process value | The API, directly | After the services ahead of it on the node's thread |
| An application thread | Longer work, or work that blocks: a bus transfer, flash, a slow control loop | A semaphore or a queue to wake it, the API, which it may call | Its own priority |
| An interrupt | Fast loops with strict timing, paced by a timer or a peripheral | Without locks | Independent of the bus traffic |

Some notes:

- Work in a callback delays the heartbeat, SDO replies and the other nodes'
  services for as long as it runs; a short one costs nothing else.
- A thread may call the API, which locks the node, so it waits for a callback
  or a service in progress; a priority above the node's thread does not change
  that.
- An interrupt never calls the API, and a zero-latency one, with
  `CONFIG_KERNEL_ZERO_LATENCY_IRQS`, no kernel function at all. A callback or
  a thread hands it the values, and takes its results, without a lock it
  could wait on: atomics for single values; for a set that must change at
  once, a copy with that interrupt masked, which delays at most one of its
  runs, or a double buffer whose index the writer swaps atomically. Masking
  every interrupt, zero-latency ones included, works too, but delays all of
  them.

:::::{tab-set}

::::{tab-item} Callback

```c
static void prv_sync(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  uint8_t on;

  if (pbl_canopen_od_read(co, OD_LED, 0U, &on, sizeof(on)) == (int)sizeof(on)) {
    led_set(on != 0U);
  }
}
```

::::

::::{tab-item} Thread

```c
static PBL_SEM_DEFINE(s_sync, 0, 1);

static void prv_sync(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  pbl_sem_give(&s_sync);
}

static void prv_worker(void *arg) {
  int32_t target;
  int32_t actual;

  for (;;) {
    (void)pbl_sem_take(&s_sync, PBL_FOREVER);
    (void)pbl_canopen_od_read(&s_co, OD_TARGET, 0U, &target, sizeof(target));
    actual = process_step(target);
    (void)pbl_canopen_od_write(&s_co, OD_ACTUAL, 0U, &actual, sizeof(actual));
  }
}
```

::::

::::{tab-item} Interrupt, atomics

```c
static atomic_int_least32_t s_target;
static atomic_int_least32_t s_actual;

/* Loop interrupt */
static void prv_loop(void) {
  int32_t target = atomic_load(&s_target);
  ...
  atomic_store(&s_actual, actual);
}

static void prv_sync(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  int32_t target;
  int32_t actual = atomic_load(&s_actual);

  if (pbl_canopen_od_read(co, OD_TARGET, 0U, &target, sizeof(target)) == (int)sizeof(target)) {
    atomic_store(&s_target, target);
  }
  (void)pbl_canopen_od_write(co, OD_ACTUAL, 0U, &actual, sizeof(actual));
}
```

::::

::::{tab-item} Interrupt, masked copy

```c
struct setpoint {
  int32_t target;
  int32_t limit;
  int16_t gain;
};

static struct setpoint s_setpoint;

static void prv_sync(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  struct setpoint sp;

  (void)pbl_canopen_od_read(co, OD_TARGET, 0U, &sp.target, sizeof(sp.target));
  (void)pbl_canopen_od_read(co, OD_LIMIT, 0U, &sp.limit, sizeof(sp.limit));
  (void)pbl_canopen_od_read(co, OD_GAIN, 0U, &sp.gain, sizeof(sp.gain));

  /* The loop sees all of it from its next run on. */
  pbl_irq_disable(LOOP_IRQ);
  s_setpoint = sp;
  pbl_irq_enable(LOOP_IRQ);
}
```

::::

:::::

## Object dictionary

The YAML file describes the device and its application objects. Every key is
optional but `objects`' `index`, `name` and `type`:

```yaml
device:
  type: 0x00000000          # 0x1000
  name: Example device      # 0x1008
  hardware_version: "1.0"   # 0x1009
  software_version: "1.0"   # 0x100a
  vendor_id: 0x00000000     # 0x1018
  product_code: 0x00000001
  revision: 0x00000001
  serial: 0x00000000

errors: 8                   # depth of the error history 0x1003, 0 for none

emcy:
  cob_id: 0x85              # 0x1014, 0x80 + node-ID by default
  inhibit: 0                # 0x1015, in 100 µs

heartbeat:
  producer: 1000            # 0x1017, in ms
  consumers:                # 0x1016: a count of empty entries, or a list
    - {node: 1, time: 1500}

sync:
  cob_id: 0x80              # 0x1005

tpdos:
  - transmission: 254       # 0-240 synchronous, 254 or 255 event-driven
    inhibit: 100            # in 100 µs
    event_timer: 1000       # in ms
    sync_start: 0
    mapping: [counter, config.gain]

rpdos:
  - cob_id: 0x205           # 0x200 + 0x100 per PDO + node-ID for the first four
    enabled: true
    transmission: 255
    event_timer: 0          # deadline, in ms
    mapping: [output]

objects:
  - index: 0x2000
    name: counter
    type: u32
    access: ro
    pdo: true
  - index: 0x2001
    name: label
    type: string
    size: 32
    default: Example
  - index: 0x2002
    name: samples
    type: array
    element: i16
    length: 4
    access: ro
    pdo: tx
  - index: 0x2003
    name: config
    type: record
    members:
      - {name: gain, type: u16, default: 10, pdo: true}
      - {name: offset, type: i32}
```

| Key | Values |
| --- | --- |
| `type` | `bool`, `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `string`, `octets`, `array`, `record` |
| `access` | `ro`, `wo`, `rw` (the default) or `const` |
| `pdo` | `true` for the PDOs its access allows, `tx` or `rx` for one kind only |
| `size` | Strings and octets, the default's length otherwise; they cannot be mapped |

Arrays name their entries `name[1]` to `name[length]` and records
`name.member`, in mappings. Application objects go at 0x2000 and above; the
generator adds the communication objects from the sections above, with mapping
objects of 8 entries for dynamic mapping. It also writes an EDS file, `od.eds`
next to the generated sources.

## PDOs

A PDO carries up to 8 bytes of mapped entries in one frame, without the
protocol overhead of an SDO. Its transmission type says when it goes:

| Type | TPDO | RPDO |
| --- | --- | --- |
| 0 | On the SYNC after a mapped value changed | Applied on the next SYNC |
| 1-240 | Every n-th SYNC, from the SYNC start value | Applied on the next SYNC |
| 254, 255 | When a mapped value changes, at most once per inhibit time, and with the event timer | Applied at once, deadline from the event timer |

PDO mappings can be changed over SDO with the procedure CiA 301 gives:
invalidate the PDO, clear sub-index 0 of the mapping, write the entries, set
sub-index 0, validate the PDO. Mapped entries are whole bytes, up to 8 per PDO
and 8 bytes in all; RPDOs can also map the dummy entries 0x0001-0x0007.

## Emergencies

The node raises emergencies of its own, in the communication bit of the error
register, and clears each one once its cause is gone:

| Code | Cause |
| --- | --- |
| 0x8110 | Received frames dropped |
| 0x8120 | Controller error passive |
| 0x8130 | Heartbeat lost |
| 0x8140 | Bus off |
| 0x8210 | RPDO shorter than its mapping |
| 0x8220 | RPDO longer than its mapping |
| 0x8250 | RPDO deadline missed |

## Configuration

| Option | Default | |
| --- | :---: | --- |
| `CONFIG_CANOPEN_THREAD_PRIO` | 4 | Priority of the nodes' thread, the highest |
| `CONFIG_CANOPEN_THREAD_STACK_SIZE` | 1536 | |
| `CONFIG_CANOPEN_RX_QUEUE_SIZE` | 16 | Received frames queued per node |
| `CONFIG_CANOPEN_TX_QUEUE_SIZE` | 16 | Frames queued for transmission per node |
| `CONFIG_CANOPEN_SDO_TIMEOUT_MS` | 1000 | Segmented transfer timeout |
| `CONFIG_CANOPEN_EMCY_ACTIVE` | 8 | Errors raised at once per node |
| `CONFIG_CANOPEN_EMCY_QUEUE` | 4 | Emergency messages held back by the inhibit time |
