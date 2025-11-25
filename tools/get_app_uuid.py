#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import uuid, sys

if len(sys.argv) > 1:
    u = uuid.UUID(sys.argv[1])
else:
    u = uuid.uuid4()
uuid_array = '{{{:s}}}'.format(', '.join(['0x{:x}'.format(ord(b)) for b in u.get_bytes()]))

print "\tBytes: '{}'".format(uuid_array)
print "\tString: '{}'".format(str(u))
