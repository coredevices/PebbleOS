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

# FIXME: For some reason this doesn't work with multiple rules with the same input extension.
#from waflib import TaskGen
#TaskGen.declare_chain(name='hex', rule='${OBJCOPY} -O ihex ${SRC} ${TGT}', ext_in='.elf', ext_out='.hex')
#TaskGen.declare_chain(name='bin', rule='${OBJCOPY} -O binary ${SRC} ${TGT}', ext_in='.elf', ext_out='.bin')

def objcopy(task, mode, extra_args=None):
    cmd = 'arm-none-eabi-objcopy -S -R .stack -R .priv_bss -R .bss -R .retained '

    if hasattr(task.generator, 'extra_args'):
        cmd += '%s ' % (task.generator.extra_args)

    if extra_args is not None:
        cmd += '%s ' % (extra_args)

    cmd += '-O %s "%s" "%s"' %  (mode, task.inputs[0].abspath(), task.outputs[0].abspath())
    return task.exec_command(cmd)

def objcopy_fill_bss(task, mode):
    return task.exec_command('arm-none-eabi-objcopy -O %s -j .text -j .data '
            '-j .bss --set-section-flags .bss=alloc,load,contents "%s" "%s"' %
        (mode, task.inputs[0].abspath(), task.outputs[0].abspath()))

def objcopy_hex(task):
    return objcopy(task, 'ihex')

def objcopy_bin(task):
    return objcopy(task, 'binary')
