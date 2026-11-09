#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Check a compiled devicetree against the upstream bindings and our own.

Needs dtschema, and a checkout of the upstream bindings. The tools report problems of the upstream
bindings too and exit 0 on findings, so only lines about our bindings and blobs count.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


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


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream-bindings", type=Path, required=True)
    parser.add_argument("--dt-mk-schema", default="dt-mk-schema")
    parser.add_argument("--dt-validate", default="dt-validate")
    parser.add_argument("dtb", nargs="+")
    args = parser.parse_args()

    ours = root / "dts" / "bindings"
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        prefixes = merged_vendor_prefixes(
            args.upstream_bindings, ours / "vendor-prefixes.txt", tmp
        )
        schema = tmp / "schema.json"
        with open(schema, "w") as out:
            result = subprocess.run(
                [
                    args.dt_mk_schema,
                    "-j",
                    str(prefixes),
                    str(args.upstream_bindings),
                    str(ours),
                ],
                stdout=out,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
        errors = [line for line in result.stderr.splitlines() if str(ours) in line]
        if result.returncode != 0 or errors:
            sys.exit("validate: dt-mk-schema failed:\n" + "\n".join(errors))

        failed = False
        for dtb in args.dtb:
            result = subprocess.run(
                [args.dt_validate, "-m", "-s", str(schema), dtb],
                capture_output=True,
                text=True,
                check=False,
            )
            output = (result.stdout + result.stderr).splitlines()
            findings = [line for line in output if dtb in line]
            if result.returncode != 0 or findings:
                failed = True
                print("\n".join(findings or output))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
