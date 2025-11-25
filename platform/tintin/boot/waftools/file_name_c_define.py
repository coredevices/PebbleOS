# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

"""
Define a __FILE_NAME__ macro to expand to the filename of the C/C++ source,
stripping the other path components.
"""
from waflib.TaskGen import feature, after_method


@feature('c')
@after_method('create_compiled_task')
def file_name_c_define(self):
    for task in self.tasks:
        task.env.append_value(
                'DEFINES', '__FILE_NAME__="%s"' % task.inputs[0].name)
