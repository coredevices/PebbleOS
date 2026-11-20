# Moving an app to flexible binaries

A project built for platforms by name gets a binary for each, and runs in a
bezel on every display it has no binary for. A flexible project gets a
binary for each class of display, which fills every watch of the class
({doc}`platforms`, {doc}`app_build`). This page is about what changes in the
project and its source.

## Opting in

In the `pebble` object of `package.json`:

```json
"flexible": true
```

Without `targetPlatforms` that builds `rect-color`, `rect-bw` and
`round-color`, and for now a binary for each platform of those classes,
which a watch of that platform prefers. To keep reaching the frozen
platforms, name them:

```json
"flexible": true,
"targetPlatforms": ["rect", "round", "aplite", "basalt", "chalk", "diorite"]
```

A flexible project builds with CMake; `pebble convert-project` ports a
`wscript`.

## What the build tells you, and what it does not

A flexible binary is compiled without `PBL_DISPLAY_WIDTH`,
`PBL_DISPLAY_HEIGHT` and `PBL_PLATFORM_<NAME>`, so code that uses the first
two no longer compiles. Most of what ties an app to a display compiles
anyway:

| In the source | What happens in a flexible binary | Who finds it |
| --- | --- | --- |
| `PBL_DISPLAY_WIDTH`, `PBL_DISPLAY_HEIGHT` | Does not compile | The compiler |
| `GRect(0, 0, 144, 168)`, a literal size | Compiles; drawn for one display | `pebble doctor` |
| A table of points, `#define TOP_RIGHT 72` | Compiles; drawn for one display | Running it |
| `#if defined(PBL_PLATFORM_EMERY)` | Compiles; takes the `#else` everywhere | `pebble doctor` |
| `#if defined(PBL_HEALTH)` | Compiles; the feature is left out | `pebble doctor` |
| `PBL_IF_HEALTH_ELSE(a, b)` | Asks the watch when it runs | Nothing to find |
| A resource with `"targetPlatforms": ["emery"]` | In no flexible binary | Configuring warns |

So build it, run `pebble doctor`, and then look at it on more than one
display: the native simulators of Emery, Flint and Gabbro are three.

## Display size

Take every position and size from the bounds of the window or layer.

```c
// Before: the middle of a 144 x 168 display
s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));

// After
s_text_layer = text_layer_create(GRect(0, bounds.size.h / 2 - 12, bounds.size.w, 20));
```

A layout written for one display often measures from its edges or its
centre. Keep the offset and measure it from the bounds:

```c
// Before
GRect(46, 114, 27, 20)

// After: 26 left of the centre, 30 below it
const GPoint center = grect_center_point(&bounds);
GRect(center.x - 26, center.y + 30, 27, 20)
```

Constants that stand for a display become expressions of the bounds:

```c
// Before: the corners of a 144 x 168 display, as lengths along its perimeter
#define TOP_RIGHT 72
#define BOT_RIGHT 240

// After
#define DISPLAY (layer_get_bounds(window_get_root_layer(s_window)).size)
#define TOP_RIGHT (DISPLAY.w / 2)
#define BOT_RIGHT (TOP_RIGHT + DISPLAY.h)
```

A table of points drawn for one display, the hour marks of a watchface for
one, is the largest change: compute the points from the bounds, or scale the
table when the window loads. A table that was `const` then lives in RAM.

Shape and colour depth are the class, so `PBL_IF_ROUND_ELSE`,
`PBL_IF_COLOR_ELSE`, `COLOR_FALLBACK`, `PBL_ROUND` and `PBL_COLOR` work as
they did.

## Tuning for a platform

```c
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
#define SIDEBAR_WIDTH 34
#else
#define SIDEBAR_WIDTH 30
#endif
```

This usually means "a large display" and not the platform. Say that:

```c
#define SIDEBAR_WIDTH (bounds.size.w >= 200 ? 34 : 30)
```

Where the value sizes an array, size the array for the largest case.

## Features: two ways

A watch of a class may or may not have a microphone, health or a smartstrap
port. An app has two ways to deal with that, and can mix them.

**Ask when the app runs.** One binary per class, for every watch of it.

```c
if (platform_has_feature(PlatformFeatureHealth)) {
  health_service_events_subscribe(prv_health_handler, NULL);
}
```

`PBL_IF_MICROPHONE_ELSE` and `PBL_IF_HEALTH_ELSE` do this by themselves, so
an app that only uses those has nothing to change. `PBL_IF_SMARTSTRAP_ELSE`
does not: it is its second argument unless the binary requires a smartstrap.
Code under `#if defined(PBL_HEALTH)` has to become a check like the one
above, or it is left out of the binary without a word from the compiler.
Both branches are in the binary; a check costs a call.

`platform_has_feature()` is not in the SDK of the frozen platforms. A project
that still builds for those keeps their code under `#if`:

```c
#if defined(PBL_FLEXIBLE)
  const bool has_health = platform_has_feature(PlatformFeatureHealth);
#else
  const bool has_health = PBL_IF_HEALTH_ELSE(true, false);
#endif
```

`PBL_IF_HEALTH_ELSE(true, false)` alone says the same in every binary.

**Require the feature.** A binary only for watches that have it.

```json
"targetPlatforms": ["rect~health", "round~health"]
```

That builds `rect-color+health`, `rect-bw+health` and `round-color+health`.
In those `PBL_HEALTH` is defined and `PBL_IF_HEALTH_ELSE` is a constant, so
feature code stays as it is and the unused branch is not shipped. A watch
without the feature is not offered the app, unless the project also builds
the plain binary of the class:

```json
"targetPlatforms": ["rect~health", "rect", "round~health", "round"]
```

A watch then takes the binary that requires the most of what it has.

Which to pick:

| | Ask when it runs | Require |
| --- | --- | --- |
| Source changes for feature code | `#ifdef` becomes a check | None |
| Binaries per class | One | One per set of features, plus the plain one to reach every watch |
| Watches without the feature | Run the app | Need the plain binary, or do not get the app |
| Size | Both branches ship | Only the branch that is used |

## Resources

- A resource picks its variant by tags as before: `icon~color.png`,
  `icon~bw.png`, `icon~round.png`. A flexible binary has the tags of its
  class and of the features it requires.
- A resource's own `targetPlatforms` has to say what it is for with tags. One
  that names platforms is in no flexible binary, and configuring the project
  warns about it.
- Size tags (`~200w`) name one display and match no flexible binary.

## Packages

A flexible app needs its Pebble packages to be flexible too
({doc}`app_build`). A package is moved the same way as an app.

## Memory

A flexible binary is given 64K for its code and heap, whatever the watch
gives. An app that needs more asks with `"appMemory"` in `package.json`, and
then only fits watches that give that much.
