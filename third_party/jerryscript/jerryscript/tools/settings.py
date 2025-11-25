#!/usr/bin/env python
# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged.
# SPDX-License-Identifier: Apache-2.0

from os import path

TOOLS_DIR = path.dirname(path.abspath(__file__))
PROJECT_DIR = path.normpath(path.join(TOOLS_DIR, '..'))
JERRY_TESTS_DIR = path.join(PROJECT_DIR, 'tests/jerry')
JERRY_TEST_SUITE_DIR = path.join(PROJECT_DIR, 'tests/jerry-test-suite')
JERRY_TEST_SUITE_MINIMAL_LIST = path.join(PROJECT_DIR, 'tests/jerry-test-suite/minimal-profile-list')

BUILD_SCRIPT = path.join(TOOLS_DIR, 'build.py')
CPPCHECK_SCRIPT = path.join(TOOLS_DIR, 'check-cppcheck.sh')
SIGNED_OFF_SCRIPT = path.join(TOOLS_DIR, 'check-signed-off.sh')
VERA_SCRIPT = path.join(TOOLS_DIR, 'check-vera.sh')
LICENSE_SCRIPT = path.join(TOOLS_DIR, 'check-license.py')
TEST_RUNNER_SCRIPT = path.join(TOOLS_DIR, 'runners/run-test-suite.sh')
UNITTEST_RUNNER_SCRIPT = path.join(TOOLS_DIR, 'runners/run-unittests.sh')
