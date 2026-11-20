# Loader: Images

This sample loads a small image the way apps are loaded: the loader copies
its stored part to RAM, clears the part after it that is not stored, and
applies its relocations. Each relocation names a 32-bit slot that holds an
offset into the image, and turns it into the address it now has.

The image is made in C here, a header, three pointer slots and two strings,
with the relocation table after them. The loader refuses a relocation that
would patch the header, and an image larger than the memory given to it.

Relocated pointers are 32 bits wide, so the sample does not run natively on
64-bit hosts.

```{pebbleos-sample}
:path: samples/subsys/loader/image
:sources: src/main.c
:output:
```
