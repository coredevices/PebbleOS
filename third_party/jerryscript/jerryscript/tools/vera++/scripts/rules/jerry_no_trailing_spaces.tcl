#!/usr/bin/tclsh
# SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

foreach f [getSourceFileNames] {
    set lineNumber 1
    foreach line [getAllLines $f] {
        if {[regexp {[[:blank:]]$} $line]} {
            report $f $lineNumber "trailing space is not allowed"
        }
        incr lineNumber
    }
}
