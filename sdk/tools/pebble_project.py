# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What a project's package.json says, shared by the CMake and waf app
builds. Nothing here knows about either build system: problems are raised
as ProjectError for the caller to report."""

import json
import os
import re

from pebble_package import LibraryPackage


class ProjectError(Exception):
    pass


# Must match the phone app's PermissionKind and PluginDefinedPermissions.
PLUGIN_PERMISSION_KINDS = ("exfiltrating", "read", "write")
RESERVED_PLUGIN_PERMISSION_NAMES = ("InvokeAction",)

# appinfo.json features the phone app only knows from a manifestVersion 2 pbw.
MANIFEST_V2_KEYS = ("plugin", "usesPermissions", "configPage")

# What the phone app calls the watch microphone, in usesPermissions.
WATCH_MICROPHONE = "WatchMicrophone"
_MICROPHONE_CALL = re.compile(r"\b(mic_capture_start|mic_stream_to_phone_start)\s*\(")

# Message keys are numbered from here, in the order they are declared.
FIRST_MESSAGE_KEY = 10000


def validate_message_keys_object(project_info, info_json_type):
    if "appKeys" in project_info and info_json_type == "package.json":
        raise ProjectError(
            "Project contains an invalid object `appKeys` in package.json. Please use "
            "`messageKeys` instead."
        )
    if "messageKeys" in project_info and info_json_type == "appinfo.json":
        raise ProjectError(
            "Project contains an invalid object `messageKeys` in appinfo.json. Please use "
            "`appKeys` instead."
        )


def validate_version(original_version):
    """The app version as MAJOR.MINOR[.PATCH], the form the firmware takes."""
    version = original_version.split(".")
    if len(version) > 3:
        raise ProjectError(
            "App versions must be of the format MAJOR or MAJOR.MINOR or MAJOR.MINOR.PATCH. "
            f"An invalid version of {original_version} was specified for the app. "
            f"Try {version[0]}.{version[1]}.{version[2]} instead"
        )
    elif not (0 <= int(version[0]) <= 255):
        raise ProjectError(
            f"An invalid or out of range value of {version[0]} was specified for the major "
            "version of the app. The valid range is 0-255."
        )
    elif not (0 <= int(version[1]) <= 255):
        raise ProjectError(
            f"An invalid or out of range value of {version[1]} was specified for the minor "
            "version of the app. The valid range is 0-255."
        )
    # The patch version never reaches the firmware, so it has no upper limit.
    if len(version) == 3:
        int(version[2])

    return ".".join(version[:3])


def app_info_from_package_json(info_json):
    """The project info appinfo.json is written from, out of a package.json
    that has a "pebble" object."""
    project_info = info_json["pebble"]
    validate_message_keys_object(project_info, "package.json")

    project_info["name"] = info_json["name"]
    project_info["shortName"] = project_info["longName"] = project_info["displayName"]

    if not info_json.get("version"):
        raise ProjectError("Project is missing a version")
    project_info["versionLabel"] = validate_version(info_json["version"])

    author = info_json.get("author")
    if isinstance(author, str):
        project_info["companyName"] = author.split("(", 1)[0].split("<", 1)[0].strip()
    elif isinstance(author, dict) and "name" in author:
        project_info["companyName"] = author["name"]
    else:
        raise ProjectError("Missing author name in project info")
    return project_info


def validate_manifest_version(project_info):
    """Check the project's manifestVersion, which defaults to 2. The phone
    app treats a version 1 pbw as a legacy app and ignores the newer appinfo
    features."""
    version = project_info.get("manifestVersion", 2)
    if isinstance(version, bool) or not isinstance(version, int) or version < 1:
        raise ProjectError("'manifestVersion' must be 1 or 2")
    if version < 2:
        used = [key for key in MANIFEST_V2_KEYS if key in project_info]
        if used:
            raise ProjectError(
                f"{', '.join(repr(key) for key in used)} {'needs' if len(used) == 1 else 'need'} "
                "manifestVersion 2, the default: "
                "remove '\"manifestVersion\": 1' from package.json"
            )


def bundled_file_paths(project_info):
    """The project files the phone app reads out of the pbw by name: the
    plugin script, and a config page shipped in the pbw rather than hosted at
    an http(s) URL. Paths are relative to the project root."""
    paths = []
    plugin = project_info.get("plugin")
    if isinstance(plugin, dict):
        paths.append(plugin.get("script", "plugin.js"))
    config_page = project_info.get("configPage")
    if isinstance(config_page, str) and not config_page.startswith(
        ("http://", "https://")
    ):
        paths.append(config_page)
    return paths


def validate_plugin(project_info, project_dir):
    """Check the project's `plugin` block, and that the files appinfo.json
    names exist, the way the phone app will when it loads the pbw."""
    plugin = project_info.get("plugin")
    if plugin is not None and not isinstance(plugin, dict):
        raise ProjectError("'plugin' must be an object")
    if project_info.get("projectType") == "plugin" and plugin is None:
        raise ProjectError("A plugin project needs a 'plugin' block in package.json")

    for permission in (plugin or {}).get("definesPermissions", []):
        name = permission.get("name", "")
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", name):
            raise ProjectError(f"Plugin permission name '{name}' must be alphanumeric")
        if name in RESERVED_PLUGIN_PERMISSION_NAMES:
            raise ProjectError(f"Plugin permission name '{name}' is reserved")
        if not str(permission.get("label", "")).strip():
            raise ProjectError(f"Plugin permission '{name}' has no label")
        if permission.get("kind") not in PLUGIN_PERMISSION_KINDS:
            raise ProjectError(
                f"Plugin permission '{name}' needs a kind: one of "
                f"{', '.join(PLUGIN_PERMISSION_KINDS)}"
            )

    for path in bundled_file_paths(project_info):
        if not os.path.exists(os.path.join(project_dir, path)):
            raise ProjectError(f"'{path}', named in package.json, does not exist")


def undeclared_microphone_warning(project_info, sources):
    """A warning when the app's C code starts the microphone without the
    permission the phone app needs to allow it, or None. Without it every
    start is refused.

    sources: (file name, file contents) pairs of the app's C sources."""
    declared = {
        p if isinstance(p, str) else p.get("name")
        for p in project_info.get("usesPermissions") or []
        if isinstance(p, (str, dict))
    }
    legacy = project_info.get("manifestVersion", 2) < 2
    if WATCH_MICROPHONE in declared and not legacy:
        return None
    for name, text in sources:
        match = _MICROPHONE_CALL.search(text)
        if match is None:
            continue
        if legacy:
            return (
                f"{name} calls {match.group(1)}(), but the phone app denies the microphone to "
                f"manifestVersion 1 apps: remove '\"manifestVersion\": 1' and declare "
                f"'{WATCH_MICROPHONE}' in usesPermissions"
            )
        return (
            f"{name} calls {match.group(1)}(), but package.json doesn't declare "
            f"'{WATCH_MICROPHONE}' in usesPermissions, so the phone app will deny the microphone"
        )
    return None


def no_permissions_warning(project_info):
    """A warning for an app with PebbleKit JS that declares no permissions,
    or None."""
    if project_info.get("manifestVersion", 2) >= 2 and not project_info.get(
        "usesPermissions"
    ):
        return (
            "package.json declares no usesPermissions: on the phone, this app's JavaScript "
            "can't make network requests and has to ask for anything else it uses, such as "
            "location. Declare them, or set '\"manifestVersion\": 1' to keep the old behaviour."
        )
    return None


def assign_message_keys(project_info, libraries, is_library):
    """Number the project's message keys and those of its libraries.

    Returns ({key: number}, [keys declared as arrays, e.g. "key[4]"])."""
    keys = project_info.get("messageKeys", project_info.get("appKeys", []))

    key_list = []
    key_dict = {}
    block_message_keys = []
    if keys:
        if isinstance(keys, list):
            key_list = list(keys)
        elif isinstance(keys, dict):
            if is_library:
                raise ProjectError(
                    "Libraries can only specify an array of messageKeys; other object types "
                    "are not supported."
                )
            key_dict = dict(keys)
        else:
            raise ProjectError(
                "You have specified an invalid messageKeys object in your project JSON file."
            )

    combined_key_list = key_list + list(key_dict.keys())
    for lib in libraries:
        if "pebble" not in lib or "messageKeys" not in lib["pebble"]:
            continue
        lib_keys = lib["pebble"]["messageKeys"]
        if not isinstance(lib_keys, list):
            raise ProjectError(
                "'{}' has an invalid messageKeys object. "
                "Libraries can only specify an messageKeys array.".format(lib["name"])
            )
        for key in lib_keys:
            if key in combined_key_list:
                raise ProjectError(
                    "The messageKey '{}' has already been used and cannot be re-used by "
                    "the {} library.".format(key, lib["name"])
                )
            combined_key_list.append(key)
        key_list.extend(lib_keys)

    if key_list:
        next_key = FIRST_MESSAGE_KEY
        multi_keys = [key for key in key_list if "]" in key]
        single_keys = [key for key in key_list if "]" not in key]

        for key in multi_keys:
            try:
                key_name, num_keys = re.findall(r"([\w]+)\[(\d+)\]$", key)[0]
            except IndexError:
                suggested_key_name = key.split("[", 1)[0]
                raise ProjectError(
                    f"An invalid message key of `{key}` was specified. Verify that a valid "
                    "length is specified if you are trying to allocate an array of keys "
                    f"with a single identifier. For example, try `{suggested_key_name}[2]`."
                ) from None
            key_dict[key_name] = next_key
            next_key += int(num_keys)
            block_message_keys.append(key_name)
        key_dict.update(
            {value: key for key, value in enumerate(single_keys, start=next_key)}
        )

    return key_dict, block_message_keys


MESSAGE_KEYS_BANNER = """
//
// AUTOGENERATED BY BUILD
// DO NOT MODIFY - CHANGES WILL BE OVERWRITTEN
//

"""


def message_keys_header(message_keys):
    return (
        "#pragma once\n#include <stdint.h>\n"
        + MESSAGE_KEYS_BANNER
        + "".join(f"extern uint32_t MESSAGE_KEY_{k};\n" for k in sorted(message_keys))
    )


def message_keys_definitions(message_keys):
    return (
        "\n#include <stdint.h>\n"
        + MESSAGE_KEYS_BANNER
        + "".join(
            f"uint32_t MESSAGE_KEY_{k} = {v};\n"
            for k, v in sorted(message_keys.items())
        )
    )


def message_keys_json(message_keys):
    return json.dumps(message_keys, sort_keys=True, indent=4, separators=(",", ": "))


def appinfo_json(project_info, libraries, block_message_keys):
    """The appinfo.json that goes in the pbw."""
    appinfo = dict(project_info)
    capabilities = list(appinfo.get("capabilities", []))
    for lib in libraries:
        if "pebble" in lib:
            capabilities.extend(lib["pebble"].get("capabilities", []))
    appinfo["capabilities"] = sorted(set(capabilities))

    if "appKeys" in appinfo:
        appinfo["appKeys"] = {
            k: v for k, v in appinfo["appKeys"].items() if k not in block_message_keys
        }
    return appinfo


def truncate_to_32_bytes(name):
    return name[:30] + ".." if len(name) > 32 else name


def resolve_package(lib_dir, package, unpack_dir):
    """Read the package.json of a dependency in node_modules.

    A Pebble package ships its headers, archives, resources and JS in a
    dist.zip, which is unpacked into unpack_dir; its info gains "path",
    pointing there. Every package gains "js_paths", the JS it brings.

    Returns (package info, its dependencies)."""
    package_dir = os.path.join(lib_dir, package)
    package_json = os.path.join(package_dir, "package.json")
    if not os.path.exists(package_json):
        raise ProjectError(f"Missing package.json for {package}: run `npm install`?")
    with open(package_json) as f:
        info = json.load(f)

    if "pebble" in info:
        dist = os.path.join(package_dir, "dist.zip")
        if not os.path.exists(dist):
            raise ProjectError(
                f"Missing dist.zip file for {package}. Are you sure this is a Pebble library?"
            )
        LibraryPackage(dist).unpack(unpack_dir)
        info["path"] = unpack_dir
        js_root = os.path.join(unpack_dir, "js")
        exclude = ()
    else:
        js_root = package_dir
        exclude = (".min.js",)

    js_paths = []
    for root, dirs, files in os.walk(js_root):
        dirs[:] = sorted(d for d in dirs if d != "node_modules")
        js_paths.extend(
            os.path.join(root, f)
            for f in sorted(files)
            if f.endswith((".js", ".json")) and not f.endswith(exclude)
        )
    info["js_paths"] = js_paths
    return info, list(info.get("dependencies", {}))


def image_dimensions(path):
    """(width, height) of a PNG, PDC or PBI image, or of a resource object
    holding one."""
    import struct

    if path.endswith(".reso"):
        from resources.types.resource_object import ResourceObject

        reso = ResourceObject.load(path)
        definition = reso.definition
        storage = (
            definition.storage_format
            if definition.type == "bitmap"
            else definition.type
        )
        data = reso.data
        if storage == "pbi":
            return struct.unpack("<hh", data[8:12])
        if storage == "png":
            data = data[12:]
            return struct.unpack(">II", data[4:12])
        if storage == "raw" and data[4:8] == b"PDCI":
            return struct.unpack("<HH", data[14:18])
        raise ProjectError(f"Unsupported published resource type for {path}")

    with open(path, "rb") as f:
        data = f.read(24)
    if data[1:4] == b"PNG":
        return struct.unpack(">II", data[16:24])
    if data[:4] == b"PDCI":
        return struct.unpack("<HH", data[10:14])
    raise ProjectError(f"Unsupported published resource type for {path}")
