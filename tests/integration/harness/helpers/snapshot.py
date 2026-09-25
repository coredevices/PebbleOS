# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Screenshot comparison against golden images kept in the repository."""

import os
from dataclasses import dataclass

import numpy as np
from PIL import Image

from harness.errors import HarnessError


@dataclass(frozen=True)
class Region:
    """A rectangle of the screen, in pixels."""

    x: int
    y: int
    w: int
    h: int


@dataclass
class Comparison:
    matched: bool
    diff_pixels: int
    total_pixels: int
    actual: str = None
    expected: str = None
    diff: str = None

    def __str__(self):
        return (
            f"{self.diff_pixels} of {self.total_pixels} pixels differ\n"
            f"  expected: {self.expected}\n  actual:   {self.actual}\n  diff:     {self.diff}"
        )


def _to_array(image):
    return np.asarray(image.convert("RGB"), dtype=np.int16)


def compare(actual, expected, mask=(), tolerance=0):
    """A boolean per-pixel mismatch map; ``mask`` regions are ignored and
    channels within ``tolerance`` count as equal."""
    a, e = _to_array(actual), _to_array(expected)
    if a.shape != e.shape:
        raise HarnessError(
            f"size mismatch: actual {actual.size}, expected {expected.size}"
        )
    mismatch = (np.abs(a - e) > tolerance).any(axis=2)
    for region in mask:
        mismatch[region.y : region.y + region.h, region.x : region.x + region.w] = False
    return mismatch


def diff_image(actual, mismatch):
    """``actual`` faded, with the mismatching pixels in red."""
    faded = (_to_array(actual) * 0.3 + 255 * 0.7).astype(np.uint8)
    faded[mismatch] = (255, 0, 0)
    return Image.fromarray(faded, "RGB")


class Snapshot:
    """Compares screenshots to ``<golden_dir>/<board>/<module>/<name>.png``."""

    def __init__(self, golden_dir, board, module, results_dir, update=False):
        self.golden_dir = os.path.join(golden_dir, board, module)
        self.results_dir = results_dir
        self.update = update

    def golden_path(self, name):
        return os.path.join(self.golden_dir, f"{name}.png")

    def check(self, image, name, mask=(), tolerance=0, max_diff_pixels=0):
        golden = self.golden_path(name)
        os.makedirs(self.results_dir, exist_ok=True)
        actual_path = os.path.join(self.results_dir, f"{name}.actual.png")
        image.save(actual_path)

        if self.update:
            os.makedirs(self.golden_dir, exist_ok=True)
            image.save(golden)
            return Comparison(True, 0, image.width * image.height, actual_path, golden)

        if not os.path.isfile(golden):
            return Comparison(
                False, -1, image.width * image.height, actual_path, golden
            )

        with Image.open(golden) as expected:
            mismatch = compare(image, expected, mask, tolerance)
        diff_pixels = int(mismatch.sum())
        result = Comparison(
            diff_pixels <= max_diff_pixels,
            diff_pixels,
            mismatch.size,
            actual_path,
            golden,
        )
        if not result.matched:
            result.diff = os.path.join(self.results_dir, f"{name}.diff.png")
            diff_image(image, mismatch).save(result.diff)
        return result

    def assert_match(self, image, name, mask=(), tolerance=0, max_diff_pixels=0):
        """Fail the test unless ``image`` matches the golden ``name``."""
        __tracebackhide__ = True
        import pytest

        result = self.check(image, name, mask, tolerance, max_diff_pixels)
        if result.diff_pixels < 0:
            pytest.fail(
                f"no golden image {result.expected}; run with --update-golden to "
                f"create it from {result.actual}"
            )
        if not result.matched:
            pytest.fail(
                f"screenshot {name!r} does not match its golden image: {result}"
            )
