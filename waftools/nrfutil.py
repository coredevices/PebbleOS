# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0


def run_command(ctx, cmd):
    ctx.exec_command(f"nrfutil device {cmd}", stdout=None, stderr=None)


def erase(ctx):
    run_command(ctx, "erase")


def reset(ctx):
    run_command(ctx, "reset")


def program(ctx, hex_path):
    run_command(
        ctx,
        " ".join(
            (
                "program",
                f"--firmware {hex_path}",
                "--options",
                "chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE",
            )
        ),
    )


def program_and_reset(ctx, hex_path):
    run_command(
        ctx,
        " ".join(
            (
                "program",
                f"--firmware {hex_path}",
                "--options",
                ",".join(
                    (
                        "chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE",
                        "reset=RESET_SYSTEM",
                    )
                ),
            )
        ),
    )
