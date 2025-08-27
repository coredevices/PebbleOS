# Copyright 2025 Core Devices LLC
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

def run_command(ctx, cmd):
    if not ctx.options.tty:
        ctx.fatal('Port not specified, use --tty')

    ctx.exec_command(f'sftool -c {ctx.env.MICRO_FAMILY} -p {ctx.options.tty} {cmd}',
                     stdout=None, stderr=None)

def erase_flash(ctx):
    run_command(ctx, 'erase_flash')

def write_flash(ctx, hex_path):
    run_command(ctx, f'write_flash {hex_path}')
