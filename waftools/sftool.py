# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0


def run_command(ctx, cmd):
    if not ctx.options.tty:
        ctx.fatal("Port not specified, use --tty")

    ctx.exec_command(
        f"sftool -c {ctx.env.MICRO_FAMILY} -p {ctx.options.tty} {cmd}",
        stdout=None,
        stderr=None,
    )


def erase_flash(ctx):
    run_command(ctx, "erase_flash")


def write_flash(ctx, *files):
    run_command(ctx, "write_flash " + " ".join(files))
