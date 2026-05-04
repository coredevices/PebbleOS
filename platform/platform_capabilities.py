# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


# Each set of boards and their capabilities.
# To use these, import as follows:
# ```
# from platform_capabilities import get_capability_dict
# ```

# Master key set. Any capabilities that are added to any platform have to be added here.
# Once added, add the capability to ALL capability dictionaries with the appropriate value.

master_capability_set = {
    "HAS_MASKING",
    "HAS_SETTINGS_ICONS",
    "HAS_SPRF_V3",
    "HAS_THEMING",
    "HAS_TIMELINE_PEEK",
    "USE_PARALLEL_FLASH",
    "HAS_PUTBYTES_PREACKING",
    "HAS_FLASH_OTP",
    "HAS_PBLBOOT",
    "HAS_DYNAMIC_BACKLIGHT",
    "HAS_COLOR_BACKLIGHT",
    "HAS_SPEAKER",
    "HAS_ACCEL_SENSITIVITY",
    "HAS_APP_SCALING",
    "HAS_ORIENTATION_MANAGER",
    "HAS_MODDABLE_XS",
}

board_capability_dicts = [
    {
        "boards": ["asterix"],
        "capabilities": {
            "HAS_SPRF_V3",
            "HAS_TIMELINE_PEEK",
            "HAS_PUTBYTES_PREACKING",
            "HAS_FLASH_OTP",
            "HAS_ACCEL_SENSITIVITY",
            "HAS_ORIENTATION_MANAGER",
            "HAS_SPEAKER",
        },
    },
    {
        "boards": ["obelix_dvt", "obelix_pvt", "obelix_bb2"],
        "capabilities": {
            "HAS_SPRF_V3",
            "HAS_TIMELINE_PEEK",
            "HAS_PUTBYTES_PREACKING",
            "HAS_FLASH_OTP",
            "HAS_PBLBOOT",
            "HAS_DYNAMIC_BACKLIGHT",
            "HAS_COLOR_BACKLIGHT",
            "HAS_SPEAKER",
            "HAS_ACCEL_SENSITIVITY",
            "HAS_APP_SCALING",
            "HAS_ORIENTATION_MANAGER",
            "HAS_MODDABLE_XS",
        },
    },
    {
        "boards": ["getafix_evt", "getafix_dvt", "getafix_dvt2"],
        "capabilities": {
            "HAS_SPRF_V3",
            "HAS_TIMELINE_PEEK",
            "HAS_PUTBYTES_PREACKING",
            "HAS_FLASH_OTP",
            "HAS_PBLBOOT",
            "HAS_ACCEL_SENSITIVITY",
            "HAS_APP_SCALING",
            "HAS_ORIENTATION_MANAGER",
            "HAS_MODDABLE_XS",
        },
    },
    {
        "boards": ["qemu_emery"],
        "capabilities": {
            "HAS_SPRF_V3",
            "HAS_TIMELINE_PEEK",
            "HAS_PUTBYTES_PREACKING",
            "HAS_APP_SCALING",
            "HAS_MODDABLE_XS",
            "HAS_COLOR_BACKLIGHT",
            "HAS_SPEAKER",
        },
    },
    {
        "boards": ["qemu_flint"],
        "capabilities": {
            "HAS_SPRF_V3",
            "HAS_TIMELINE_PEEK",
            "HAS_PUTBYTES_PREACKING",
            "HAS_MODDABLE_XS",
            "HAS_SPEAKER",
        },
    },
    {
        "boards": ["qemu_gabbro"],
        "capabilities": {
            "HAS_SPRF_V3",
            "HAS_PUTBYTES_PREACKING",
            "HAS_APP_SCALING",
            "HAS_MODDABLE_XS",
        },
    },
]

# Run through again and make sure all sets include only valid keys defined in
# `master_capability_set`
boards_seen = set()

for board_dict in board_capability_dicts:
    capabilities_of_board = board_dict["capabilities"]
    boards = board_dict["boards"]

    # Check for duplicate boards using the intersection of boards already seen and the boards
    # in the dict we are operating on. After the check, add the ones seen to the set
    duped_boards = boards_seen.intersection(boards)
    if duped_boards:
        raise ValueError(
            "There are multiple capability sets for the boards {!r}".format(
                duped_boards
            )
        )
    boards_seen.update(boards)

    # Check for capabilities that aren't in the master_capability_set
    unknown_capabilities = capabilities_of_board - master_capability_set
    if unknown_capabilities:
        raise ValueError(
            "The capability set for boards {!r} contains unknown "
            "capabilities {!r}".format(boards, unknown_capabilities)
        )


def get_capability_dict(ctx, board):
    capabilities_of_board = None
    # Find capability set for board
    for capability_dict in board_capability_dicts:
        if board in capability_dict["boards"]:
            capabilities_of_board = capability_dict["capabilities"]

    if not capabilities_of_board:
        raise KeyError(
            'Capability set for board: "{}" is missing or undefined'.format(board)
        )

    # Overrides
    # If you want the capabilities to change depending on the configure/build environment, add
    # them here.

    if ctx.env.JS_ENGINE == "none":
        capabilities_of_board.discard("HAS_MODDABLE_XS")

    # End overrides section

    false_capabilities = master_capability_set - capabilities_of_board
    cp_dict = {key: True for key in capabilities_of_board}
    cp_dict.update({key: False for key in false_capabilities})

    return cp_dict
