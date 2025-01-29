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

"""
Define a __FILE_NAME__ macro to expand to the filename of the C/C++ source,
stripping the other path components.
"""
from waflib.TaskGen import feature, after_method

@feature('c')
@after_method('create_compiled_task')
def file_name_c_define(self):
    for task in self.tasks:
        if len(task.inputs) > 0:
            task.env.append_value(
                    'DEFINES', '__FILE_NAME_LEGACY__="%s"' % task.inputs[0].name)

