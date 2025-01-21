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

from waflib import Utils, Errors, Node
from waflib.TaskGen import after, feature


@after('apply_link')
@feature('cprogram', 'cshlib')
def process_ldscript(self):
    if not getattr(self, 'ldscript', None) or self.env.CC_NAME != 'gcc':
        return

    def convert_to_node(node_or_path_str):
        if isinstance(node_or_path_str, str):
            return self.path.make_node(node_or_path_str)
        else:
            return node_or_path_str

    if isinstance(self.ldscript, str) or \
       isinstance(self.ldscript, list):
        ldscripts = Utils.to_list(self.ldscript)
    else:  # Assume Nod3
        ldscripts = [self.ldscript]
    nodes = [convert_to_node(node) for node in ldscripts]

    for node in nodes:
        if not node:
            raise Errors.WafError('could not find %r' % self.ldscript)
        self.link_task.env.append_value('LINKFLAGS', '-T%s' % node.abspath())
        self.link_task.dep_nodes.append(node)
