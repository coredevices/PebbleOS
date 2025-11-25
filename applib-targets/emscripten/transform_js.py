#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import re
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('js_file')
    parser.add_argument('--unittest', action='store_true')
    args = parser.parse_args()

    # load file to be processed
    with open(args.js_file, "r") as f:
        source = f.read()

    # remove all known functions for memory access
    # note: this implementation uses a weak heuristic: only the closing } of a
    # given function has no indentation
    for func in ["SAFE_HEAP_LOAD", "SAFE_HEAP_LOAD_D", "SAFE_HEAP_STORE", "SAFE_HEAP_STORE_D"]:
        source = re.sub("function %s\([^\)]*\)\s*{(.*\n)+?}" % func, "", source)

    # applies the same patch as seen at
    # https://github.com/kripken/emscripten/commit/bc11547fbf446993ee0f6f30a0deb3f80f205c35
    # which is part of the fix for https://github.com/kripken/emscripten/issues/3945
    # TODO: fix after PBL-32521 is done
    orig_source = source
    source = source.replace("funcstr += arg + '=' + convertCode.returnValue + ';';",
                            "funcstr += arg + '=(' + convertCode.returnValue + ');';")
    # assert source != orig_source, "Emscripten output does not match expected output of 1.35.0"

    # we're not using emscripten's --pre-js and --post-js as it interferes
    # with --embed-file
    with open(args.js_file, "w") as f:
        f.write(PROLOGUE)
        if args.unittest:
            f.write(UNITTEST_PROLOGUE)
        f.write(source)
        f.write(EPILOGUE)
        if args.unittest:
            f.write("new RockySimulator();\n")

PROLOGUE = """
RockySimulator = function(options) {
  options = options || {};

  var Module = {
    print: function(text) {
      console.log(text);
    },
    printErr: function(text) {
      console.error(text);
    },
  };

"""

EPILOGUE = """
// support non-aligned memory access
function SAFE_HEAP_STORE(dest, value, bytes, isFloat) {
  if (dest <= 0) abort('segmentation fault storing ' + bytes + ' bytes to address ' + dest);
  if (dest + bytes > Math.max(DYNAMICTOP, STATICTOP)) abort('segmentation fault, exceeded the top of the available heap when storing ' + bytes + ' bytes to address ' + dest + '. STATICTOP=' + STATICTOP + ', DYNAMICTOP=' + DYNAMICTOP);
  assert(DYNAMICTOP <= TOTAL_MEMORY);
  if (dest % bytes !== 0) {
    for (var i = 0; i < bytes; i++) {
      HEAPU8[dest + i >> 0] = (value >> (8 * i)) & 0xff;
    }
  } else {
    setValue(dest, value, getSafeHeapType(bytes, isFloat), 1);
  }
}

function SAFE_HEAP_STORE_D(dest, value, bytes) {
  SAFE_HEAP_STORE(dest, value, bytes, true);
}

function SAFE_HEAP_LOAD(dest, bytes, unsigned, isFloat) {
  // overrule
  if (dest <= 0) abort('segmentation fault loading ' + bytes + ' bytes from address ' + dest);
  if (dest + bytes > Math.max(DYNAMICTOP, STATICTOP)) abort('segmentation fault, exceeded the top of the available heap when loading ' + bytes + ' bytes from address ' + dest + '. STATICTOP=' + STATICTOP + ', DYNAMICTOP=' + DYNAMICTOP);
  assert(DYNAMICTOP <= TOTAL_MEMORY);
  var type = getSafeHeapType(bytes, isFloat);
  var ret;
  if (dest % bytes !== 0) {
    for (var i = 0; i < bytes; i++) {
      ret |= HEAPU8[dest + i >> 0] << (8 * i);
    }
  } else {
    ret = getValue(dest, type, 1);
  }
  if (unsigned) ret = unSign(ret, parseInt(type.substr(1)), 1);
  return ret;
}
function SAFE_HEAP_LOAD_D(dest, bytes, unsigned) {
  return SAFE_HEAP_LOAD(dest, bytes, unsigned, true);
}

  return Module;
};

if (typeof(module) !== "undefined") {
    module.exports = RockySimulator;
}

"""

UNITTEST_PROLOGUE = """
var defaultPreRun = Module.preRun;
Module.preRun = function() {
  if (defaultPreRun) {
      defaultPreRun();
  }
  // Mount the host filesystem to make the fixture files accessible:
  FS.mkdir('/node_fs');
  FS.mount(NODEFS, { root: '/' }, '/node_fs');
}
"""

if __name__ == "__main__":
    main()
