#!/usr/bin/tclsh
# SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

proc check_part_of_the_file {file line_num col_start col_end} {
    if {$col_start == $col_end} {
        return
    }

    set line [getLine $file $line_num]
    set line [string range $line $col_start $col_end]

    if {[regexp {[[:graph:]][[:blank:]]+\)} $line]} {
        report $file $line_num "there should be no blank characters before closing parentheses"
    }
}

foreach fileName [getSourceFileNames] {
    set checkLine 1
    set checkColStart 0
    set seenOmitToken false
    foreach token [getTokens $fileName 1 0 -1 -1 {}] {
        set lineNumber [lindex $token 1]
        set colNumber [lindex $token 2]
        set tokenType [lindex $token 3]

        if {$checkLine != $lineNumber} {
            if {!$seenOmitToken} {
                check_part_of_the_file $fileName $checkLine $checkColStart end
            }
            set checkColStart $colNumber
            set checkLine $lineNumber
        } elseif {$seenOmitToken} {
            set checkColStart $colNumber
        }

        if {$tokenType in {ccomment cppcomment stringlit}} {
            check_part_of_the_file $fileName $checkLine $checkColStart $colNumber
            set seenOmitToken true
        } else {
            set seenOmitToken false
        }
    }
}
