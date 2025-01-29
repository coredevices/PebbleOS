#!/usr/bin/env python
import re
import sys

# when cross-compiling JerryScript with Emscripten, we need to fix a few minor things
# to account for the various setups as some of the code assumes to be execute inside of node

def replace_ensured(s, old, new):
    result = s.replace(old, new)
    # make sure our search pattern `old` actually matched something
    # if we didn't change anything it means that we missed the Emscrupten outout (e.g. new version)
    assert result != s, "Emscripten output does not match expected output of 1.35.0"
    return result

# load file to be processed
with open(sys.argv[1], "r") as f:
    source = f.read()

# source = replace_ensured(source, "func = eval('_' + ident); // explicit lookup",
#                                  "// func = eval('_' + ident); // explicit lookup")
source = replace_ensured(source, "process['on']('uncaughtException',",
                                 "process['on']('uncaughtException-ignore',")

source = "(function(){\n%s\n})(this);" % source

with open(sys.argv[1], "w") as f:
    f.write(source)
