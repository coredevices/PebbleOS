# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import validate

BINDING = "properties:\n  compatible:\n    enum:\n      - vendor,a\n"
PATCH = """Subject: [PATCH] x: vendor,a: Add vendor,b

--- a/x/vendor,a.yaml
+++ b/x/vendor,a.yaml
@@ -2,3 +2,4 @@
   compatible:
     enum:
       - vendor,a
+      - vendor,b
"""


INDEX = "0001-b.patch:\n  type: temporary\n  status: not-submitted\n"


def setup(tmp_path, binding, index=INDEX):
    tree = tmp_path / "upstream"
    (tree / "x").mkdir(parents=True)
    (tree / "x" / "vendor,a.yaml").write_text(binding)
    patches_dir = tmp_path / "patches_dir"
    patches_dir.mkdir()
    (patches_dir / "0001-b.patch").write_text(PATCH)
    (patches_dir / "patches.yaml").write_text(index)
    return tree, patches_dir


def test_applies(tmp_path):
    tree, patches_dir = setup(tmp_path, BINDING)
    assert validate.apply_patches(tree, patches_dir) == [Path("x/vendor,a.yaml")]
    assert "vendor,b" in (tree / "x" / "vendor,a.yaml").read_text()


def test_taken_upstream(tmp_path):
    tree, patches_dir = setup(tmp_path, BINDING + "      - vendor,b\n")
    with pytest.raises(SystemExit, match="drop it"):
        validate.apply_patches(tree, patches_dir)


def test_no_longer_applies(tmp_path):
    tree, patches_dir = setup(tmp_path, "properties: {}\n")
    with pytest.raises(SystemExit, match="no longer applies"):
        validate.apply_patches(tree, patches_dir)


def test_ours_apply(tmp_path):
    upstream = (
        validate.Path(__file__).resolve().parents[3] / "third_party" / "devicetree"
    )
    bindings = upstream / "devicetree-rebasing" / "Bindings"
    if not (bindings / "vendor-prefixes.yaml").exists():
        pytest.skip("needs the devicetree-rebasing submodule")
    tree = tmp_path / "upstream"
    validate.shutil.copytree(bindings, tree, symlinks=True)
    ours = (
        validate.Path(__file__).resolve().parents[3]
        / "third_party"
        / "devicetree"
        / "patches"
    )
    validate.apply_patches(tree, ours)


E = "0001-b.patch:\n"


@pytest.mark.parametrize(
    ("index", "error"),
    [
        ("", "not in patches.yaml"),
        (INDEX + "0009-c.patch:\n  type: permanent\n  reason: r\n", "no such patch"),
        (E + "  type: maybe\n", "type is one of"),
        (E + "  type: temporary\n", "status is one of"),
        (E + "  type: temporary\n  status: submitted\n", "links to its submission"),
        (
            E + "  type: temporary\n  status: not-submitted\n  link: https://x\n",
            "has no link",
        ),
        (E + "  type: permanent\n", "gives its reason"),
        (E + "  type: permanent\n  reason: r\n  status: submitted\n", "no status"),
        (INDEX + "  owner: me\n", "no owner"),
    ],
)
def test_index_errors(tmp_path, index, error):
    tree, patches_dir = setup(tmp_path, BINDING, index)
    with pytest.raises(SystemExit, match=error):
        validate.apply_patches(tree, patches_dir)


@pytest.mark.parametrize("status", ["submitted", "applied", "merged"])
def test_index_submitted(tmp_path, status):
    index = (
        E
        + f"  type: temporary\n  status: {status}\n  link: https://lore.kernel.org/x\n"
    )
    tree, patches_dir = setup(tmp_path, BINDING, index)
    assert validate.apply_patches(tree, patches_dir)


PATCH2 = """Subject: [PATCH] x: vendor,a: Add vendor,c

--- a/x/vendor,a.yaml
+++ b/x/vendor,a.yaml
@@ -3,3 +3,4 @@
     enum:
       - vendor,a
       - vendor,b
+      - vendor,c
"""


def test_stacked(tmp_path):
    index = INDEX + "0002-c.patch:\n  type: temporary\n  status: not-submitted\n"
    tree, patches_dir = setup(tmp_path, BINDING, index)
    (patches_dir / "0002-c.patch").write_text(PATCH2)
    assert validate.apply_patches(tree, patches_dir) == [Path("x/vendor,a.yaml")]
    assert "vendor,c" in (tree / "x" / "vendor,a.yaml").read_text()


def test_stacked_first_taken(tmp_path):
    index = INDEX + "0002-c.patch:\n  type: temporary\n  status: not-submitted\n"
    tree, patches_dir = setup(tmp_path, BINDING + "      - vendor,b\n", index)
    (patches_dir / "0002-c.patch").write_text(PATCH2)
    with pytest.raises(SystemExit, match="0001-b.patch now: drop it"):
        validate.apply_patches(tree, patches_dir)


def test_patches_nothing(tmp_path):
    tree, patches_dir = setup(tmp_path, BINDING)
    (patches_dir / "0001-b.patch").write_text("Subject: [PATCH] x: nothing\n")
    with pytest.raises(SystemExit, match="patches nothing"):
        validate.apply_patches(tree, patches_dir)


def test_no_message(tmp_path):
    tree, patches_dir = setup(tmp_path, BINDING)
    patch = patches_dir / "0001-b.patch"
    patch.write_text(PATCH.replace("Subject: [PATCH] x: vendor,a: Add vendor,b\n", ""))
    with pytest.raises(SystemExit, match="has no message"):
        validate.apply_patches(tree, patches_dir)
