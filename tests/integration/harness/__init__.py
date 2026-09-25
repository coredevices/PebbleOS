# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Integration test harness for PebbleOS.

A test drives a :class:`~harness.watch.Watch`: firmware running on a
:class:`~harness.targets.Target` (the emulator or a real device), reached
through one or more :class:`~harness.connections.Connection` backends.
:mod:`harness.plugin` wires them together from the pytest command line.
"""
