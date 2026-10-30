#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The steps of the CMake app build (see cmake/pebble.cmake in the SDK).

``configure`` runs when CMake configures the project: it reads
package.json once, writes what does not depend on the sources (appinfo,
message keys, linker inputs) and describes the project to CMake. The other
sub-commands are build steps; they take everything they need from the
project description configure wrote.
"""

import argparse
import copy
import json
import os
import shutil
import struct
import subprocess
import sys
import time
from string import Template

SDK_TOOLS = os.path.dirname(os.path.abspath(__file__))
SDK_COMMON = os.path.dirname(SDK_TOOLS)
SDK_ROOT = os.path.dirname(SDK_COMMON)
if SDK_TOOLS not in sys.path:
    sys.path.insert(0, SDK_TOOLS)

import pebble_project
from pebble_project import ProjectError
from pebble_sdk_platform import pebble_platforms

# Platforms whose SDK is older than this are 2.x: no PNG resources, and a
# different SDK define.
LAST_2X_SDK_VERSION = (5, 19)

PROJECT_TYPES = {
    None: "app",
    "native": "app",
    "package": "package",
    "moddable": "moddable",
    "plugin": "plugin",
}


def _write_if_changed(path, content):
    """Leave a file alone when its content is already right, so what depends
    on it is not rebuilt."""
    mode = "wb" if isinstance(content, bytes) else "w"
    try:
        with open(path, "rb" if isinstance(content, bytes) else "r") as f:
            if f.read() == content:
                return
    except FileNotFoundError:
        pass
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, mode) as f:
        f.write(content)


def _warn(msg):
    print(f"warning: {msg}", file=sys.stderr)


def _load_project(path):
    with open(path) as f:
        return json.load(f)


def _sdk_version(platform_dir):
    major = minor = None
    with open(os.path.join(platform_dir, "include", "pebble_process_info.h")) as f:
        for line in f:
            if "PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR" in line:
                major = int(line.split()[2], 16)
            elif "PROCESS_INFO_CURRENT_SDK_VERSION_MINOR" in line:
                minor = int(line.split()[2], 16)
    return major, minor


def _installed_platforms(sdk_root):
    """The platforms this SDK can build for."""
    platforms = []
    for name in sorted(os.listdir(sdk_root)):
        platform_dir = os.path.join(sdk_root, name)
        if name in pebble_platforms and all(
            os.path.exists(os.path.join(platform_dir, p))
            for p in ("include/pebble.h", "lib/libpebble.a")
        ):
            platforms.append(name)
    return platforms


# --- configure ----------------------------------------------------------------


class _Bld:
    """What the resource generators expect of their build context."""

    def __init__(self, project_dir, build_dir, platform):
        import wafshim

        self.env = wafshim.Env(PLATFORM_NAME=platform)
        self.variant = ""
        self.path = wafshim.Node(project_dir, bld_root=build_dir, src_root=project_dir)

    def fatal(self, msg):
        raise ProjectError(msg)


def _load_generators():
    """The resource types an app can declare."""
    import resources.resource_map.resource_generator_bitmap
    import resources.resource_map.resource_generator_font
    import resources.resource_map.resource_generator_pbi
    import resources.resource_map.resource_generator_png
    import resources.resource_map.resource_generator_raw  # noqa: F401


def _resource_definitions(project, platform):
    """The resources the project declares for one platform, as resource
    definitions, with the per-platform compatibility rewrites applied."""
    from resources.resource_map import resource_generator

    _load_generators()
    bld = _Bld(project["project_dir"], project["build_dir"], platform)
    sdk_version = tuple(project["platforms"][platform]["sdk_version"])
    definitions = []
    for r in project["resources"]:
        if r.get("type") == "js":
            raise ProjectError(f"Resource {r['name']}: JS resources are not supported")
        for d in resource_generator.definitions_from_dict(
            bld, r, project["resources_dir"]
        ):
            if not d.is_in_target_platform(bld):
                continue
            if d.type == "png-trans":
                # One png-trans entry stands for two resources, a white and a
                # black mask, each with its own ID.
                for suffix in ("WHITE", "BLACK"):
                    masked = copy.deepcopy(d)
                    masked.name = f"{d.name}_{suffix}"
                    definitions.append(masked)
                continue
            if d.type == "png" and sdk_version <= LAST_2X_SDK_VERSION:
                d.type = "pbi"
            definitions.append(d)

    if project["type"] == "moddable":
        from resources.types.resource_definition import ResourceDefinition

        mod = os.path.join(project["build_dir"], "mods", platform, "mc.xsa")
        definitions.append(
            ResourceDefinition(
                type="raw", name="MOD", file=mod, target_platforms=[platform]
            )
        )
    return definitions


def _package_resources(project, platform):
    """The resources the project's Pebble packages bring for one platform, as
    {name: .reso path}."""
    resos = {}
    for lib in project["libraries"]:
        media = lib.get("pebble", {}).get("resources", {}).get("media")
        if not media:
            continue
        resources_dir = os.path.join(lib["path"], "resources", platform)
        if not os.path.isdir(resources_dir):
            raise ProjectError(
                f"Package {lib['name']} is missing resources for the {platform} platform"
            )
        for resource in media:
            if platform not in resource.get("targetPlatforms", [platform]):
                continue
            reso = os.path.join(
                resources_dir, f"{resource['file']}.{resource['name']}.reso"
            )
            if not os.path.exists(reso):
                raise ProjectError(
                    f"Package {lib['name']} is missing the {resource['name']} resource for "
                    f"the {platform} platform"
                )
            resos[resource["name"]] = reso
    return resos


def _resource_inputs(project, platform):
    inputs = []
    for d in _resource_definitions(project, platform):
        inputs.extend(os.path.join(project["project_dir"], s) for s in d.sources)
    inputs.extend(_package_resources(project, platform).values())
    return sorted(set(inputs))


def _cmake_value(values):
    if isinstance(values, bool):
        return "TRUE" if values else "FALSE"
    if not isinstance(values, (list, tuple)):
        values = [values]
    items = []
    for v in values:
        v = str(v)
        if ";" in v:
            raise ProjectError(f"'{v}': paths and values cannot contain ';'")
        items.append(v.replace("\\", "/").replace('"', '\\"').replace("$", "\\$"))
    return '"' + ";".join(items) + '"'


def _package_archive(lib, platform):
    """The archive a Pebble package ships for a platform."""
    scope, _, name = lib["name"].rpartition("/")
    archive = os.path.join(lib["path"], "binaries", platform, scope, f"lib{name}.a")
    return archive if os.path.exists(archive) else None


def configure(args):
    project_dir = os.path.abspath(args.project_dir)
    build_dir = os.path.abspath(args.build_dir)
    sdk_root = os.path.abspath(args.sdk)
    # The resource generators look files up relative to the project.
    os.chdir(project_dir)

    package_json = os.path.join(project_dir, "package.json")
    if not os.path.exists(package_json):
        if os.path.exists(os.path.join(project_dir, "appinfo.json")):
            raise ProjectError(
                "appinfo.json projects build with waf only: run `pebble convert-project` "
                "to move to package.json"
            )
        raise ProjectError(f"Could not find {package_json}")
    with open(package_json) as f:
        info_json = json.load(f)
    if "pebble" not in info_json:
        raise ProjectError('package.json has no "pebble" object')

    project_type = PROJECT_TYPES.get(info_json["pebble"].get("projectType"))
    if project_type is None:
        raise ProjectError(
            f"projectType '{info_json['pebble']['projectType']}' does not build with CMake"
        )

    configure_depends = [package_json]

    if project_type == "package":
        info = info_json["pebble"]
        info["name"] = info_json["name"]
        pebble_project.validate_message_keys_object(info, "package.json")
        resources_dir = "src/resources"
    else:
        info = pebble_project.app_info_from_package_json(info_json)
        pebble_project.validate_manifest_version(info)
        pebble_project.validate_plugin(info, project_dir)
        info.setdefault("manifestVersion", 2)
        if project_type == "plugin":
            # Nothing to install on the watch, but the phone app still reads these.
            info.setdefault("resources", {"media": []})
            info.setdefault("watchapp", {"watchface": False})
        resources_dir = "resources"

    # Pebble packages, and the packages they depend on in turn.
    libraries = []
    lib_dir = os.path.join(project_dir, "node_modules")
    pending = list(info_json.get("dependencies", {}))
    seen = set()
    while pending:
        name = pending.pop(0)
        if name in seen:
            continue
        seen.add(name)
        unpack_dir = os.path.join(build_dir, "packages", name)
        lib, deps = pebble_project.resolve_package(lib_dir, name, unpack_dir)
        libraries.append(lib)
        pending.extend(deps)
        configure_depends.append(os.path.join(lib_dir, name, "package.json"))
        if "pebble" in lib:
            configure_depends.append(os.path.join(lib_dir, name, "dist.zip"))

    message_keys, block_keys = {}, []
    if project_type != "plugin":
        message_keys, block_keys = pebble_project.assign_message_keys(
            info, libraries, project_type == "package"
        )
        info["messageKeys"] = message_keys
        # Older phone apps read the keys from appKeys.
        info["appKeys"] = dict(message_keys)

    # Platforms.
    installed = _installed_platforms(sdk_root)
    requested = info.get("targetPlatforms") or installed
    platforms = [p for p in requested if p in installed]
    if args.platforms:
        unknown = sorted(set(args.platforms) - set(platforms))
        if unknown:
            raise ProjectError(
                f"PEBBLE_PLATFORMS names {', '.join(unknown)}, which this project does not "
                f"target or this SDK does not have: pick from {', '.join(platforms)}"
            )
        platforms = [p for p in platforms if p in args.platforms]
    if project_type == "plugin":
        platforms = []
    elif not platforms:
        raise ProjectError(
            f"No valid targetPlatforms specified in package.json. Valid options are "
            f"{', '.join(installed)}"
        )

    platform_info = {}
    for p in platforms:
        desc = pebble_platforms[p]
        sdk_dir = os.path.join(sdk_root, p)
        major, minor = _sdk_version(sdk_dir)
        platform_info[p] = {
            "sdk_dir": sdk_dir,
            "sdk_version": [major, minor],
            "frozen": "FROZEN_AT_REVISION" in desc,
            "defines": desc["DEFINES"],
            "bundle_dir": desc["BUNDLE_BIN_DIR"],
            "max_app_memory_size": desc["MAX_APP_MEMORY_SIZE"],
            "max_worker_memory_size": desc["MAX_WORKER_MEMORY_SIZE"],
            "max_app_binary_size": desc["MAX_APP_BINARY_SIZE"],
            "max_resources_size": desc["MAX_RESOURCES_SIZE"],
            "max_resources_size_appstore": desc["MAX_RESOURCES_SIZE_APPSTORE"],
        }

    project = {
        "type": project_type,
        "name": info["name"],
        "project_dir": project_dir,
        "build_dir": build_dir,
        "info": info,
        "resources_dir": resources_dir,
        "resources": info.get("resources", {}).get("media", []),
        "published_media": info.get("resources", {}).get("publishedMedia", []),
        "libraries": libraries,
        "message_keys": message_keys,
        "block_message_keys": block_keys,
        "platforms": platform_info,
    }
    # Which file each resource comes from on each platform. Recorded here, so
    # that a different pick (a ~color variant added, say) rebuilds them.
    for p, pinfo in platform_info.items():
        pinfo["resource_inputs"] = _resource_inputs(project, p)

    menu_icons = {r["name"] for r in project["resources"] if r.get("menuIcon")}
    checked = set()
    for p in platform_info:
        for d in _resource_definitions(project, p):
            if d.name in menu_icons and d.sources[0] not in checked:
                checked.add(d.sources[0])
                if not _fits(d.sources[0], (25, 25)):
                    _warn(f"menuIcon {d.sources[0]} is larger than 25 x 25")

    # Stamped into the binaries and the pbw. Nothing depends on it: a
    # reconfigure does not rebuild them.
    _write_if_changed(
        os.path.join(build_dir, "pebble", "timestamp"),
        str(args.timestamp or int(time.time())),
    )
    _write_if_changed(
        os.path.join(build_dir, "pebble", "project.json"),
        json.dumps(project, indent=2, sort_keys=True),
    )

    # Inputs that do not depend on the sources.
    include_dir = os.path.join(build_dir, "include")
    _write_if_changed(
        os.path.join(include_dir, "message_keys.auto.h"),
        pebble_project.message_keys_header(message_keys),
    )
    if project_type != "package":
        if message_keys:
            _write_if_changed(
                os.path.join(build_dir, "src", "message_keys.auto.c"),
                pebble_project.message_keys_definitions(message_keys),
            )
            _write_if_changed(
                os.path.join(build_dir, "js", "message_keys.json"),
                pebble_project.message_keys_json(message_keys),
            )
        _write_if_changed(
            os.path.join(build_dir, "appinfo.json"),
            json.dumps(
                pebble_project.appinfo_json(info, libraries, block_keys), indent=4
            ),
        )

    from generate_appinfo import generate_appinfo_c

    template = os.path.join(SDK_COMMON, "pebble_app.ld.template")
    with open(template) as f:
        ldscript_template = f.read()
    with open(os.path.join(SDK_COMMON, "pebble_app_init_arrays.ld")) as f:
        init_arrays = f.read()
    configure_depends.append(template)

    for p, pinfo in platform_info.items():
        platform_dir = os.path.join(build_dir, p)
        _write_if_changed(
            os.path.join(platform_dir, "pebble_app.ld"),
            ldscript_template.replace(
                "@MAX_APP_MEMORY_SIZE@", str(pinfo["max_app_memory_size"])
            ).replace("@INIT_ARRAYS@", "" if pinfo["frozen"] else init_arrays),
        )
        if project_type != "package":
            appinfo = dict(info)
            appinfo["shortName"] = pebble_project.truncate_to_32_bytes(
                appinfo["shortName"]
            )
            appinfo["companyName"] = pebble_project.truncate_to_32_bytes(
                appinfo["companyName"]
            )
            tmp = os.path.join(platform_dir, "appinfo.auto.c.tmp")
            generate_appinfo_c(appinfo, tmp, p)
            with open(tmp) as f:
                _write_if_changed(
                    os.path.join(platform_dir, "appinfo.auto.c"), f.read()
                )
            os.remove(tmp)

    # A file added next to a resource can change which one a platform picks.
    resources_root = os.path.join(project_dir, resources_dir)
    if os.path.isdir(resources_root):
        for root, dirs, _ in os.walk(resources_root):
            dirs.sort()
            configure_depends.append(root)

    out = []

    def emit(name, value):
        out.append(f"set({name} {_cmake_value(value)})")

    emit("PEBBLE_PROJECT_TYPE", project_type)
    emit("PEBBLE_PROJECT_NAME", info["name"])
    emit("PEBBLE_PLATFORMS", platforms)
    emit("PEBBLE_MESSAGE_KEYS", bool(message_keys))
    emit("PEBBLE_ENABLE_MULTI_JS", bool(info.get("enableMultiJS", False)))
    emit("PEBBLE_JS_ENTRY", (info.get("main") or {}).get("pkjs", ""))
    emit(
        "PEBBLE_BUNDLED_FILES",
        [os.path.join(project_dir, p) for p in pebble_project.bundled_file_paths(info)],
    )
    emit("PEBBLE_PACKAGE_JS", [js for lib in libraries for js in lib["js_paths"]])
    emit("PEBBLE_CONFIGURE_DEPENDS", configure_depends)
    for p, pinfo in platform_info.items():
        sdk_version = tuple(pinfo["sdk_version"])
        emit(f"PEBBLE_{p}_SDK_DIR", pinfo["sdk_dir"])
        emit(f"PEBBLE_{p}_FROZEN", pinfo["frozen"])
        emit(
            f"PEBBLE_{p}_DEFINES",
            pinfo["defines"]
            + ["PBL_SDK_2" if sdk_version <= LAST_2X_SDK_VERSION else "PBL_SDK_3"],
        )
        # pebble.h declares the time API with a 32-bit time_t, which libc's
        # time.h must not redefine.
        emit(f"PEBBLE_{p}_OWN_TIME_H", sdk_version > LAST_2X_SDK_VERSION)
        emit(f"PEBBLE_{p}_RESOURCE_INPUTS", pinfo["resource_inputs"])
        pebble_libs = [lib for lib in libraries if "pebble" in lib]
        includes = []
        for lib in pebble_libs:
            lib_include = os.path.join(lib["path"], "include")
            lib_platform_include = os.path.join(lib_include, lib["name"], p)
            if not os.path.isdir(lib_platform_include):
                raise ProjectError(
                    f"The Pebble package '{lib['name']}' doesn't support the '{p}' platform"
                )
            includes += [lib_include, lib_platform_include]
        emit(f"PEBBLE_{p}_PACKAGE_INCLUDES", includes)
        archives = []
        for lib in pebble_libs:
            if os.path.isdir(os.path.join(lib["path"], "binaries")):
                archive = _package_archive(lib, p)
                if archive is None:
                    raise ProjectError(
                        f"Package {lib['name']} is missing a binary for the {p} platform"
                    )
                archives.append(archive)
        emit(f"PEBBLE_{p}_PACKAGE_ARCHIVES", archives)

    _write_if_changed(
        os.path.join(build_dir, "pebble", "project.cmake"), "\n".join(out) + "\n"
    )


# --- resources ----------------------------------------------------------------


def _timeline_table(project, platform, id_mapping, out_path):
    """The table the firmware maps publishedMedia IDs to resource IDs with,
    as a resource."""
    from resources.types.resource_definition import ResourceDefinition
    from resources.types.resource_object import ResourceObject

    lib_media = [
        m
        for lib in project["libraries"]
        for m in lib.get("pebble", {}).get("resources", {}).get("publishedMedia", [])
    ]
    table = []
    for item in copy.deepcopy(project["published_media"]):
        timeline = item.get("timeline", {})
        if "glance" in item:
            if "tiny" in timeline and timeline["tiny"] != item["glance"]:
                raise ProjectError(
                    f"Resource {item['name']} in publishedMedia specifies different values "
                    "for ['glance'] and ['timeline']['tiny']: these must match."
                )
            timeline["tiny"] = item["glance"]
        elif "tiny" not in timeline:
            if "alias" not in item:
                raise ProjectError(
                    f"Resource {item['name']} in publishedMedia is missing values for "
                    "['glance'] and ['timeline']['tiny']."
                )
            for definition in lib_media:
                if definition["name"] == item["alias"]:
                    timeline = dict(definition.get("timeline", {}))
                    if "glance" in definition:
                        timeline.setdefault("tiny", definition["glance"])
                    break
            else:
                raise ProjectError(
                    f"No resource for alias '{item['alias']}' exists in installed packages"
                )
        while item["id"] >= len(table):
            table.append({"tiny": 0, "small": 0, "large": 0})
        for size, res_id in timeline.items():
            if res_id not in id_mapping:
                raise ProjectError(
                    f"Invalid resource ID {res_id} specified in publishedMedia"
                )
            table[item["id"]][size] = id_mapping[res_id]

    data = b"TLUT" + b"".join(
        struct.pack("<III", r["tiny"], r["small"], r["large"]) for r in table
    )
    from wafshim import Node

    ResourceObject(ResourceDefinition("raw", "TIMELINE_LUT", ""), data).dump(
        Node(out_path)
    )


def _fits(path, max_size):
    width, height = pebble_project.image_dimensions(path)
    return width <= max_size[0] and height <= max_size[1]


def _check_published_media(project, platform, files):
    """Published media must have IDs, unique ones, and images small enough
    for where they show."""
    max_sizes = {
        "glance": (25, 25),
        "tiny": (25, 25),
        "small": (50, 50),
        "large": (80, 80),
    }
    used_ids = set()
    for item in project["published_media"]:
        if "id" not in item:
            if project["type"] == "package":
                continue
            raise ProjectError(
                f"Missing 'id' attribute for publishedMedia item '{item['name']}'"
            )
        if item["id"] in used_ids:
            raise ProjectError(
                "Cannot specify multiple resources with the same publishedMedia ID. Please "
                f"modify your publishedMedia items to only use the ID {item['id']} once"
            )
        used_ids.add(item["id"])
        uses = [("glance", item["glance"])] if "glance" in item else []
        uses += list(item.get("timeline", {}).items())
        for size, res_id in uses:
            if res_id not in files:
                raise ProjectError(
                    f"No resource '{res_id}' found for publishedMedia use."
                )
            if not _fits(files[res_id], max_sizes[size]):
                _warn(
                    f"publishedMedia item '{item['name']}' uses resource '{res_id}' for "
                    f"'{size}', which exceeds the maximum of {max_sizes[size][0]} x "
                    f"{max_sizes[size][1]}"
                )


def resources(args):
    from resources.resource_map import resource_generator
    from resources.types.resource_object import ResourceObject
    from wafshim import Node, Task

    from resources import generators

    project = _load_project(args.project)
    platform = args.platform
    os.chdir(project["project_dir"])
    build_dir = project["build_dir"]
    out_dir = os.path.join(build_dir, platform)
    reso_dir = os.path.join(out_dir, "resources")
    is_package = project["type"] == "package"

    definitions = _resource_definitions(project, platform)
    bld = _Bld(project["project_dir"], build_dir, platform)

    # One resource object per resource, named after the file package.json
    # declares (not the platform's variant of it): that is how a package's
    # users find the resources it ships.
    declared = {}
    for r in project["resources"]:
        for suffix in ("", "_WHITE", "_BLACK"):
            declared[r["name"] + suffix] = r.get("file", "")
    own_resos = []
    files = {}
    for d in definitions:
        source = d.sources[0]
        rel = declared.get(d.name) or os.path.basename(source)
        out = os.path.join(reso_dir, f"{rel}.{d.name}.reso")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        task = Task(bld, [Node(source)], [Node(out)], path=bld.path)
        resource_generator.generate_object(task, d).dump(Node(out))
        own_resos.append((f"{platform}/{rel}.{d.name}.reso", out))
        files[d.name] = source

    lib_resos = _package_resources(project, platform)
    clash = sorted(set(lib_resos) & set(files))
    if clash:
        raise ProjectError(
            f"Duplicate resource IDs are not permitted: package resource {clash[0]} uses the "
            "same resource ID as another resource in this project."
        )
    files.update(lib_resos)

    reso_paths = [path for _, path in own_resos] + list(lib_resos.values())
    published_media = project["published_media"]
    id_mapping = None
    if published_media and not is_package:
        _check_published_media(project, platform, files)
        # The timeline lookup table is the first resource, and has to know
        # the IDs of the others.
        names = ["TIMELINE_LUT"] + [
            ResourceObject.load(p).definition.name for p in reso_paths
        ]
        id_mapping = {name: i for i, name in enumerate(names, start=1)}
        lut = os.path.join(out_dir, "timeline_resource_table.reso")
        _timeline_table(project, platform, id_mapping, lut)
        reso_paths.append(lut)
        layouts = {
            "resources": {"app://images/" + m["name"]: m["id"] for m in published_media}
        }
        _write_if_changed(
            os.path.join(out_dir, "layouts.json"), json.dumps(layouts, indent=8)
        )

    ball = os.path.join(out_dir, "resources.resball")
    generators.build_resource_ball(
        reso_paths, Node(ball), resource_id_mapping=id_mapping
    )

    header = os.path.join(out_dir, "src", "resource_ids.auto.h")
    definitions_c = os.path.join(out_dir, "src", "resource_ids.auto.c")
    os.makedirs(os.path.dirname(header), exist_ok=True)
    for path, build in (
        (
            header,
            lambda p: generators.build_resource_id_header(
                ball,
                p,
                use_extern=is_package,
                use_define=not is_package,
                published_media=published_media,
            ),
        ),
        (
            definitions_c,
            lambda p: generators.build_resource_id_definitions(
                ball, p, published_media=published_media
            ),
        ),
    ):
        build(path + ".tmp")
        with open(path + ".tmp") as f:
            _write_if_changed(path, f.read())
        os.remove(path + ".tmp")

    if is_package:
        _write_if_changed(
            os.path.join(out_dir, "package_resources.json"),
            json.dumps(own_resos, indent=2),
        )
    else:
        generators.build_pbpack(ball, os.path.join(out_dir, "app_resources.pbpack"))


# --- binaries -----------------------------------------------------------------


def binary(args):
    """The image the watch loads, out of a linked app or worker."""
    import inject_metadata
    from binutils import size
    from memory_reports import (
        app_appstore_resource_memory_error,
        app_memory_report,
        app_resource_memory_error,
    )

    os.environ["PATH"] = args.toolchain_bin + os.pathsep + os.environ["PATH"]
    project = _load_project(args.project)
    platform = project["platforms"][args.platform]
    if args.sources_list:
        with open(args.sources_list) as f:
            sources = [s for s in f.read().split(";") if s.endswith(".c")]
        texts = []
        for s in sources:
            with open(s, errors="replace") as f:
                texts.append((os.path.basename(s), f.read()))
        warning = pebble_project.undeclared_microphone_warning(project["info"], texts)
        if warning:
            _warn(warning)

    raw = os.path.splitext(args.output)[0] + ".raw.bin"
    subprocess.check_call(
        [
            args.objcopy,
            "-S",
            "-R",
            ".stack",
            "-R",
            ".priv_bss",
            "-R",
            ".bss",
            "-R",
            ".retained",
            "-O",
            "binary",
            args.elf,
            raw,
        ]
    )
    shutil.copyfile(raw, args.output)
    with open(args.timestamp) as f:
        timestamp = int(os.environ.get("SOURCE_DATE_EPOCH") or f.read())
    inject_metadata.inject_metadata(
        args.output,
        args.elf,
        args.resources,
        timestamp,
        allow_js=args.has_js,
        has_worker=args.has_worker,
        max_binary_size=platform["max_app_binary_size"],
    )

    ram = sum(size(args.elf))
    if args.kind == "worker":
        print(
            *app_memory_report(
                args.platform,
                "worker",
                ram,
                platform["max_worker_memory_size"],
                platform["max_worker_memory_size"] - ram,
            )[1:]
        )
        return
    res = os.stat(args.resources).st_size
    if res > platform["max_resources_size"]:
        print(
            app_resource_memory_error(
                args.platform, res, platform["max_resources_size"]
            )[1],
            file=sys.stderr,
        )
        sys.exit(1)
    if res > platform["max_resources_size_appstore"]:
        _warn(
            app_appstore_resource_memory_error(
                args.platform, res, platform["max_resources_size_appstore"]
            )[1]
        )
    print(
        *app_memory_report(
            args.platform,
            "app",
            ram,
            platform["max_app_memory_size"],
            platform["max_app_memory_size"] - ram,
            res,
            platform["max_resources_size_appstore"],
        )[1:]
    )


# --- JavaScript ---------------------------------------------------------------


def js(args):
    """PebbleKit JS: merged into one file with webpack, or copied as is."""
    project = _load_project(args.project)
    project_dir = project["project_dir"]
    build_dir = project["build_dir"]
    sources = [os.path.abspath(s) for s in args.sources]

    if not project["info"].get("enableMultiJS", False):
        for source in sources:
            src_root = os.path.join(project_dir, "src")
            rel = os.path.relpath(
                source,
                src_root if source.startswith(src_root + os.sep) else project_dir,
            )
            target = os.path.join(build_dir, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copyfile(source, target)
        return

    entry = os.path.abspath(args.entry)
    if entry not in sources:
        raise ProjectError(
            f"The JS entry file, {os.path.relpath(entry, project_dir)}, is not one of the JS "
            "sources: name the file run when the app launches with pebble_js(ENTRY ...)"
        )
    webpack_dir = os.path.join(SDK_TOOLS, "webpack")
    with open(os.path.join(webpack_dir, "webpack-config.js.pytemplate")) as f:
        config_template = f.read()

    output = os.path.abspath(args.output)
    aliases = {
        lib["name"]: os.path.join(lib["path"], "js")
        for lib in project["libraries"]
        if "pebble" in lib
    }
    aliases["app_package.json"] = os.path.join(project_dir, "package.json")
    values = {
        "IS_SANDBOX": False,
        # _pkjs_shared_additions.js has to come first.
        "ENTRY_FILENAMES": ["_pkjs_shared_additions.js", entry],
        "OUTPUT_PATH": os.path.dirname(output),
        "OUTPUT_FILENAME": os.path.basename(output),
        "RESOLVE_ROOTS": [
            os.path.join(SDK_COMMON, "include"),
            webpack_dir,
            args.node_modules,
            os.path.join(build_dir, "js"),
        ],
        "RESOLVE_ALIASES": aliases,
        "SOURCE_MAP_CONFIG": {"sourceMapFilename": os.path.basename(output) + ".map"},
    }
    config = os.path.join(build_dir, "webpack", "webpack.config.js")
    os.makedirs(os.path.dirname(config), exist_ok=True)
    with open(config, "w") as f:
        f.write(
            Template(config_template).substitute(
                {k: json.dumps(v, separators=(",\n", ": ")) for k, v in values.items()}
            )
        )

    env = dict(os.environ, NODE_PATH=args.node_modules)
    result = subprocess.run(
        [
            args.node,
            os.path.join(args.node_modules, ".bin", "webpack"),
            "--config",
            config,
        ],
        cwd=project_dir,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise ProjectError("JS bundling failed")

    # Keep the user's directory layout out of the source map shipped in the pbw.
    source_map = output + ".map"
    with open(source_map) as f:
        content = json.load(f)

    def clean(path):
        if not os.path.isabs(path):
            return path
        if path.startswith(project_dir + os.sep):
            return os.path.relpath(path, project_dir)
        return os.path.basename(path)

    content["sources"] = [clean(s) for s in content.get("sources", [])]
    with open(source_map, "w") as f:
        json.dump(content, f, separators=(",", ":"))


# --- bundles ------------------------------------------------------------------


def bundle(args):
    """The pbw."""
    import mkbundle

    project = _load_project(args.project)
    with open(args.manifest) as f:
        manifest = json.load(f)
    with open(args.timestamp) as f:
        timestamp = int(os.environ.get("SOURCE_DATE_EPOCH") or f.read())

    if manifest["js"] and project["type"] != "plugin":
        warning = pebble_project.no_permissions_warning(project["info"])
        if warning:
            _warn(warning)

    binaries = []
    for platform, files in manifest["platforms"].items():
        pinfo = project["platforms"][platform]
        layouts = os.path.join(project["build_dir"], platform, "layouts.json")
        binaries.append(
            {
                "watchapp": files["app"],
                "worker_bin": files.get("worker"),
                "resources": files["resources"],
                "layouts": layouts if os.path.exists(layouts) else None,
                "sdk_version": dict(zip(("major", "minor"), pinfo["sdk_version"])),
                "subfolder": pinfo["bundle_dir"],
            }
        )

    project_dir = project["project_dir"]
    mkbundle.make_watchapp_bundle(
        timestamp=timestamp,
        appinfo=os.path.join(project["build_dir"], "appinfo.json"),
        binaries=binaries,
        js=manifest["js"],
        outfile=args.output,
        named_files={
            p: os.path.join(project_dir, p)
            for p in pebble_project.bundled_file_paths(project["info"])
        },
    )


def package(args):
    """The dist.zip a Pebble package is published with."""
    from pebble_package import LibraryPackage

    project = _load_project(args.project)
    project_dir = project["project_dir"]
    build_dir = project["build_dir"]
    name = project["name"]

    includes = {}
    include_root = os.path.join(project_dir, "include")
    for header in args.headers:
        header = os.path.abspath(header)
        base = include_root if header.startswith(include_root + os.sep) else project_dir
        includes[os.path.join(name, os.path.relpath(header, base))] = header
    includes[os.path.join(name, "message_keys.auto.h")] = os.path.join(
        build_dir, "include", "message_keys.auto.h"
    )

    scope, _, base_name = name.rpartition("/")
    binaries = {}
    resources = {}
    for platform in project["platforms"]:
        includes[os.path.join(name, platform, "src", "resource_ids.auto.h")] = (
            os.path.join(build_dir, platform, "src", "resource_ids.auto.h")
        )
        archive = os.path.join(platform, scope, f"lib{base_name}.a")
        binaries[archive] = os.path.join(build_dir, archive)
        with open(os.path.join(build_dir, platform, "package_resources.json")) as f:
            resources.update(dict(json.load(f)))

    js_root = os.path.join(project_dir, "src", "js")
    js_files = {}
    for source in args.js:
        source = os.path.abspath(source)
        base = js_root if source.startswith(js_root + os.sep) else project_dir
        js_files[os.path.relpath(source, base)] = source

    if os.path.exists(args.output):
        os.remove(args.output)
    package = LibraryPackage(args.output)
    package.add_files(
        includes=includes, binaries=binaries, resources=resources, js=js_files
    )
    package.pack()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("configure")
    p.add_argument("--sdk", required=True)
    p.add_argument("--project-dir", required=True)
    p.add_argument("--build-dir", required=True)
    p.add_argument("--platforms", type=lambda s: [x for x in s.split(";") if x])
    p.add_argument("--timestamp", type=int)
    p.set_defaults(func=configure)

    p = sub.add_parser("resources")
    p.add_argument("--project", required=True)
    p.add_argument("--platform", required=True)
    p.set_defaults(func=resources)

    p = sub.add_parser("binary")
    p.add_argument("--project", required=True)
    p.add_argument("--platform", required=True)
    p.add_argument("--kind", choices=("app", "worker"), required=True)
    p.add_argument("--elf", required=True)
    p.add_argument("--resources")
    p.add_argument("--output", required=True)
    p.add_argument("--timestamp", required=True)
    p.add_argument("--has-js", action="store_true")
    p.add_argument("--has-worker", action="store_true")
    p.add_argument("--sources-list")
    p.add_argument("--objcopy", required=True)
    p.add_argument("--toolchain-bin", required=True)
    p.set_defaults(func=binary)

    p = sub.add_parser("js")
    p.add_argument("--project", required=True)
    p.add_argument("--entry")
    p.add_argument("--output")
    p.add_argument("--node", default="node")
    p.add_argument("--node-modules")
    p.add_argument("sources", nargs="*")
    p.set_defaults(func=js)

    p = sub.add_parser("bundle")
    p.add_argument("--project", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--timestamp", required=True)
    p.add_argument("--output", required=True)
    p.set_defaults(func=bundle)

    p = sub.add_parser("package")
    p.add_argument("--project", required=True)
    p.add_argument("--output", required=True)
    p.add_argument("--headers", nargs="*", default=[])
    p.add_argument("--js", nargs="*", default=[])
    p.set_defaults(func=package)

    args = parser.parse_args()
    try:
        args.func(args)
    except (ProjectError, RuntimeError) as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
