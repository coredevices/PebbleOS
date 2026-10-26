# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Instrument backends of bench.py.

A backend module has a one-line docstring, add_arguments(parser) for its own
options, and open(args), which returns an instrument with:

- charge(profile, log): charges with a bench.ChargeProfile until the
  charger terminates, logging to a fuel_gauge_log.LogWriter.
- discharge(profile, log): runs a bench.DischargeProfile down to its
  cutoff, logging the current as positive.
- close().
"""
