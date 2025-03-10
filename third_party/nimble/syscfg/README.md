# NimBLE syscfg.h generation

This folder contains a dummy project to assist with the generation of the
syscfg.h header needed by NimBLE. This file is crucial for the correct
operation of the stack.

## How to generate syscfg.h

First install Go and then `newt`, the tool used to build mynewt:

```shell
go install mynewt.apache.org/newt/newt@latest
export PATH=${PATH}:`go env GOPATH`/bin
```

Then, initialize the project:

```shell
newt upgrade --shallow=1
# Use NimBLE version from submodule
rm -rf repos/apache-mynewt-nimble
git clone ../mynewt-nimble repos/apache-mynewt-nimble
```

For each supported target `$TARGET` (see `targets` folder) run the build:

```shell
newt build $TARGET
```

Note that the build does not need to succeed to obtain `syscfg.h` (it can be
the case if building e.g. on macOS for native BSP)

Finally, copy The generated `syscfg.h`:

```shell
cp -r bin/targets/$TARGET/generated/include/syscfg ../port/include/$TARGET
```
