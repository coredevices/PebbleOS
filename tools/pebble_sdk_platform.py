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

    platform = {
        "NAME": name,
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


def _load_platforms():
    here = os.path.dirname(os.path.abspath(__file__))
    descriptors = list(FROZEN_DESCRIPTORS)

    kconfig = os.path.join(os.path.dirname(here), "pebble", "Kconfig.platform")
    if os.path.isfile(kconfig):
        descriptors += kconfig_descriptors(kconfig)
    else:
        # In an SDK this is <sdk>/common/tools.
        descriptors += sdk_descriptors(os.path.dirname(os.path.dirname(here)))

    return {desc["name"]: platform_from_descriptor(desc) for desc in descriptors}


def bundle_description(platform_name):
    """What a pbw says of the platform one of its binaries was built for: the
    part of the platform's description a binary is matched to a watch by."""
    desc = pebble_platforms[platform_name]["DESCRIPTOR"]
    return {
        "version": DESCRIPTOR_VERSION,
        "name": desc["name"],
        "abi": desc["abi"],
        "display": dict(desc["display"]),
        "features": [f for f in FEATURES if f in desc.get("features", [])],
        "limits": {"app_memory": desc["limits"]["app_memory"]},
    }


pebble_platforms = _load_platforms()
