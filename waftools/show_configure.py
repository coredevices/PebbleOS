# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from __future__ import print_function

import os
from shlex import quote

from waflib import ConfigSet, Options
from waflib.Build import BuildContext
from waflib.Configure import conf


def load_lockfile(env, basepath):
    lockfile_path = os.path.join(basepath, Options.lockfile)
    try:
        env.load(lockfile_path)
    except IOError:
        raise ValueError('{} is not configured yet'.format(os.path.basename(os.getcwd())))
    except Exception:
        raise ValueError('Could not load {}'.format(lockfile_path))


@conf
def get_lockfile(ctx):
    env = ConfigSet.ConfigSet()

    try:
        load_lockfile(env, ctx.out_dir)
    except ValueError:
        try:
            load_lockfile(env, ctx.top_dir)
        except ValueError as err:
            ctx.fatal(str(err))
            return

    return env


class show_configure(BuildContext):
    """shows the last used configure command"""
    cmd = 'show_configure'

    def execute_build(ctx):
        env = ctx.get_lockfile()
        if not env:
            return

        argv = env.argv

        # Configure time environment vars
        for var in ['CFLAGS']:
            if var in env.environ:
                argv = ['{}={}'.format(var, quote(env.environ[var]))] + argv

        # Persistent environment vars
        for var in ['WAFLOCK']:
            if var in env.environ:
                argv = ['export {}={};'.format(var, quote(env.environ[var]))] + argv

        # Print and force waf to complete without further output
        print(' '.join(argv))
        exit()
