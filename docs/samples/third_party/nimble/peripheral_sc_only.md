# NimBLE: Secure Connections peripheral

This sample only lets a peer in with LE Secure Connections and a passkey:
security mode 1, level 4. Its `prj.conf` turns off legacy pairing, asks for
bonding and protection against a man in the middle, and declares that the
watch can display but not type, so the peer types the passkey the sample
prints on the console.

Its one characteristic is only readable over a link encrypted with an
authenticated key. The sample also asks a peer to secure the link as soon as
it connects. A bonded peer that has lost its keys is forgotten, and pairs
again.

Bonds are kept by NimBLE's config store, which PebbleOS saves in a settings
file: the sample mounts the filesystem first, and lists the bonded peers at
boot. The board overlays give each board a filesystem partition.

```{pebbleos-sample}
:path: samples/third_party/nimble/peripheral_sc_only
:sources: src/main.c prj.conf
:output:
```
