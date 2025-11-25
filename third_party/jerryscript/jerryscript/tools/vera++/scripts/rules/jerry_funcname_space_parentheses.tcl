#!/usr/bin/tclsh
# SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

proc check_part_of_the_file {file line_num col_start col_end} {
    if {$col_start == $col_end} {
        return
    }
    set line [getLine $file $line_num]

    if {[regexp {^\s*#[ ]*define} $line]} {
        return
    }

    set line [string range $line $col_start $col_end]

    if {[regexp {([[:alnum:]][\s]{2,}\()|([[:alnum:]]\()} $line]} {
        report $file $line_num "there should be exactly one space before left parentheses"
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
