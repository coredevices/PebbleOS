# Settings

This sample keeps a settings file, a store of key-value records in a single
file on the {doc}`file system <../../../subsystems/fs>`. Each write appends a
record, so a reset never leaves a value half written; the file is compacted
when the dead records fill it.

It counts its runs in a record kept from one run to the next, writes and
reads two more, lists them all with `pbl_settings_each()` and deletes one.
Keys and values are bytes: the keys here are strings without their
terminator. A watch, registered before the file is written, reports each
change.

It is built with the `filesystem` {doc}`snippet
<../../../development/build_system/snippets>`, which enables the file system
and gives the board a partition for it.

```{pebbleos-sample}
:path: samples/subsys/settings/store
:sources: src/main.c
:output:
```
