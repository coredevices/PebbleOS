# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


def compress(task):
    cmd = ['cp', task.inputs[0].abspath(), task.inputs[0].get_bld().abspath()]
    task.exec_command(cmd)

    cmd = ['xz', '--keep', '--check=crc32', '--lzma2=dict=4KiB', task.inputs[0].get_bld().abspath()]
    task.exec_command(cmd)
