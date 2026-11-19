# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The SDK platforms, as the builds know them.

A platform is a name and a description (see docs/sdk/platforms.md):

    {
        "version": 1,
        "name": "emery",
        "abi": "armv7m",
        "display": {"width": 200, "height": 228, "shape": "rect", "color": True},
        "features": ["microphone", "touch", ...],
        "limits": {"app_binary": 131072, "app_memory": 131072, ...},
        "moddable": True,
    }

`pebble_platforms` maps each known name to what the builds read: the
defines and resource tags derived from the description, and the limits.
The platforms come from

- the frozen platforms below, which no firmware is built for anymore;
- Kconfig.platform, in a firmware checkout, which defines the others;
- the `platform.json` of each platform of an SDK, when this is the SDK's
  copy.
"""

import json
import os
import re

DESCRIPTOR_VERSION = 1

ABIS = ("armv7m",)

# feature: (the define apps are given, the resource tag)
FEATURES = {
    "microphone": ("PBL_MICROPHONE", "mic"),
    "smartstrap": ("PBL_SMARTSTRAP", "strap"),
    "smartstrap_power": ("PBL_SMARTSTRAP_POWER", "strappower"),
    "health": ("PBL_HEALTH", "health"),
    "compass": ("PBL_COMPASS", "compass"),
    "touch": ("PBL_TOUCH", "touch"),
    "rgb_backlight": ("PBL_RGB_BACKLIGHT", None),
    "speaker": ("PBL_SPEAKER", "speaker"),
}

# limit: the key the builds read it by, and the Kconfig symbol less PLATFORM_
_LIMITS = {
    "app_binary": "MAX_APP_BINARY_SIZE",
    "app_memory": "MAX_APP_MEMORY_SIZE",
    "worker_memory": "MAX_WORKER_MEMORY_SIZE",
    "resources": "MAX_RESOURCES_SIZE",
    "resources_appstore": "MAX_RESOURCES_SIZE_APPSTORE",
    "resources_appstore_2x": "MAX_RESOURCES_SIZE_APPSTORE_2_X",
    "font_glyph": "MAX_FONT_GLYPH_SIZE",
}
_REQUIRED_LIMITS = tuple(k for k in _LIMITS if k != "resources_appstore_2x")

# What an SDK holds beside its platforms, and the directories of the ABIS.
_SDK_DIRECTORIES = ("cmake", "common")

_FROZEN_LIMITS = {
    "app_binary": 0x10000,
    "app_memory": 0x10000,
    "worker_memory": 0x2800,
    "resources": 0x100000,
    "resources_appstore": 0x40000,
    "font_glyph": 256,
}


def _display(width, height, shape, color):
    return {"width": width, "height": height, "shape": shape, "color": color}


# Platforms whose SDK is frozen: their apps still run, but nothing new is
# exported to them.
FROZEN_DESCRIPTORS = [
    {
        "name": "aplite",
        "abi": "armv7m",
        "display": _display(144, 168, "rect", False),
        "features": ["compass"],
        "limits": dict(
            _FROZEN_LIMITS,
            app_memory=0x6000,
            resources=0x80000,
            resources_appstore=0x20000,
            resources_appstore_2x=0x18000,
        ),
        "frozen_at_revision": 81,
    },
    {
        "name": "basalt",
        "abi": "armv7m",
        "display": _display(144, 168, "rect", True),
        "features": [
            "microphone",
            "smartstrap",
            "smartstrap_power",
            "health",
            "compass",
        ],
        "limits": dict(_FROZEN_LIMITS),
        "frozen_at_revision": 89,
    },
    {
        "name": "chalk",
        "abi": "armv7m",
        "display": _display(180, 180, "round", True),
        "features": [
            "microphone",
            "smartstrap",
            "smartstrap_power",
            "health",
            "compass",
        ],
        "limits": dict(_FROZEN_LIMITS),
        "frozen_at_revision": 89,
    },
    {
        "name": "diorite",
        "abi": "armv7m",
        "display": _display(144, 168, "rect", False),
        "features": ["microphone", "smartstrap", "health"],
        "limits": dict(_FROZEN_LIMITS),
        "frozen_at_revision": 89,
    },
]


class PlatformError(Exception):
    pass


def validate_descriptor(desc):
    """Raise PlatformError unless desc is a platform description."""
    name = desc.get("name")
    if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", name):
        raise PlatformError(
            f"platform name {name!r}: lowercase letters, digits and underscores, "
            "starting with a letter"
        )
    if desc.get("abi") not in ABIS:
        raise PlatformError(f"platform {name}: unknown ABI {desc.get('abi')!r}")
    if name in ABIS or name in _SDK_DIRECTORIES:
        raise PlatformError(f"platform name {name!r} is a directory of the SDK")
    display = desc.get("display")
    if not isinstance(display, dict):
        raise PlatformError(f"platform {name}: no display")
    for key in ("width", "height"):
        value = display.get(key)
        if (
            isinstance(value, bool)
            or not isinstance(value, int)
            or not 0 < value < 0x10000
        ):
            raise PlatformError(f"platform {name}: bad display {key} {value!r}")
    if display.get("shape") not in ("rect", "round"):
        raise PlatformError(f"platform {name}: display shape is 'rect' or 'round'")
    if not isinstance(display.get("color"), bool):
        raise PlatformError(f"platform {name}: display color is true or false")
    unknown = sorted(set(desc.get("features", [])) - set(FEATURES))
    if unknown:
        raise PlatformError(f"platform {name}: unknown features {', '.join(unknown)}")
    limits = desc.get("limits", {})
    for key in _REQUIRED_LIMITS:
        value = limits.get(key)
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            raise PlatformError(f"platform {name}: no '{key}' limit")
    unknown = sorted(set(limits) - set(_LIMITS))
    if unknown:
        raise PlatformError(f"platform {name}: unknown limits {', '.join(unknown)}")


def platform_from_descriptor(desc):
    """What the builds read of a platform, from its description."""
    validate_descriptor(desc)
    name = desc["name"]
    display = desc["display"]
    features = desc.get("features", [])
    frozen = desc.get("frozen_at_revision")

    defines = [
        f"PBL_PLATFORM_{name.upper()}",
        "PBL_COLOR" if display["color"] else "PBL_BW",
        "PBL_ROUND" if display["shape"] == "round" else "PBL_RECT",
    ]
    tags = [
        name,
        "color" if display["color"] else "bw",
        display["shape"],
    ]
    # In FEATURES order, whatever order the description lists them in.
    for feature, (define, tag) in FEATURES.items():
        if feature in features:
            defines.append(define)
            if tag:
                tags.append(tag)
    defines += [
        f"PBL_DISPLAY_WIDTH={display['width']}",
        f"PBL_DISPLAY_HEIGHT={display['height']}",
    ]
    tags += [f"{display['width']}w", f"{display['height']}h"]
    if frozen is not None:
        defines.append("PBL_SDK_FROZEN")
    if desc.get("moddable"):
        defines.append("PBL_MODDABLE")

    platform = {
        "NAME": name,
        "ABI": desc["abi"],
        "DEFINES": defines,
        "BUILD_DIR": name,
        "BUNDLE_BIN_DIR": name,
        "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
        "TAGS": tags,
        "DESCRIPTOR": desc,
    }
    for key, value in desc["limits"].items():
        platform[_LIMITS[key]] = value
    if frozen is not None:
        platform["FROZEN_AT_REVISION"] = frozen
    if desc.get("moddable"):
        platform["HAS_MODDABLE_XS"] = True
    return platform


def display_class(display):
    """The class of a display: its shape and colour depth, "rect-color"."""
    return f"{display['shape']}-{'color' if display['color'] else 'bw'}"


# What a flexible binary is given for its code and heap unless its project
# asks for more: what every platform gives.
FLEXIBLE_APP_MEMORY = 0x10000

_FLEXIBLE_NAME = re.compile(r"(rect|round)-(bw|color)((?:\+[a-z]+)*)")


def flexible_name(shape, color, requires=()):
    """The name of a flexible build, and of its directory in a pbw: its
    class and the resource tags of the features it requires,
    "rect-color+mic"."""
    tags = [FEATURES[f][1] for f in FEATURES if f in requires]
    return "+".join([display_class({"shape": shape, "color": color}), *tags])


def parse_flexible_name(name):
    """The shape, colour and required features a flexible build is named
    after, or None if the name is not that of one."""
    m = _FLEXIBLE_NAME.fullmatch(name)
    if not m:
        return None
    by_tag = {tag: feature for feature, (_, tag) in FEATURES.items() if tag}
    tags = m.group(3).split("+")[1:]
    if any(tag not in by_tag for tag in tags):
        return None
    requires = [by_tag[tag] for tag in tags]
    # One name for one build.
    if name != flexible_name(m.group(1), m.group(2) == "color", requires):
        return None
    return m.group(1), m.group(2) == "color", requires


def _flexible_limits(shape, color):
    """What a flexible binary may take: no more than any platform of its
    class gives."""
    display = {"shape": shape, "color": color}
    of_class = [
        p["DESCRIPTOR"]["limits"]
        for p in pebble_platforms.values()
        if "FROZEN_AT_REVISION" not in p
        and display_class(p["DESCRIPTOR"]["display"]) == display_class(display)
    ] or [_FROZEN_LIMITS]
    limits = {key: min(limits[key] for limits in of_class) for key in _REQUIRED_LIMITS}
    limits["app_binary"] = limits["app_memory"] = FLEXIBLE_APP_MEMORY
    return limits


def flexible_build(shape, color, requires=()):
    """What a flexible binary is built with, as the builds read a platform.
    It is of a class instead of a platform, and runs on every display of
    that class: it is given no display size and no platform name. Of the
    features it is only given those it requires, which every watch it runs
    on then has; it asks the watch for the others."""
    unknown = sorted(set(requires) - set(FEATURES))
    if unknown:
        raise PlatformError(f"unknown features {', '.join(unknown)}")
    untagged = sorted(f for f in requires if FEATURES[f][1] is None)
    if untagged:
        raise PlatformError(f"{', '.join(untagged)} cannot be required")
    requires = [f for f in FEATURES if f in requires]
    name = flexible_name(shape, color, requires)
    defines = [
        "PBL_FLEXIBLE",
        "PBL_COLOR" if color else "PBL_BW",
        "PBL_ROUND" if shape == "round" else "PBL_RECT",
    ]
    tags = ["color" if color else "bw", shape]
    for feature in requires:
        define, tag = FEATURES[feature]
        defines.append(define)
        tags.append(tag)
    limits = _flexible_limits(shape, color)
    platform = {
        "NAME": name,
        "ABI": ABIS[0],
        "DEFINES": defines,
        "BUILD_DIR": name,
        "BUNDLE_BIN_DIR": name,
        "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
        "TAGS": tags,
        "REQUIRES": requires,
        "FLEXIBLE": True,
        "DESCRIPTOR": {
            "version": DESCRIPTOR_VERSION,
            "name": name,
            "abi": ABIS[0],
            "flexible": True,
            "display": {"shape": shape, "color": color},
            "requires": requires,
            "limits": limits,
        },
    }
    for key, value in limits.items():
        platform[_LIMITS[key]] = value
    return platform


def flexible_targets(selectors, installed):
    """What a flexible project builds, out of the platforms an SDK has: a
    binary for each class of display, and one for each platform it names.
    A targetPlatforms entry names a platform, or a class by "rect",
    "round", "color" and "bw", with the features its binary cannot run
    without: "color~rect~mic". Without entries it is every class.

    Until whatever installs a pbw knows flexible binaries, the platforms of
    a class that have those features are built for as well."""
    live = [p for p in installed if "FROZEN_AT_REVISION" not in pebble_platforms[p]]

    def display(platform):
        return pebble_platforms[platform]["DESCRIPTOR"]["display"]

    # The classes the SDK has a platform of, in the order of the platforms.
    classes = []
    for p in live:
        if (display(p)["shape"], display(p)["color"]) not in classes:
            classes.append((display(p)["shape"], display(p)["color"]))
    if not classes:
        raise PlatformError("This SDK has no platform to build a flexible binary for")

    by_tag = {tag: feature for feature, (_, tag) in FEATURES.items() if tag}
    builds = []

    def add(name):
        if name not in builds:
            builds.append(name)

    for selector in selectors or [""]:
        if selector in pebble_platforms:
            if selector in installed:
                add(selector)
            continue
        tags = set(filter(None, selector.split("~")))
        unknown = sorted(tags - {"rect", "round", "color", "bw"} - set(by_tag))
        if unknown:
            raise PlatformError(
                f"targetPlatforms entry '{selector}': in a flexible project an entry "
                f"names a platform or a class and the features it requires, and "
                f"{', '.join(unknown)} is neither"
            )
        requires = [by_tag[tag] for tag in tags if tag in by_tag]
        for shape, color in classes:
            if not tags.isdisjoint({"rect", "round"}) and shape not in tags:
                continue
            if (
                not tags.isdisjoint({"color", "bw"})
                and ("color" if color else "bw") not in tags
            ):
                continue
            add(flexible_name(shape, color, requires))
            for p in live:
                features = pebble_platforms[p]["DESCRIPTOR"].get("features", [])
                if (display(p)["shape"], display(p)["color"]) == (
                    shape,
                    color,
                ) and set(requires) <= set(features):
                    add(p)
    return builds


def bezel_platforms(built, installed):
    """Which of an SDK's platforms a project that builds for fixed displays
    does not fill: {platform: the built platform whose binary it shows in a
    bezel, or None if it runs none of them}. A platform the project builds
    for, and a frozen one, is not in it."""
    report = {}
    for name in installed:
        platform = pebble_platforms[name]
        if name in built or "FROZEN_AT_REVISION" in platform:
            continue
        display = platform["DESCRIPTOR"]["display"]
        fitting = []
        for other in built:
            try:
                desc = pebble_platforms[other]["DESCRIPTOR"]
            except KeyError:
                continue
            if desc.get("flexible"):
                continue
            built_for = desc["display"]
            if (
                built_for["shape"] == display["shape"]
                and built_for["width"] <= display["width"]
                and built_for["height"] <= display["height"]
                and (display["color"] or not built_for["color"])
                and desc["limits"]["app_memory"]
                <= platform["DESCRIPTOR"]["limits"]["app_memory"]
            ):
                # The largest, and of those the one of the platform's colours.
                fitting.append(
                    (
                        built_for["width"] * built_for["height"],
                        built_for["color"] == display["color"],
                        other,
                    )
                )
        if any(f[0] == display["width"] * display["height"] for f in fitting):
            continue
        report[name] = max(fitting)[2] if fitting else None
    return report


class _Platforms(dict):
    """The platforms by name. A flexible build is looked up by its name
    like a platform, but is not one of them: it is not listed."""

    def __missing__(self, name):
        parsed = parse_flexible_name(name)
        if parsed is None:
            raise KeyError(name)
        return flexible_build(*parsed)


def descriptor_from_kconfig(symbols):
    """A platform's description out of its Kconfig.platform symbols, given
    as {name: value} without the CONFIG_ prefix; unset bools are left out."""

    def enabled(name):
        return symbols.get(name) in ("1", "y", True, 1)

    def number(name):
        return int(str(symbols[name]), 0)

    desc = {
        "version": DESCRIPTOR_VERSION,
        "name": str(symbols.get("PLATFORM_NAME", "")).strip('"'),
        "abi": str(symbols.get("PLATFORM_ABI", "")).strip('"'),
        "display": _display(
            number("PLATFORM_DISPLAY_WIDTH"),
            number("PLATFORM_DISPLAY_HEIGHT"),
            "round" if enabled("PLATFORM_ROUND") else "rect",
            enabled("PLATFORM_COLOR"),
        ),
        "features": [f for f in FEATURES if enabled(f"PLATFORM_{f.upper()}")],
        "limits": {
            key: number(f"PLATFORM_{symbol}")
            for key, symbol in _LIMITS.items()
            if f"PLATFORM_{symbol}" in symbols
        },
    }
    if enabled("PLATFORM_MODDABLE_XS"):
        desc["moddable"] = True
    validate_descriptor(desc)
    return desc


def kconfig_symbols(kconf):
    """The PLATFORM_* symbols of an evaluated Kconfig, as
    descriptor_from_kconfig takes them."""
    import kconfiglib

    symbols = {}
    for sym in kconf.unique_defined_syms:
        if not sym.name.startswith("PLATFORM_"):
            continue
        if sym.type == kconfiglib.BOOL:
            if sym.tri_value == 2:
                symbols[sym.name] = "1"
        elif sym.str_value:
            symbols[sym.name] = sym.str_value
    return symbols


def _load_kconfig(kconfig_path):
    import kconfiglib

    saved = os.environ.get("srctree")
    os.environ["srctree"] = os.path.dirname(kconfig_path)
    try:
        return kconfiglib.Kconfig(kconfig_path, warn=False)
    finally:
        if saved is None:
            del os.environ["srctree"]
        else:
            os.environ["srctree"] = saved


def kconfig_platform(kconfig_path, choice):
    """Kconfig.platform, evaluated for one of its platforms, by the name of
    its choice symbol."""
    kconf = _load_kconfig(kconfig_path)
    sym = kconf.syms.get(choice)
    if sym is None or sym.choice is None:
        raise PlatformError(f"{choice} is not a platform of {kconfig_path}")
    sym.set_value(2)
    return kconf


def kconfig_descriptors(kconfig_path):
    """The descriptions of the platforms of Kconfig.platform."""
    choices = [
        sym.name
        for sym in _load_kconfig(kconfig_path).unique_defined_syms
        if sym.choice is not None
    ]
    return [
        descriptor_from_kconfig(kconfig_symbols(kconfig_platform(kconfig_path, choice)))
        for choice in choices
    ]


def load_descriptor(path):
    with open(path) as f:
        desc = json.load(f)
    if desc.get("version", DESCRIPTOR_VERSION) > DESCRIPTOR_VERSION:
        raise PlatformError(
            f"{path} is a version {desc['version']} platform description; "
            f"this SDK reads up to version {DESCRIPTOR_VERSION}"
        )
    try:
        validate_descriptor(desc)
    except PlatformError as e:
        raise PlatformError(f"{path}: {e}") from None
    return desc


def sdk_descriptors(sdk_root):
    """The descriptions of the platforms of an SDK: <sdk>/<name>/platform.json."""
    if not os.path.isdir(sdk_root):
        return []
    files = sorted(
        os.path.join(sdk_root, name, "platform.json") for name in os.listdir(sdk_root)
    )
    return [load_descriptor(path) for path in files if os.path.isfile(path)]


def sdk_component(sdk_root, platform_name, component):
    """Where an SDK keeps `component` ("include", "lib") for a platform: in
    the platform's own directory if it has one, otherwise in the directory
    the platforms of its ABI share. A frozen platform has only its own: what
    is shared is of a later revision."""
    platform = pebble_platforms[platform_name]
    directories = [platform_name]
    if "FROZEN_AT_REVISION" not in platform:
        directories.append(platform["ABI"])
    for directory in directories:
        path = os.path.join(sdk_root, directory, component)
        if os.path.exists(path):
            return path
    return None


def _load_platforms():
    here = os.path.dirname(os.path.abspath(__file__))
    descriptors = list(FROZEN_DESCRIPTORS)

    kconfig = os.path.join(os.path.dirname(here), "pebble", "Kconfig.platform")
    if os.path.isfile(kconfig):
        descriptors += kconfig_descriptors(kconfig)
    else:
        # In an SDK this is <sdk>/common/tools.
        descriptors += sdk_descriptors(os.path.dirname(os.path.dirname(here)))

    return _Platforms(
        (desc["name"], platform_from_descriptor(desc)) for desc in descriptors
    )


def bundle_description(platform_name, app_memory=None):
    """What a pbw says of the platform one of its binaries was built for: the
    part of the platform's description a binary is matched to a watch by. A
    flexible binary has no display size, and says what it requires in place
    of what its platform has; `app_memory` is what its project asks for."""
    desc = pebble_platforms[platform_name]["DESCRIPTOR"]
    if desc.get("flexible"):
        return {
            "version": DESCRIPTOR_VERSION,
            "name": desc["name"],
            "abi": desc["abi"],
            "flexible": True,
            "display": dict(desc["display"]),
            "requires": list(desc["requires"]),
            "limits": {"app_memory": app_memory or desc["limits"]["app_memory"]},
        }
    return {
        "version": DESCRIPTOR_VERSION,
        "name": desc["name"],
        "abi": desc["abi"],
        "display": dict(desc["display"]),
        "features": [f for f in FEATURES if f in desc.get("features", [])],
        "limits": {"app_memory": desc["limits"]["app_memory"]},
    }


def platform_matches(platform_name, selectors):
    """Whether a platform is one a targetPlatforms list asks for. An entry
    names a platform or the capabilities it has to have, as the tags
    resource file names use: "emery", "round", "color~rect"."""
    # By its own name, which for a flexible build is not one of its tags.
    if platform_name in selectors:
        return True
    try:
        platform = pebble_platforms[platform_name]
    except KeyError:
        return False
    tags = set(platform["TAGS"])
    return any(set(selector.split("~")) <= tags for selector in selectors)


pebble_platforms = _load_platforms()
