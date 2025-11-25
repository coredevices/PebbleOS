#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import math

def print_sin_table():
    print "{"
    for a in xrange(0, 0xffff + 1, 0xff):
        print str(int(round(math.sin(a*math.pi/0xffff/2.0) * 0xffff))) + ','
    print '}'

def print_atan_table():
    print '{'
    for a in xrange(0, 0xffff + 1, 0xff):
        b = float(a) / float(0xffff)
        print str(int(round(math.atan(b) * 0x8000 / math.pi))*8) + ','
    print '}'

print_sin_table()
print_atan_table()


