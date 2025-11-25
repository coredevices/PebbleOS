# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from waflib import Task
from waflib.TaskGen import extension

class asm(Task.Task):
    color = 'BLUE'
    run_str = '${AS} ${ASFLAGS} ${DEFINES_ST:DEFINES} -c ${SRC} -o ${TGT}'

@extension('.s', '.S')
def asm_hook(self,node):
    return self.create_compiled_task('asm',node)
