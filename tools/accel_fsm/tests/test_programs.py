# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The draft gesture programs on synthetic motion, in watch axes.

The wrist is modelled as a rotation around the forearm (X): at angle phi,
gravity reads y = -sin(phi), z = -cos(phi); phi > 0 tilts the face towards
the user, phi < 0 turns it away.
"""

import math
import pathlib
import random

import pytest

from .. import fsm

PROGRAMS = pathlib.Path(__file__).parent.parent / "programs"
RATE = 52


def _program(name, odr):
    return fsm.assemble((PROGRAMS / name).read_text(), odr=odr)


def _rotation(path, rate=RATE):
    """Samples for a list of (target angle in degrees, duration in s) moves."""
    rnd = random.Random(1)
    phi = path[0][0]
    out = []
    for target, duration in path[1:]:
        n = max(1, round(duration * rate))
        start = phi
        for i in range(1, n + 1):
            # Smooth (cosine) move from start to target
            a = start + (target - start) * (1 - math.cos(math.pi * i / n)) / 2
            r = math.radians(a)
            out.append(
                (
                    rnd.gauss(0, 0.02),
                    -math.sin(r) + rnd.gauss(0, 0.02),
                    -math.cos(r) + rnd.gauss(0, 0.02),
                )
            )
        phi = target
    return out


def _events(name, samples, odr):
    step = RATE // odr
    sim = fsm.Simulator(_program(name, odr))
    return len(sim.run(samples[::step]))


HOLD = 0.6

GESTURES = {
    # name: (samples, flick_out events, flick_in events)
    "flick out, flat": (
        _rotation([(0, 0), (0, HOLD), (-70, 0.12), (0, 0.15), (0, HOLD)]),
        1,
        0,
    ),
    "flick out, tilted": (
        _rotation([(30, 0), (30, HOLD), (-60, 0.12), (30, 0.15), (30, HOLD)]),
        1,
        0,
    ),
    "flick in, flat": (
        _rotation([(0, 0), (0, HOLD), (70, 0.12), (0, 0.15), (0, HOLD)]),
        0,
        1,
    ),
    "slow turn away and back": (
        _rotation([(0, 0), (0, HOLD), (-70, 1.0), (0, 1.0), (0, HOLD)]),
        0,
        0,
    ),
    "turn away and stay": (
        _rotation([(0, 0), (0, HOLD), (-70, 0.12), (-70, 2.0)]),
        0,
        0,
    ),
    "raise to look": (_rotation([(-80, 0), (-80, HOLD), (30, 0.4), (30, 2.0)]), 0, 0),
    "still": (_rotation([(20, 0), (20, 5.0)]), 0, 0),
}


def _knock():
    """A bump on +Y lasting about 60 ms, two samples at 26 Hz."""
    samples = _rotation([(0, 0), (0, 2.0)])
    i = len(samples) // 2
    for j in range(i, i + 3):
        samples[j] = (0.0, 1.5, -1.0)
    return samples


def _arm_swing():
    """Walking with the arm down: the forearm (X) points down, face sideways."""
    out = []
    for i in range(10 * RATE):
        swing = 0.6 * math.sin(2 * math.pi * 0.9 * i / RATE)
        out.append((-0.95, 0.3 * math.sin(swing) + 0.4 * math.cos(swing), -0.2))
    return out


GESTURES["knock"] = (_knock(), 0, 0)
GESTURES["arm swing"] = (_arm_swing(), 0, 0)


@pytest.mark.parametrize("odr", [26, 52])
@pytest.mark.parametrize("gesture", GESTURES)
def test_flick(gesture, odr):
    samples, out_events, in_events = GESTURES[gesture]
    assert _events("flick_out.fsm", samples, odr) == out_events
    assert _events("flick_in.fsm", samples, odr) == in_events


def test_flick_in_needs_flat_start():
    # From a face-towards-the-user posture, Y already sits past neutral on -Y
    samples = _rotation([(30, 0), (30, HOLD), (110, 0.12), (30, 0.15), (30, HOLD)])
    assert _events("flick_in.fsm", samples, 26) == 0
