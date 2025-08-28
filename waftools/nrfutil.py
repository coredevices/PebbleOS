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
