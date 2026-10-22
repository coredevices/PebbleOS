# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The resources a firmware builds, read from the YAML tree.

A *fragment* is a directory holding a ``resources.yaml`` that declares
resources built from the files beside it. A *platform file* under
``resources/platforms/`` lists the fragments one firmware includes. This
module resolves the two into the flat list of entries the generators in
``tools/resources/`` consume.
"""

import os

import yaml
from pebble_sdk_platform import pebble_platforms

FRAGMENT_FILE = "resources.yaml"

PLATFORMS = ("emery", "flint", "gabbro")
DISPLAY_TAGS = ("bw", "color", "rect", "round")
FILE_KEYS = PLATFORMS + DISPLAY_TAGS

ENTRY_KEYS = {
    "name",
    "type",
    "file",
    "targetPlatforms",
    "builtin",
    "config",
    "aliases",
    # Font options, read by the font generator.
    "characterRegex",
    "characterList",
    "compatibility",
    "extended",
    "trackingAdjust",
    "pixelHeight",
    "compress",
}
FRAGMENT_KEYS = {"media", "timeline", "files"}
PLATFORM_FILE_KEYS = {"include", "overrides"}

# Files that may sit in a fragment without an entry naming them.
UNDECLARED_FILE_MARKERS = ("LICENSE", "LICENCE", "OFL", "README")
UNDECLARED_FILE_SUFFIXES = (".in",)


class ResourceTreeError(Exception):
    pass


class ResourceSet:
    """What one firmware builds: the media entries in id order, the
    timeline and file-store tables, and every YAML file they came from."""

    def __init__(self):
        self.media = []
        self.timeline = []
        self.files = []
        self.sources = []


class Fragment:
    def __init__(self, path):
        self.dir = os.path.dirname(path)
        self.path = path
        with open(path, encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
        if not isinstance(data, dict):
            raise ResourceTreeError(f"{path}: expected a mapping")
        unknown = set(data) - FRAGMENT_KEYS
        if unknown:
            raise ResourceTreeError(f"{path}: unknown keys {sorted(unknown)}")
        self.media = data.get("media") or []
        self.timeline = data.get("timeline")
        self.files = data.get("files")
        for entry in self.media:
            self._check_entry(entry)

    def _check_entry(self, entry):
        if not isinstance(entry, dict) or "name" not in entry:
            raise ResourceTreeError(f"{self.path}: every media entry needs a name")
        where = f"{self.path}: {entry['name']}"
        unknown = set(entry) - ENTRY_KEYS
        if unknown:
            raise ResourceTreeError(f"{where}: unknown keys {sorted(unknown)}")
        for key in ("type", "file"):
            if key not in entry:
                raise ResourceTreeError(f"{where}: missing {key}")
        spec = entry["file"]
        if isinstance(spec, dict):
            bad = [k for k in spec if k not in FILE_KEYS]
            if bad:
                raise ResourceTreeError(
                    f"{where}: file keys must be a platform or display tag, not {bad}"
                )
        elif not isinstance(spec, str):
            raise ResourceTreeError(f"{where}: file must be a path or a mapping")
        for key in ("targetPlatforms", "aliases"):
            if key in entry and not isinstance(entry[key], list):
                raise ResourceTreeError(f"{where}: {key} must be a list")
        unknown = set(entry.get("targetPlatforms", ())) - set(PLATFORMS)
        if unknown:
            raise ResourceTreeError(f"{where}: unknown platforms {sorted(unknown)}")
        builtin = entry.get("builtin", False)
        if not isinstance(builtin, (bool, list)):
            raise ResourceTreeError(
                f"{where}: builtin must be a bool or a platform list"
            )

    def resolve(self, path):
        return os.path.normpath(os.path.join(self.dir, path))

    def file_for(self, entry, platform):
        """The path an entry builds from on a platform: a platform key
        wins over a display tag, and two matching tags are an error."""
        spec = entry["file"]
        if not isinstance(spec, dict):
            return self.resolve(spec)
        if platform in spec:
            return self.resolve(spec[platform])
        tags = pebble_platforms[platform]["TAGS"]
        matches = [k for k in spec if k in tags]
        if len(matches) != 1:
            what = "no file" if not matches else f"ambiguous keys {matches}"
            raise ResourceTreeError(
                f"{self.path}: {entry['name']}: {what} for {platform}"
            )
        return self.resolve(spec[matches[0]])

    def referenced_files(self):
        """Every path any entry names, on any platform."""
        paths = set()
        for entry in self.media:
            spec = entry["file"]
            values = spec.values() if isinstance(spec, dict) else [spec]
            paths.update(self.resolve(v) for v in values)
            if "characterList" in entry:
                paths.add(self.resolve(entry["characterList"]))
        return paths

    def selected(self, entry, platform, config):
        if "targetPlatforms" in entry and platform not in entry["targetPlatforms"]:
            return False
        return "config" not in entry or entry["config"] in config

    def expand(self, entry, platform):
        """The entry as the generators take it, for one platform."""
        out = {k: v for k, v in entry.items() if k not in ("config", "builtin")}
        out["file"] = self.file_for(entry, platform)
        builtin = entry.get("builtin", False)
        if isinstance(builtin, list):
            builtin = platform in builtin
        if builtin:
            out["builtin"] = True
        if "characterList" in entry:
            out["characterList"] = self.resolve(entry["characterList"])
        return out


_fragments = {}


def load_fragment(directory):
    path = os.path.join(os.path.abspath(directory), FRAGMENT_FILE)
    if path not in _fragments:
        if not os.path.isfile(path):
            raise ResourceTreeError(
                f"{directory}: not a resource fragment ({FRAGMENT_FILE} missing)"
            )
        _fragments[path] = Fragment(path)
    return _fragments[path]


def _parse_include(item, resources_dir, platform_file):
    """An include is a fragment path, or a mapping with the path under
    ``path`` and the names to take from it under ``only``."""
    only = None
    if isinstance(item, dict):
        unknown = set(item) - {"path", "only"}
        if unknown or "path" not in item:
            raise ResourceTreeError(f"{platform_file}: bad include {item!r}")
        only = item.get("only")
        item = item["path"]
    return load_fragment(os.path.join(resources_dir, item)), only


def load(platform_file, extra_fragments, platform, config=frozenset()):
    """Resolve a platform file and the fragments registered from the build
    into a ResourceSet for ``platform``."""
    if platform not in PLATFORMS:
        raise ResourceTreeError(f"unknown platform {platform}")
    resources_dir = os.path.dirname(os.path.dirname(os.path.abspath(platform_file)))

    with open(platform_file, encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    unknown = set(data) - PLATFORM_FILE_KEYS
    if unknown:
        raise ResourceTreeError(f"{platform_file}: unknown keys {sorted(unknown)}")
    overrides = data.get("overrides") or {}
    unknown = set(overrides) - ENTRY_KEYS
    if unknown:
        raise ResourceTreeError(f"{platform_file}: cannot override {sorted(unknown)}")

    result = ResourceSet()
    result.sources.append(os.path.abspath(platform_file))

    includes = [
        _parse_include(item, resources_dir, platform_file)
        for item in data.get("include") or []
    ]
    includes.extend((load_fragment(d), None) for d in extra_fragments)

    owners = {}
    for fragment, only in includes:
        result.sources.append(fragment.path)
        names = {e["name"] for e in fragment.media}
        if only is not None:
            missing = set(only) - names
            if missing:
                raise ResourceTreeError(
                    f"{platform_file}: {fragment.path} has no {sorted(missing)}"
                )
        for entry in fragment.media:
            if only is not None and entry["name"] not in only:
                continue
            if not fragment.selected(entry, platform, config):
                continue
            name = entry["name"]
            if name in owners:
                raise ResourceTreeError(
                    f"{name} is declared by both {owners[name]} and {fragment.path}"
                )
            owners[name] = fragment.path
            expanded = fragment.expand(entry, platform)
            expanded.update(overrides)
            result.media.append(expanded)
        for key in ("timeline", "files"):
            table = getattr(fragment, key)
            if table is None:
                continue
            if getattr(result, key):
                raise ResourceTreeError(f"{fragment.path}: {key} is already defined")
            setattr(result, key, table)

    check_tree(resources_dir, extra_fragments)
    return result


def _undeclared_allowed(name):
    return (
        name == FRAGMENT_FILE
        or any(marker in name for marker in UNDECLARED_FILE_MARKERS)
        or name.endswith(UNDECLARED_FILE_SUFFIXES)
    )


def check_tree(resources_dir, extra_fragments=()):
    """Every file an entry names exists, and every file in a fragment is
    named by an entry, so the tree cannot grow dangling references or
    orphans. All fragments are checked, included or not, because a
    fragment may name files beside another one."""
    directories = []
    for root, dirs, files in os.walk(resources_dir):
        dirs.sort()
        if FRAGMENT_FILE in files:
            directories.append(root)
    directories.extend(os.path.abspath(d) for d in extra_fragments)
    fragments = [load_fragment(d) for d in directories]

    referenced = set()
    for fragment in fragments:
        for path in sorted(fragment.referenced_files()):
            if not os.path.exists(path):
                raise ResourceTreeError(f"{fragment.path}: {path} does not exist")
            referenced.add(path)

    orphans = []
    for fragment in fragments:
        for root, dirs, files in os.walk(fragment.dir):
            if root != fragment.dir and FRAGMENT_FILE in files:
                dirs[:] = []
                continue
            if any(root == r or root.startswith(r + os.sep) for r in referenced):
                dirs[:] = []
                continue
            orphans.extend(
                os.path.join(root, f)
                for f in sorted(files)
                if os.path.join(root, f) not in referenced
                and not _undeclared_allowed(f)
            )
    if orphans:
        raise ResourceTreeError(
            "files no resource entry names:\n  " + "\n  ".join(orphans)
        )
