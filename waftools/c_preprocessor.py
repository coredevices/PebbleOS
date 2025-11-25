# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

"""
Tool that invokes the C preprocessor with any type of file.
"""


# FIXME: convert this from a rule to a task
def c_preproc(task):
    args = {
        'CC': task.generator.env.CC[0],
        'CFLAGS': ' '.join(task.generator.cflags),
        'SRC': task.inputs[0].abspath(),
        'TGT': task.outputs[0].abspath(),
    }
    return task.exec_command(
        '{CC} -E -P -c {CFLAGS} "{SRC}" -o "{TGT}"'.format(**args))


def configure(ctx):
    pass
