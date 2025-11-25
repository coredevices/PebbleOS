# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import os
from waflib.Errors import BuildError

import inject_metadata


def configure(conf):
    """
    This method is called from the configure method of the pebble_sdk waftool to setup the
    environment variables for compiling a 3rd party app
    :param conf: the ConfigContext
    :return: None
    """
    CROSS_COMPILE_PREFIX = 'arm-none-eabi-'

    conf.env.AS = CROSS_COMPILE_PREFIX + 'gcc'
    conf.env.AR = CROSS_COMPILE_PREFIX + 'ar'
    conf.env.CC = CROSS_COMPILE_PREFIX + 'gcc'
    conf.env.LD = CROSS_COMPILE_PREFIX + 'ld'
    conf.env.SIZE = CROSS_COMPILE_PREFIX + 'size'

    optimize_flag = '-Os'

    conf.load('gcc')

    pebble_cflags = ['-std=c99',
                     '-mcpu=cortex-m3',
                     '-mthumb',
                     '-ffunction-sections',
                     '-fdata-sections',
                     '-g',
                     '-fPIE',
                     optimize_flag]
    c_warnings = ['-Wall',
                  '-Wextra',
                  '-Werror',
                  '-Wno-unused-parameter',
                  '-Wno-error=unused-function',
                  '-Wno-error=unused-variable']

    if (conf.env.SDK_VERSION_MAJOR == 5) and (conf.env.SDK_VERSION_MINOR > 19):
        pebble_cflags.append('-D_TIME_H_')
    pebble_cflags.extend(c_warnings)

    pebble_linkflags = ['-mcpu=cortex-m3',
                        '-mthumb',
                        '-Wl,--gc-sections',
                        '-Wl,--warn-common',
                        '-fPIE',
                        optimize_flag]

    conf.env.prepend_value('CFLAGS', pebble_cflags)
    conf.env.prepend_value('LINKFLAGS', pebble_linkflags)
    conf.env.SHLIB_MARKER = None
    conf.env.STLIB_MARKER = None


# -----------------------------------------------------------------------------------
def gen_inject_metadata_rule(bld, src_bin_file, dst_bin_file, elf_file, resource_file, timestamp,
                             has_pkjs, has_worker):
    """
    Copy from src_bin_file to dst_bin_file and inject the correct meta-data into the
    header of dst_bin_file
    :param bld: the BuildContext
    :param src_bin_file: the path to the pebble-app.raw.bin file
    :param dst_bin_file: the path to the pebble-app.bin
    :param elf_file: the path to the pebble-app.elf file
    :param resource_file: the path to the resource pack
    :param timestamp: the timestamp of the project build
    :param has_pkjs: boolean for whether the project contains code using PebbleKit JS
    :param has_worker: boolean for whether the project has a worker binary
    """

    def inject_data_rule(task):
        bin_path = task.inputs[0].abspath()
        elf_path = task.inputs[1].abspath()
        if len(task.inputs) >= 3:
            res_path = task.inputs[2].abspath()
        else:
            res_path = None
        tgt_path = task.outputs[0].abspath()

        # First copy the raw bin that the compiler produced to a new location. This way we'll have
        # the raw binary around to inspect just in case anything went wrong while we were injecting
        # metadata.
        cp_result = task.exec_command('cp "{}" "{}"'.format(bin_path, tgt_path))
        if cp_result < 0:
            raise BuildError("Failed to copy %s to %s!" % (bin_path, tgt_path))

        # Now actually inject the metadata into the new copy of the binary.
        inject_metadata.inject_metadata(tgt_path, elf_path, res_path, timestamp,
                                        allow_js=has_pkjs, has_worker=has_worker)

    sources = [src_bin_file, elf_file]
    if resource_file is not None:
        sources.append(resource_file)
    bld(rule=inject_data_rule, name='inject-metadata', source=sources, target=dst_bin_file)
