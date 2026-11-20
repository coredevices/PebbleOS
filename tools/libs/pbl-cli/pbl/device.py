# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Glue between the CLI and the binary runners.

Turns a configured build plus the parsed arguments into a runner ready to
execute one of the device commands.
"""

import os
import struct
import zlib

from pbl import runners, util
from pbl.errors import CommandContextError


def runner_options(parser):
    """Add the runner selection flag and every runner's own arguments.

    Which runner is active is only known once a build directory has been
    read, so all of them contribute their arguments; the names have to stay
    unique across runners.
    """
    group = parser.add_argument_group("runner options")
    group.add_argument(
        "-r", "--runner", choices=runners.names(), help="Override the board's runner"
    )
    runners.register_args(group)


def _flash_files(build, names):
    images = build.images()
    if names:
        known = [name for name, _ in images]
        unknown = [name for name in names if name not in known]
        if unknown:
            raise CommandContextError(
                f"no image {', '.join(unknown)} in the build; images: {', '.join(known)}"
            )
        images = [(name, files) for name, files in images if name in names]
    return tuple(f for _, files in images for f in files)


# What the bootloader checks ahead of the recovery firmware: the lengths of
# this description and of the firmware, and the firmware's CRC.
RECOVERY_DESCRIPTION = struct.Struct("<III")


def _installs_recovery(build):
    """Whether the bootloader installs the build from the recovery partition:
    the recovery firmware, on boards whose bootloader copies the firmware into
    place rather than booting it from a slot."""
    return build.variant == "prf" and not build.config.CONFIG_PBLBOOT


def _recovery_image(build):
    """``(file, offset)``: the build as the recovery partition holds it."""
    offset, size = build.flash_region("SAFE_FIRMWARE")
    with open(build.bin, "rb") as f:
        firmware = f.read()
    image = RECOVERY_DESCRIPTION.pack(
        RECOVERY_DESCRIPTION.size, len(firmware), zlib.crc32(firmware)
    )
    image += firmware
    if len(image) > size:
        raise CommandContextError(
            f"the recovery firmware ({len(image)} bytes) does not fit "
            f"SAFE_FIRMWARE ({size})"
        )
    path = build.join("recovery.bin")
    with open(path, "wb") as f:
        f.write(image)
    return path, offset


def _code_range(build):
    """``(address, size)`` of the flash the firmware runs from, to its end."""
    config = build.config
    offset = config.CONFIG_FLASH_OFFSET or 0
    return config.CONFIG_FLASH_BASE + offset, config.CONFIG_FLASH_SIZE - offset


def make_runner(
    build,
    args,
    commands,
    resources=False,
    erase_ranges=(),
    extra_args=(),
    watch=None,
    images=None,
    provision=False,
):
    """Create the runner for ``build``, honoring ``--runner``.

    Without it, the board's first runner that can handle the build and one
    of ``commands`` is picked. ``watch``, the lab's (:class:`pbl.lab.Watch`),
    gives the ports the command line leaves out. ``images`` names the images
    of the build to flash, all of them by default. ``provision`` installs a
    recovery firmware build the way the bootloader does, see
    :func:`_installs_recovery`.
    """
    data_files = []
    if resources and build.variant != "prf":
        # The bank the firmware checks first
        offset, _ = build.flash_region("SYSTEM_RESOURCES_BANK_0")
        data_files.append((build.pbpack, offset))

    # Installed the way the bootloader does it: the firmware is erased, so it
    # copies the recovery firmware into place on the next boot.
    flash_files = _flash_files(build, images)
    erase_code = ()
    if provision and build.variant != "prf":
        raise CommandContextError("only a recovery firmware build is provisioned")
    if provision and _installs_recovery(build) and "app" in (images or ["app"]):
        app = dict(build.images()).get("app", [])
        flash_files = tuple(f for f in flash_files if f not in app)
        data_files.append(_recovery_image(build))
        erase_code = _code_range(build)

    cfg = runners.RunnerConfig(
        board_dir=os.path.join(build.topdir, "boards", build.board_spec.name),
        soc=build.config.CONFIG_SOC or None,
        hex_file=build.hex,
        flash_files=flash_files,
        elf_file=build.elf,
        bin_file=build.bin,
        pbpack_file=build.pbpack if build.variant != "prf" else None,
        firmware_slot=build.config.CONFIG_FIRMWARE_SLOT
        if build.config.CONFIG_PBLBOOT
        else None,
        data_files=tuple(data_files),
        erase_code=erase_code,
        erase_ranges=tuple(erase_ranges),
        sftool=build.tool("sftool") or "sftool",
        gdb=build.tool("gdb"),
        gdb_port=getattr(args, "gdb_port", None),
        build=build,
        extra_args=tuple(extra_args),
        tty=getattr(args, "tty", None) or (watch.serial if watch else None),
        probe=getattr(args, "probe", None) or (watch.probe if watch else None),
        watch=watch,
        dry_run=getattr(args, "dry_run", False),
    )

    supported = build.board_spec.runners
    selected = getattr(args, "runner", None)
    if not selected:
        usable = [
            name
            for name in supported
            if runners.get(name).supports(cfg)
            and runners.get(name).capabilities().commands & set(commands)
        ]
        selected = usable[0] if usable else None
    if not selected:
        raise CommandContextError(
            f"board {build.board} has no runner for {' or '.join(commands)}"
        )
    if selected not in supported:
        raise CommandContextError(
            f"board {build.board} does not support the {selected} runner; "
            f"supported runners: {', '.join(supported) or 'none'}"
        )
    if not runners.get(selected).supports(cfg):
        raise CommandContextError(
            f"the {selected} runner cannot run this {build.board} build"
        )
    runner = runners.create(selected, cfg, args)
    if watch is not None and runner.lab_ports:
        util.inf(f"lab: watch {watch.name} ({watch.board})")
    return runner


def run_on_device(build, args, command, resources=False, erase_ranges=(), watch=None):
    """Dispatch one runner command, reporting failures as command errors."""
    try:
        make_runner(
            build,
            args,
            (command,),
            resources=resources,
            erase_ranges=erase_ranges,
            watch=watch,
        ).run(command)
    except runners.RunnerError as e:
        raise CommandContextError(str(e)) from e
