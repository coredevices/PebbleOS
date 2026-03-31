# Copyright 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import re
import subprocess
import sys

from waflib.Build import BuildContext
from waflib.Configure import conf


@conf
def load_kconfig(ctx, config_path):
    """Parse a .config file into a Python dictionary.

    Reads lines of the form CONFIG_FOO=val, skipping comments and blank lines.
    String values are unquoted. Returns a dict mapping config key names to
    string values.
    """
    config = {}
    with open(config_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            m = re.match(r'^(CONFIG_\w+)=(.*)$', line)
            if m:
                key = m.group(1)
                val = m.group(2)
                if val in ('y', 'n'):
                    val = val == 'y'
                elif val.startswith('"') and val.endswith('"'):
                    val = val[1:-1]
                elif val.startswith('0x') or val.startswith('0X'):
                    val = int(val, 16)
                else:
                    try:
                        val = int(val)
                    except ValueError:
                        pass
                config[key] = val
    return config


def configure(conf):
    import kconfiglib

    board = conf.options.board
    srcdir = conf.srcnode.abspath()
    blddir = conf.bldnode.abspath()

    defconfig = os.path.join(srcdir, 'boards', board, 'defconfig')
    if not os.path.exists(defconfig):
        conf.fatal(f'Board defconfig not found: {defconfig}')

    os.environ['srctree'] = srcdir
    kconf = kconfiglib.Kconfig(os.path.join(srcdir, 'Kconfig'))
    kconf.load_config(defconfig)

    config_path = os.path.join(blddir, '.config')
    kconf.write_config(config_path)

    autoconf_path = os.path.join(blddir, 'autoconf.h')
    kconf.write_autoconf(autoconf_path)

    kconfig = conf.load_kconfig(config_path)
    for key, val in kconfig.items():
        conf.env[key] = val
    conf.env.append_unique('CFLAGS', ['-include', autoconf_path])
    conf.env.append_unique('cfg_files', [config_path])
    conf.msg('Kconfig', f'{len(kconfig)} symbols loaded from {board}')


class menuconfig(BuildContext):
    """launch menuconfig to interactively configure the firmware"""
    cmd = 'menuconfig'

    def execute(self):
        self.restore()
        srcdir = self.srcnode.abspath()
        blddir = self.bldnode.abspath()

        env = os.environ.copy()
        env['srctree'] = srcdir
        env['KCONFIG_CONFIG'] = os.path.join(blddir, '.config')

        subprocess.run(
            [sys.executable, '-m', 'menuconfig',
             os.path.join(srcdir, 'Kconfig')],
            env=env,
        )
