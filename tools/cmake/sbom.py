#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""CycloneDX software bill of materials for the firmware image."""

import argparse
import datetime
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import uuid

import wafshim
import yaml
from wafshim import REPO_ROOT

wafshim.setup_path()

import gitinfo

METADATA_NAME = "sbom.yml"
SPEC_VERSION = "1.6"
SUPPLIER = "Core Devices LLC"
REPO_URL = "https://github.com/coredevices/PebbleOS"


class SbomError(Exception):
    pass


def git(*args, cwd=REPO_ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()


def submodules():
    """Submodule paths and the commits HEAD records for them."""
    gitlinks = {}
    for line in git("ls-files", "--stage").splitlines():
        mode, sha, _, path = line.split(maxsplit=3)
        if mode == "160000":
            gitlinks[path] = sha
    return gitlinks


def load_components():
    components = []
    for meta in git(
        "ls-files", "--cached", "--others", "--exclude-standard", f"*{METADATA_NAME}"
    ).splitlines():
        if os.path.basename(meta) != METADATA_NAME:
            continue
        with open(os.path.join(REPO_ROOT, meta)) as f:
            data = yaml.safe_load(f)
        base = os.path.dirname(meta)
        for entry in data["components"]:
            entry = dict(entry)
            entry["metadata"] = meta
            if "path" in entry:
                entry["paths"] = [entry.pop("path")]
            if "toolchain-path" in entry:
                entry["toolchain-paths"] = [entry.pop("toolchain-path")]
            entry["paths"] = [
                os.path.normpath(os.path.join(base, p)) for p in entry.get("paths", [])
            ]
            entry["nested"] = [
                os.path.join(entry["paths"][0], p) for p in entry.get("nested", [])
            ]
            components.append(entry)
    return components


def within(path, prefix):
    return path == prefix or path.startswith(prefix.rstrip("/") + "/")


def containing_submodule(path, gitlinks):
    for sub in gitlinks:
        if within(path, sub):
            return sub
    return None


def check_components(components, gitlinks):
    """Every submodule is described, and every description is current."""
    errors = []
    names = set()
    for comp in components:
        where = f"{comp['metadata']}: {comp.get('name', '?')}"
        for key in ("name", "supplier", "license"):
            if not comp.get(key):
                errors.append(f"{where}: missing '{key}'")
        if comp.get("name") in names:
            errors.append(f"{where}: duplicate component name")
        names.add(comp.get("name"))
        if not comp["paths"] and not comp.get("toolchain-paths"):
            errors.append(f"{where}: no 'path' or 'toolchain-path'")

        subs = {containing_submodule(p, gitlinks) for p in comp["paths"]}
        subs.discard(None)
        if len(subs) > 1:
            errors.append(f"{where}: paths span several submodules")
        for sub in subs:
            if comp.get("commit") != gitlinks[sub]:
                errors.append(
                    f"{where}: commit {comp.get('commit')} does not match "
                    f"{sub} at {gitlinks[sub]}; update its version and commit"
                )
        for path in comp["paths"]:
            if not subs and not os.path.exists(os.path.join(REPO_ROOT, path)):
                errors.append(f"{where}: {path} does not exist")

    for sub in gitlinks:
        if not any(within(p, sub) for c in components for p in c["paths"]):
            errors.append(f"submodule {sub} is not described by any {METADATA_NAME}")
    return errors


def build_inputs(ninja, build_dir, target):
    """Every file the target was built from, headers included."""

    def run(*args):
        # -n keeps ninja from compacting the logs the running build has open.
        cmd = [ninja, "-n", "-C", build_dir, *args]
        return subprocess.check_output(cmd, text=True).splitlines()

    inputs = [shlex.split(p)[0] for p in run("-t", "inputs", target)]
    objects = [p for p in inputs if p.endswith((".obj", ".o"))]
    files = {p for p in inputs if not p.endswith((".obj", ".o"))}
    for i in range(0, len(objects), 500):
        for line in run("-t", "deps", *objects[i : i + 500]):
            if line.startswith(" "):
                files.add(line.strip())
    return {os.path.realpath(os.path.join(build_dir, p)) for p in files}


def define_value(path, name):
    with open(path) as f:
        match = re.search(rf'#define\s+{name}\s+"([^"]+)"', f.read())
    if not match:
        raise SbomError(f"{name} not found in {path}")
    return match.group(1)


def resolve_version(comp, args):
    source = comp.get("version-from")
    if source == "compiler":
        return args.compiler_version
    if isinstance(source, dict):
        return define_value(
            os.path.join(args.toolchain_root, source["file"]), source["define"]
        )
    return comp.get("version") or comp.get("commit")


def match_components(files, components, args):
    repo = os.path.realpath(REPO_ROOT)
    build = os.path.realpath(args.build_dir)
    toolchain = os.path.realpath(args.toolchain_root)

    matchers = []
    for comp in components:
        for p in comp["paths"]:
            matchers.append((os.path.join(repo, p), comp))
        for p in comp.get("build-paths", []):
            matchers.append((os.path.join(build, p), comp))
        for p in comp.get("toolchain-paths", []):
            matchers.append((os.path.join(toolchain, p), comp))
    matchers.sort(key=lambda m: len(m[0]), reverse=True)

    nested = [os.path.join(repo, n) for c in components for n in c["nested"]]

    used = {}
    unknown = []
    for path in sorted(files):
        for prefix, comp in matchers:
            if within(path, prefix):
                if any(within(path, n) and not within(prefix, n) for n in nested):
                    unknown.append(path)
                else:
                    used[comp["name"]] = comp
                break
        else:
            if not (within(path, repo) or within(path, build)):
                unknown.append(path)
    if unknown:
        raise SbomError(
            "files with no component describing them:\n  " + "\n  ".join(unknown)
        )
    return [used[name] for name in sorted(used)]


def file_hashes(path):
    with open(path, "rb") as f:
        data = f.read()
    return [
        {"alg": "SHA-256", "content": hashlib.sha256(data).hexdigest()},
        {"alg": "SHA-512", "content": hashlib.sha512(data).hexdigest()},
    ]


def bom_ref(name):
    return re.sub(r"[^A-Za-z0-9.+-]", "-", name)


def cdx_component(comp, version):
    out = {
        "type": comp.get("type", "library"),
        "bom-ref": bom_ref(comp["name"]),
        "name": comp["name"],
    }
    if version:
        out["version"] = version
    out["supplier"] = {"name": comp["supplier"]}
    if comp.get("description"):
        out["description"] = comp["description"]
    out["licenses"] = [{"expression": comp["license"]}]
    for key in ("cpe", "purl"):
        if comp.get(key):
            out[key] = comp[key].replace("{version}", version or "")
    if comp.get("url"):
        ref_type = "vcs" if comp.get("commit") else "website"
        out["externalReferences"] = [{"type": ref_type, "url": comp["url"]}]
    if comp.get("commit"):
        out["properties"] = [{"name": "pebbleos:commit", "value": comp["commit"]}]
    return out


def cmd_generate(args):
    components = load_components()
    gitlinks = submodules()
    errors = check_components(components, gitlinks)
    if errors:
        raise SbomError("\n".join(errors))

    files = build_inputs(args.ninja, args.build_dir, args.target)
    used = match_components(files, components, args)

    for comp in used:
        sub = (
            containing_submodule(comp["paths"][0], gitlinks) if comp["paths"] else None
        )
        if (
            sub
            and git("rev-parse", "HEAD", cwd=os.path.join(REPO_ROOT, sub))
            != comp["commit"]
        ):
            raise SbomError(f"{sub} is not checked out at {comp['commit']}")

    revision = gitinfo.get_git_revision()
    version = revision["TAG"]
    timestamp = int(os.environ.get("SOURCE_DATE_EPOCH", revision["TIMESTAMP"]))
    product = {
        "type": "firmware",
        "bom-ref": "pebbleos",
        "name": "PebbleOS",
        "version": version,
        "supplier": {"name": SUPPLIER},
        "description": f"PebbleOS {args.variant} firmware for {args.board}",
        "hashes": file_hashes(args.image),
        "licenses": [{"expression": "Apache-2.0"}],
        "purl": f"pkg:github/coredevices/PebbleOS@{version}",
        "externalReferences": [{"type": "vcs", "url": REPO_URL}],
        "properties": [
            {"name": "pebbleos:board", "value": args.board},
            {"name": "pebbleos:variant", "value": args.variant},
            {"name": "pebbleos:image", "value": os.path.basename(args.image)},
        ],
    }

    cdx = [cdx_component(c, resolve_version(c, args)) for c in used]
    bom = {
        "bomFormat": "CycloneDX",
        "specVersion": SPEC_VERSION,
        "version": 1,
        "metadata": {
            "timestamp": datetime.datetime.fromtimestamp(
                timestamp, datetime.timezone.utc
            ).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "manufacturer": {"name": SUPPLIER},
            "component": product,
        },
        "components": cdx,
        "dependencies": [
            {"ref": "pebbleos", "dependsOn": [c["bom-ref"] for c in cdx]},
            *({"ref": c["bom-ref"], "dependsOn": []} for c in cdx),
        ],
    }
    digest = hashlib.sha256(json.dumps(bom, sort_keys=True).encode()).hexdigest()
    bom["serialNumber"] = f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_OID, digest)}"

    with open(args.output, "w") as f:
        json.dump(bom, f, indent=2)
        f.write("\n")
    print(f"Wrote {args.output} ({len(cdx)} components)")


def cmd_check(args):
    errors = check_components(load_components(), submodules())
    if errors:
        raise SbomError("\n".join(errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("generate")
    p.add_argument("--ninja", default="ninja")
    p.add_argument("--build-dir", required=True)
    p.add_argument("--target", required=True)
    p.add_argument("--image", required=True)
    p.add_argument("--board", required=True)
    p.add_argument("--variant", required=True)
    p.add_argument("--toolchain-root", required=True)
    p.add_argument("--compiler-version", required=True)
    p.add_argument("--output", required=True)
    p.set_defaults(func=cmd_generate)

    p = sub.add_parser("check", help="check the component metadata against the tree")
    p.set_defaults(func=cmd_check)

    args = parser.parse_args()
    try:
        args.func(args)
    except SbomError as e:
        sys.exit(f"SBOM: {e}")


if __name__ == "__main__":
    main()
