
#
# This file is the default set of rules to compile a Pebble project.
#
# Feel free to customize this to your needs.
#

import os.path

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')

    ctx.pbl_program(source=ctx.path.ant_glob('src/**/*.c'),
                    target='pebble-app.elf')

    if os.path.exists('worker_src'):
        ctx.pbl_worker(source=ctx.path.ant_glob('worker_src/**/*.c'),
                        target='pebble-worker.elf')
        ctx.pbl_bundle(elf='pebble-app.elf',
                        worker_elf='pebble-worker.elf',
                        js=ctx.path.ant_glob('src/js/**/*.js'))
    else:
        ctx.pbl_bundle(elf='pebble-app.elf',
                        js=ctx.path.ant_glob('src/js/**/*.js'))
