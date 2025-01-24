# encoding: utf-8
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Philipp Bender, 2012
# Matt Clarkson, 2012

import re
from waflib.Task import Task
from waflib.TaskGen import extension, after_method

"""
A simple tool to integrate protocol buffers into your build system.
Adapted for nanopb from waftools/extras/protoc.py.

Example::

    def configure(conf):
        conf.load('compiler_c c protoc')

    def build(bld):
        bld(
                features = 'c cprogram'
                source   = 'main.c file1.proto proto/file2.proto',
                include  = '. proto',
                target   = 'executable')

Notes when using this tool:

- protoc command line parsing is tricky.

  The generated files can be put in subfolders which depend on
  the order of the include paths.

  Try to be simple when creating task generators
  containing protoc stuff.

"""

class protoc(Task):
    # protoc expects the input proto file to be an absolute path.
    run_str = '${NANOPB_GENERATOR} -I ${SRC[0].parent.abspath()} -D ${TGT[0].parent.abspath()} ${SRC[0].abspath()}'
    color   = 'BLUE'
    ext_out = ['.h', 'pb.c']
    def scan(self):
        """
        Scan .proto dependencies
        """
        node = self.inputs[0]

        nodes = []
        names = []
        seen = []

        if not node: return (nodes, names)

        def parse_node(node):
            if node in seen:
                return
            seen.append(node)
            code = node.read().splitlines()
            for line in code:
                m = re.search(r'^import\s+"(.*)";.*(//)?.*', line)
                if m:
                    dep = m.groups()[0]
                    for incpath in self.generator.includes_nodes:
                        found = incpath.find_resource(dep)
                        if found:
                            nodes.append(found)
                            parse_node(found)
                        else:
                            names.append(dep)

        parse_node(node)
        return (nodes, names)

@extension('.proto')
def process_protoc(self, node):
    c_node = node.change_ext('.pb.c')
    h_node = node.change_ext('.pb.h')
    self.create_task('protoc', node, [c_node, h_node])
    self.source.append(c_node)

    use = getattr(self, 'use', '')
    if not 'PROTOBUF' in use:
        self.use = self.to_list(use) + ['PROTOBUF']

def configure(conf):
    missing_nanopb = """
    'nanopb' cannot be found on the system.

    Follow the instructions on the wiki for installing it: https://pebbletechnology.atlassian.net/wiki/display/DEV/Getting+Started+with+Firmware
    """
    conf.find_program('nanopb_generator', var='NANOPB_GENERATOR', errmsg=missing_nanopb)
