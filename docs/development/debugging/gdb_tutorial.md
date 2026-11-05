# GDB tutorial

This tutorial debugs the firmware in the emulator with GDB, on the command
line. Nothing in it needs a watch, and on a watch only starting the session
differs: see {ref}`on-a-watch`. The native simulator runs under LLDB
on macOS, with the same steps in other words: see
{ref}`in-the-native-simulator`.

## Start a session

Build the firmware for the emulator and start it:

```shell
pbl configure --board qemu_emery
pbl build
pbl run
```

In another terminal, attach GDB:

```shell
pbl debug
```

GDB stops the firmware wherever it was, most of the time idling:

```
Remote debugging using :1233
__ISB () at ./third_party/cmsis_core/CMSIS/CMSIS/Core/Include/cmsis_gcc.h:266
266	  __ASM volatile ("isb 0xF":::"memory");
(gdb)
```

The emulator is frozen until `continue`. `Ctrl-C` stops it again, and
`quit` detaches GDB and leaves the firmware running.

## Look at the threads

Each kernel thread is a GDB thread, with its name and state:

```
(gdb) info threads
  Id   Target Id                                               Frame
* 1    Thread 1.1 (IDLE 0x2001E8B0: Running)                   pbl_soc_idle (max_ticks=max_ticks@entry=33)
    at ./soc/qemu/idle.c:19
  3    Thread 1.536990144 (PULSE 0x2001D1C0: Blocked)          0x00143666 in arch_irq_enable ()
    at ./kernel/arch/arm/arch.c:213
  4    Thread 1.536989952 (NewTimer 0x2001D100: Blocked)       0x00143666 in arch_irq_enable ()
    at ./kernel/arch/arm/arch.c:213
  5    Thread 1.536989376 (NimbleHost 0x2001CEC0: Blocked)     0x00143666 in arch_irq_enable ()
    at ./kernel/arch/arm/arch.c:213
  6    Thread 1.536989184 (App <Launcher> 0x2001CE00: Blocked) 0x00143666 in arch_irq_enable ()
    at ./kernel/arch/arm/arch.c:213
...
```

A blocked thread's stack shows what it waits for. Switch to one and print
its backtrace:

```
(gdb) thread 5
[Switching to thread 5 (Thread 1.536989376)]
#0  0x00143666 in arch_irq_enable () at ./kernel/arch/arm/arch.c:213
213	}
(gdb) bt
#0  0x00143666 in arch_irq_enable () at ./kernel/arch/arm/arch.c:213
#1  0x000ecad0 in pbl_irq_unlock () at ./kernel/irq.c:16
#2  0x000eceee in sched_block (wq=wq@entry=0x2000cb38 <g_eventq_dflt+20>, timeout=timeout@entry=...)
    at ./kernel/sched.c:192
#3  0x0014333a in pbl_msgq_get (q=0x2000cb24 <g_eventq_dflt>, msg=msg@entry=0x2000336c <s_host_task_stack+4972>,
    timeout=...) at ./kernel/msgq.c:81
#4  0x00014bee in npl_pebble_eventq_get (evq=evq@entry=0x2000cb24 <g_eventq_dflt>, tmo=tmo@entry=4294967295)
    at ./third_party/nimble/port/src/npl_os_pebble.c:31
#5  0x00016270 in ble_npl_eventq_get (evq=0x2000cb24 <g_eventq_dflt>, tmo=4294967295)
    at ./third_party/nimble/port/include/nimble/nimble_npl_os.h:98
#6  prv_host_task_main (unused=<optimized out>) at ./subsys/bluetooth/init.c:104
```

The Bluetooth host thread is waiting on its event queue.

## Set breakpoints

A breakpoint stops the firmware when it reaches a function, or a line of a
file:

```
(gdb) break FUNCTION
(gdb) break FILE:LINE
(gdb) continue
```

`tbreak` sets one that is deleted once hit, `condition N EXPR` makes
breakpoint `N` stop only when `EXPR` is true, `info breakpoints` lists
them and `delete` removes them all. GDB reports the stop with the function
and its arguments, and the line about to run, e.g. on
`event_put`:

```
Breakpoint 1, event_put (event=event@entry=0x2000960c <s_new_timer_stack+1292>)
    at ./fw/kernel/events.c:222
222	  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);
```

## Inspect a stop

`bt` prints how the firmware got there; `bt N` only the innermost `N`
frames:

```
(gdb) bt 4
#0  event_put (event=event@entry=0x2000960c <s_new_timer_stack+1292>) at ./fw/kernel/events.c:222
#1  0x12154dce in launcher_task_add_callback (callback=<optimized out>, data=<optimized out>) at ./fw/kernel/event_loop.c:95
#2  0x120982b0 in task_timer_manager_execute_expired_timers (manager=manager@entry=0x2002bc60 <s_task_timer_manager>) at ./fw/kernel/task_timer.c:350
#3  0x1206cb86 in new_timer_service_loop (data=<optimized out>) at ./fw/services/new_timer/new_timer.c:87
(More stack frames follow...)
```

`frame N` moves to another frame, where `info args` and `info locals`
show its arguments and variables. `print` evaluates any C expression,
following pointers and calling firmware functions:

```
(gdb) print event->type
$1 = PEBBLE_CALLBACK_EVENT
(gdb) print pbl_cur->name
$2 = "NewTimer\000\000\000\000\000\000\000"
```

`x/8x ADDRESS` dumps memory, and `display EXPR` prints an expression at
every stop. The firmware is optimized for size, so some variables are
`<optimized out>`; building with `-DCONFIG_DEBUG_OPTIMIZATIONS=y` keeps
more of them.

## Step

From a stop, `next` runs to the next line, stepping over calls, `step`
into them, `finish` to the end of the current function, and
`until LINE` to a later line. Interrupts and other threads can run during a
step, so it can stop somewhere else when a breakpoint hits there. `continue`
lets the firmware run until the next breakpoint or `Ctrl-C`.

## Use the PebbleOS commands

The `pbl` commands read the firmware's own structures. `pbl` lists them;
a few examples:

```
(gdb) pbl heapstats &s_kernel_heap
Heap start 0x20023780
Heap end 0x2004377c
Heap total size 131068
Heap allocated 14192
Heap high water mark 51900
Heap free blocks: 116876 bytes, 8 blocks
Heap alloc blocks: 14192 bytes, 64 blocks
Heap largest free block: 77720
(gdb) pbl metadata
Build Timestamp:  2026-10-29 09:54:22+01:00
Version Tag:      "v4.43.0-131-ga855823dd8"
Version Short:    "a855823"
Is Recovery:      False
HW Platform:      None
HW Board Name:    FirmwareMetadataPlatformPebbleEmeryEmu
HW Board Num:     245
```

`pbl layer-tree` prints the layers of the window on screen, and
`pbl to_png` saves the framebuffer as an image.

## Leave

`quit` detaches GDB and stops the proxy; the firmware carries on:

```
(gdb) quit
Detaching from program: build/pebbleos.elf, process 1
Ending remote debugging.
[Inferior 1 (process 1) detached]
```

(on-a-watch)=

## On a watch

The session is the same on a watch, once the firmware is flashed with
{doc}`../../pbl/run` and the build is the one on the watch. Release builds
are not meant for debugging. Attach:

```shell
pbl debug
```

A serial-based debugger also needs the programmer's port, given with
`--tty`; {doc}`index` covers what to look out for with those.

The watch stops for real while GDB has it: its display freezes and it
may drop its Bluetooth connection if it stays stopped for long. The core has
a handful of hardware breakpoints, which GDB uses for code in flash, so
only that many breakpoints can be set at a time.

(in-the-native-simulator)=

## In the native simulator

Build a native board and run it under the debugger:

```shell
pbl configure --board native_emery
pbl build
pbl debug
```

The program waits for `run` (`r`) to start. The steps above apply, with
LLDB's commands on macOS:

| GDB | LLDB |
| --- | --- |
| `break FUNCTION` | `b FUNCTION` |
| `continue` | `c` |
| `next` | `n` |
| `bt 4` | `bt 4` |
| `info threads` | `thread list` |
| `thread 5` | `thread select 5` |
| `print EXPR` | `p EXPR` |
| `info locals` | `frame variable` |

Each kernel thread is a host thread, named after it:

```
(lldb) b app_manager_launch_new_app
(lldb) r
Process 84855 stopped
* thread #10, name = 'KernelMain', stop reason = breakpoint 1.1
    frame #0: 0x00000001000f9a28 pebbleos`app_manager_launch_new_app(config=0x00000001702e6d70) at app_manager.c:713:43 [opt]
(lldb) thread list
...
* thread #10: tid = 0x3c26550, ... app_manager_launch_new_app(config=0x00000001702e6d70) at app_manager.c:713:43, name = 'KernelMain'
  thread #11: tid = 0x3c26551, ... libsystem_kernel.dylib`__psynch_cvwait + 8, name = 'NewTimer'
  thread #15: tid = 0x3c2655a, ... libsystem_kernel.dylib`__psynch_cvwait + 8, name = 'IDLE'
  thread #17: tid = 0x3c2657a, ... libsystem_kernel.dylib`__psynch_cvwait + 8, name = 'NimbleHost'
  thread #21: tid = 0x3c2657e, ... libsystem_kernel.dylib`__psynch_cvwait + 8, name = 'App <Launcher>'
```

The unnamed threads are the host's: the window, the timer, the serial
ports. `Ctrl-C` stops the firmware; the console is on TCP port 12345
meanwhile, where {doc}`../../pbl/console` attaches.
