#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

"""
Print out PATH after stripping out the directories inserted by the XCode app.
This is called when we issue a waf build from within XCode because the paths inserted by
XCode interfere with our tintin build. 

When you issue a build from within XCode, it calls into the waf-xcode.sh script. That script
calls this script in order to strip out the XCode directories from PATH before it transfers
control to waf. 
"""

import os

# Strip out the XCode inserted paths in PATH
path = os.getenv('PATH')
elements = path.split(':')
new_elements = []
for x in elements:
    if not 'Xcode.app' in x:
        new_elements.append(x)
        
print ':'.join(new_elements)
