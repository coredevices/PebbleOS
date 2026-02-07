# SPDX-License-Identifier: Apache-2.0

from waflib import TaskGen, Task, Context


@TaskGen.extension('.zig')
def zig_hook(self, node):
    "Binds the zig file extensions to create Zig task instances"
    return self.create_compiled_task('zig', node)


class zig(Task.Task):
    "Compiles Zig files into object files"
    run_str = '${ZIG} build-obj ${ZIGFLAGS} ${CPPPATH_ST:INCPATHS} ${DEFINES_ST:DEFINES} ${SRC} -femit-bin=${TGT[0].abspath()}'


def configure(conf):
    conf.find_program('zig', var='ZIG', mandatory=True)

    # Lookup cross-compiler include paths
    cc_include_paths = [path.strip() for path in conf.cmd_and_log([conf.env.CC[0], '-E', '-Wp,-v', '-xc', '/dev/null'], output=Context.STDERR, quiet=Context.BOTH).split('\n') if path.startswith(' /')]

    optimize_flag = 'ReleaseSmall'

    pebble_zigflags = ['-target', 'thumb-freestanding-eabi',
                       '-mcpu', 'cortex_m3',
                       '-fPIC',
                       '-fno-unwind-tables',
                       '-O', optimize_flag]

    pebble_zigflags += ['-I' + path for path in cc_include_paths]

    # Disable time.h and define time_t (see pebble_sdk_gcc.py)
    if (conf.env.SDK_VERSION_MAJOR == 5) and (conf.env.SDK_VERSION_MINOR > 19):
        pebble_zigflags.append('-D_TIME_H_')
        pebble_zigflags.append('-Dtime_t=long')

    conf.env.prepend_value('ZIGFLAGS', pebble_zigflags)
