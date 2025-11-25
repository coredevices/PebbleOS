#!/usr/bin/tclsh
# SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

set maxLen 120

foreach f [getSourceFileNames] {
    set lineNumber 1
    foreach line [getAllLines $f] {
        if {[string length $line] > $maxLen} {
            report $f $lineNumber "line is longer than ${maxLen} characters"
        }
        incr lineNumber
    }
}
