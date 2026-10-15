# How it works

## The app binary

An app is an ARM binary linked to run from address zero, and relocated
when it is loaded. It starts with a
`PebbleProcessInfo` header (`fw/process_management/pebble_process_info.h`)
that records, among others:

- the SDK version it was built with;
- its entry point, size and the size of its memory once loaded;
- the offset of a word left for the firmware to fill in, `sym_table_addr`;
- the number of entries in a relocation table appended to the image.

## Loading

The process loader (`fw/services/process_management/process_loader_storage.c`)
copies the app into the RAM reserved for it and prepares it:

1. It checks the header, the image size and its checksum.
2. It applies the relocations: each entry names a pointer in the image
   that is relative to the image, which the loader turns into an
   absolute address. The table is loaded over the start of `.bss`, so the
   loader zeroes it afterwards.
3. It writes the address of the firmware's exported function table,
   `g_pbl_system_tbl`, into the word `sym_table_addr` points at.

The app then runs as its own task, unprivileged, with the MPU limiting it
to its own memory.

## Calling the firmware

An app calling an SDK function goes through three steps:

```{mermaid}
sequenceDiagram
  participant app as App
  participant tramp as libpebble.a trampoline
  participant applib as fw/applib
  participant sys as sys_*() syscall
  app->>tramp: SDK function call
  tramp->>applib: jump through g_pbl_system_tbl[n]
  Note over app,applib: unprivileged, in the app's task
  applib->>sys: when it needs the system
  Note over sys: privileged, checks the app's pointers
  sys-->>applib: result
  applib-->>app: return
```

1. **Trampoline.** The function the app links against is a few
   instructions of `libpebble.a`. It loads the table address the loader
   wrote, takes the entry at the function's index, and jumps to it with the
   original arguments untouched.
2. **Applib.** The table, `g_pbl_system_tbl` in the generated
   `pebble.auto.c`, points at the implementation in `fw/applib/`. It runs
   in the app's task, still unprivileged, and does what needs no
   privileges itself: drawing into the app's framebuffer, walking the
   app's layers, handling its timers.
3. **Syscall.** What needs the kernel goes through a `sys_*` function
   defined with `PBL_SYSCALL` (`fw/syscall/syscall_define.h`). It raises
   the privilege level on entry and drops it on return, so the kernel and
   services run privileged for the duration of the call. Syscalls check
   whatever the app hands them: `PBL_SYSCALL` copies small arguments into
   kernel memory and checks that buffers and strings lie in the app's
   memory, and a failed check terminates the app rather than the firmware.

The position of a function in the table is its index in every app built
against it, so the table is append-only: new functions go at the end, in a
new SDK revision, and removed ones keep their slot. {doc}`exporting`
describes the rules.
