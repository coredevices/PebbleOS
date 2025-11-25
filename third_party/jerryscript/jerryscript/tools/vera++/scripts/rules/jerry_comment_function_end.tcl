#!/usr/bin/tclsh
# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

foreach fileName [getSourceFileNames] {
    set funcStart 0
    set funcName ""
    set lineNumber 1
    foreach line [getAllLines $fileName] {
        if {[regexp {^((static |const )*\w+ )*\w+ \(.*[,\)]} $line]} {
            set type {}
            set modifier {}
            if {$funcStart == 0} {
                regexp {^((static |const )*\w+ )*(\w+) \(} $line matched type modifier funcName
            }
        }

        if {[regexp {^\{$} $line]} {
            set funcStart 1
        }

        if {$funcStart == 1} {
            if {[regexp {^\}$} $line] && [string length $funcName] != 0} {
                report $fileName $lineNumber "missing comment at the end of function: /* $funcName */"
                set funcStart 0
            } elseif {[regexp {^\} /\*\s*\w+\s*\*/$} $line] && [string length $funcName] != 0} {
                set comment {}
                regexp {^\} /\*\s*(\w+)\s*\*/$} $line -> comment
                if {$comment != $funcName} {
                    report $fileName $lineNumber "comment missmatch. (Current: $comment, Expected: $funcName) "
                }
                set funcStart 0
            } elseif {[regexp {^\}.*;?$} $line]} {
                set funcStart 0
            }
        }

        incr lineNumber
    }
}
