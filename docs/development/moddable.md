# Moddable JS Engine

PebbleOS supports the [Moddable SDK](https://github.com/pebble-dev/moddable)'s XS JavaScript engine.
The Moddable integration provides a lightweight sandbox for runningJavaScript applications and watchfaces on Pebble hardware.

## Building with Moddable

Moddable is enabled by default on platforms that support it.
It can also be explicitly enabled or disabled using the `--js-engine` configure option.

## Installable applications

The JavaScript is stored in a resource. A small native C application bootstraps JavaScript execution by calling `moddable_createMachine`.

The JavaScript itself is a Moddable SDK **mod**, which is a precompiled collection of modules.
The modules are compiled using the `mcrun` tool in the Moddable SDK. See Moddable's [Pebble Examples repository](https://github.com/moddable-OpenSource/pebble-examples) for further details.
The Pebble resource containing the precompiled mod is currently loaded into RAM for execution; eventually, it should be executed directly from flash (this is what the Moddable SDK does on most platforms).

## Sandbox

The JavaScript in installable applications runs inside a lightweight sandbox, which can limit access to certain JavaScript global variables and modules.

Installable applications can be either a normal application or a watchface.
The sandbox prevents watchfaces from subscribing to the Pebble hardware buttons (which is slightly redundant since PebbleOS also blocks them — but PebbleOS blocks them silently, whereas the sandbox throws an exception, which is more clear to developers).
