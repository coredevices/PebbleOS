#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Check our bindings and devicetrees against the upstream bindings and our own.

Bindings are checked as upstream checks its own: against the meta-schema, and their examples
against the processed schema. Trees are checked from compiled blobs, or compiled from every board
tree. Needs dtschema, dtc and a C preprocessor. The tools
report problems of the upstream bindings too and exit 0 on findings, so only lines about our
files count. The upstream bindings and headers come from the devicetree-rebasing submodule.
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dtmap

# Those upstream gives dtc for binding examples
EXAMPLE_DTC_FLAGS = (
    "-Wno-avoid_unnecessary_addr_size",
    "-Wno-graph_child_address",
    "-Wno-interrupt_provider",
    "-Wno-unique_unit_address",
    "-Wunique_unit_address_if_enabled",
)


def merged_vendor_prefixes(upstream, extra, outdir):
    """The upstream registry plus the prefixes it lacks, shadowing it (the first $id wins)."""
    lines = (upstream / "vendor-prefixes.yaml").read_text().splitlines(keepends=True)
    entries = []
    for line in extra.read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            prefix, description = line.split("\t", 1)
            if any(f'"^{prefix},.*"' in existing for existing in lines):
                sys.exit(f"validate: upstream has '{prefix}' now: drop it from {extra}")
            entries += [f'  "^{prefix},.*":\n', f"    description: {description}\n"]
    anchor = next(
        i for i, line in enumerate(lines) if line.startswith("patternProperties:")
    )
    lines[anchor + 1 : anchor + 1] = entries
    out = outdir / "vendor-prefixes.yaml"
    out.write_text("".join(lines))
    return out


def our_compatibles(ours):
    found = set()
    for path in ours.rglob("*.yaml"):
        found |= set(re.findall(r"(?:const:|-)\s+([a-z0-9-]+,[a-z0-9.,+-]+)\s*$", path.read_text(), re.MULTILINE))
    return found


def upstream_copy(upstream, ours, tmp):
    """Upstream, without the trivial devices we have a binding for, as upstream does when one
    gains properties."""
    tree = tmp / "upstream"
    shutil.copytree(upstream, tree, symlinks=True)
    trivial = tree / "trivial-devices.yaml"
    compatibles = our_compatibles(ours)
    lines = trivial.read_text().splitlines(keepends=True)
    kept = [
        line
        for line in lines
        if not (line.strip().startswith("- ") and line.strip()[2:].strip() in compatibles)
    ]
    trivial.write_text("".join(kept))
    return tree


def run(cmd, **kwargs):
    return subprocess.run(cmd, capture_output=True, text=True, check=False, **kwargs)


def ours_only(output, names):
    return [line for line in output.splitlines() if any(n in line for n in names)]


def make_schema(args, ours, prefixes, tmp):
    schema = tmp / "schema.json"
    with open(schema, "w") as out:
        result = subprocess.run(
            [args.dt_mk_schema, "-j", str(prefixes), str(ours), str(args.upstream)],
            stdout=out,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
    errors = ours_only(result.stderr, [str(ours)])
    if result.returncode != 0 or errors:
        sys.exit("validate: dt-mk-schema failed:\n" + "\n".join(errors))
    return schema


def check_bindings(args, ours, files, schema, tmp):
    """Check bindings with the meta-schema, from a copy of upstream they are merged into, as
    references only resolve within one tree, then their examples against the schema."""
    tree = tmp / "bindings"
    shutil.copytree(args.upstream, tree, symlinks=True)
    shutil.copytree(ours, tree, dirs_exist_ok=True)
    rels = [Path(path).resolve().relative_to(ours) for path in files]
    copies = {str(tree / rel): str(rel) for rel in rels}

    def report(lines, names):
        for line in lines:
            for name, rel in names.items():
                line = line.replace(name, rel)
            print(line)

    findings = ours_only(
        run([args.dt_doc_validate, "-u", str(tree), *copies]).stderr, list(copies)
    )
    report(findings, copies)
    failed = bool(findings)

    includes = [args.root / "include", *args.include]
    dtbs = {}
    for rel in rels:
        example = run([args.dt_extract_example, str(ours / rel)])
        base = tmp / "examples" / rel.with_suffix("")
        base.parent.mkdir(parents=True, exist_ok=True)
        pre = run(
            [
                args.cc,
                "-E",
                "-nostdinc",
                "-undef",
                "-D__DTS__",
                "-x",
                "assembler-with-cpp",
                "-P",
                *[f for d in includes for f in ("-I", str(d))],
                "-",
            ],
            input=example.stdout,
        )
        dtb = base.with_suffix(".dtb")
        dtc = run(
            ["dtc", "-@", *EXAMPLE_DTC_FLAGS, "-I", "dts", "-O", "dtb", "-o", str(dtb)],
            input=pre.stdout,
        )
        errors = [r.stderr.strip() for r in (example, pre, dtc) if r.stderr.strip()]
        if example.returncode or pre.returncode or dtc.returncode or errors:
            failed = True
            print(f"{rel}: examples: " + "\n".join(errors))
            continue
        dtbs[str(dtb)] = f"{rel} (examples)"

    if dtbs:
        result = run([args.dt_validate, "-m", "-s", str(schema), *dtbs])
        findings = ours_only(result.stdout + result.stderr, list(dtbs))
        report(findings, dtbs)
        failed |= result.returncode != 0 or bool(findings)
    return failed


def check_dtbs(args, dtbs, schema):
    if not dtbs:
        return False
    result = run([args.dt_validate, "-m", "-s", str(schema), *map(str, dtbs)])
    output = result.stdout + result.stderr
    findings = ours_only(output, list(map(str, dtbs)))
    if result.returncode != 0 or findings:
        print("\n".join(findings) or output)
        return True
    return False


def compile_trees(args, tmp):
    """Every board tree, compiled as the build does."""
    root = args.root
    dtbs = []
    for dts in sorted((root / "boards").glob("*/*.dts")):
        outdir = tmp / "trees" / dts.stem
        try:
            dtb, _ = dtmap.compile_dts(
                dts, args.cc, [root / "boards", root / "soc", root / "include"], outdir
            )
        except dtmap.DtError as e:
            sys.exit(f"validate: {e}")
        named = outdir / f"{dts.stem}.dtb"
        Path(dtb).rename(named)
        dtbs.append(named)
    return dtbs


def main():
    root = Path(__file__).resolve().parents[2]
    upstream = root / "third_party" / "devicetree" / "devicetree-rebasing"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream-bindings", type=Path, default=upstream / "Bindings")
    parser.add_argument(
        "--include",
        type=Path,
        action="append",
        default=[upstream / "include"],
        help="more include directories for binding examples",
    )
    parser.add_argument("--cc", default="cc", help="C compiler, to preprocess trees")
    parser.add_argument("--dt-mk-schema", default="dt-mk-schema")
    parser.add_argument("--dt-validate", default="dt-validate")
    parser.add_argument("--dt-doc-validate", default="dt-doc-validate")
    parser.add_argument("--dt-extract-example", default="dt-extract-example")
    parser.add_argument(
        "--bindings", nargs="+", default=[], metavar="YAML", help="our bindings to check"
    )
    parser.add_argument(
        "--all-trees", action="store_true", help="compile and check every board tree"
    )
    parser.add_argument("dtb", nargs="*", help="compiled trees to check")
    args = parser.parse_args()
    args.root = root
    args.upstream_bindings = args.upstream_bindings.resolve()
    if not (args.upstream_bindings / "vendor-prefixes.yaml").exists():
        sys.exit(
            f"validate: no bindings in {args.upstream_bindings}: run "
            "git submodule update --init --depth 1 third_party/devicetree/devicetree-rebasing"
        )
    if not (args.bindings or args.all_trees or args.dtb):
        parser.error("nothing to check")

    ours = args.root / "dts" / "bindings"
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        prefixes = merged_vendor_prefixes(
            args.upstream_bindings, ours / "vendor-prefixes.txt", tmp
        )
        args.upstream = upstream_copy(args.upstream_bindings, ours, tmp)
        schema = make_schema(args, ours, prefixes, tmp)
        failed = check_bindings(args, ours, args.bindings, schema, tmp) if args.bindings else False
        dtbs = [*args.dtb, *(compile_trees(args, tmp) if args.all_trees else [])]
        failed |= check_dtbs(args, dtbs, schema)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
