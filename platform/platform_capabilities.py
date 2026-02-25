# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


# Each set of boards and their capabilities.
# To use these, import as follows:
# ```
# from platform_capabilities import get_capability_dict
# ```

# Master key set. Any capabilities that are added to any platform have to be added here.
# Once added, add the capability to ALL capability dictionaries with the appropriate value.

JAVASCRIPT_BYTECODE_VERSION = 1

master_capability_set = {
    "COMPOSITOR_USES_DMA",
    "HAS_ALS_OPT3001",
    "HAS_ALS_W1160",
    "HAS_APPLE_MFI",
    "HAS_APP_GLANCES",
    "HAS_BUILTIN_HRM",
    "HAS_DEFECTIVE_FW_CRC",
    "HAS_GLYPH_BITMAP_CACHING",
    "HAS_HARDWARE_PANIC_SCREEN",
    "HAS_HEALTH_TRACKING",
    "HAS_ROCKY_JS",
    "HAS_LAUNCHER4",
    "HAS_LED",
    "HAS_MAGNETOMETER",
    "HAS_MAPPABLE_FLASH",
    "HAS_MASKING",
    "HAS_MICROPHONE",
    "HAS_PMIC",
    "HAS_SDK_SHELL4",
    "HAS_SPRF_V3",
    "HAS_TEMPERATURE",
    "HAS_THEMING",
    "HAS_TIMELINE_PEEK",
    "HAS_TOUCHSCREEN",
    "HAS_VIBE_SCORES",
    "HAS_VIBE_DRV2604",
    "HAS_WEATHER",
    "USE_PARALLEL_FLASH",
    "HAS_PUTBYTES_PREACKING",
    "HAS_FLASH_OTP",
    "HAS_VIBE_AW86225",
    "HAS_VIBE_AW8623X",
    "HAS_PBLBOOT",
    "HAS_DYNAMIC_BACKLIGHT",
    "HAS_COLOR_BACKLIGHT",
    "HAS_SPEAKER",
    "HAS_ACCEL_SENSITIVITY",
    "HAS_APP_SCALING",
    "HAS_FPGA_DISPLAY",
    "HAS_ORIENTATION_MANAGER",
    "HAS_PRESSURE_SENSOR",
    "HAS_MODDABLE_XS",
}

board_capability_dicts = [
    {
        "boards": ["snowy_bb2", "snowy_dvt"],
        "capabilities": {
            "COMPOSITOR_USES_DMA",
            "HAS_APPLE_MFI",
            "HAS_APP_GLANCES",
            "HAS_DEFECTIVE_FW_CRC",
            "HAS_HARDWARE_PANIC_SCREEN",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_MAGNETOMETER",
            "HAS_MAPPABLE_FLASH",
            "HAS_MASKING",
            "HAS_MICROPHONE",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_TIMELINE_PEEK",
            "HAS_VIBE_SCORES",
            "USE_PARALLEL_FLASH",
            "HAS_WEATHER",
            "HAS_FPGA_DISPLAY",
        },
    },
    {
        "boards": ["snowy_emery"],
        "capabilities": {
            "COMPOSITOR_USES_DMA",
            "HAS_APPLE_MFI",
            "HAS_APP_GLANCES",
            "HAS_DEFECTIVE_FW_CRC",
            "HAS_HARDWARE_PANIC_SCREEN",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_MAGNETOMETER",
            "HAS_MAPPABLE_FLASH",
            "HAS_MASKING",
            "HAS_MICROPHONE",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_TIMELINE_PEEK",
            "HAS_VIBE_SCORES",
            "USE_PARALLEL_FLASH",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
            "HAS_FPGA_DISPLAY",
            "HAS_APP_SCALING",
            "HAS_MODDABLE_XS",
        },
    },
    {
        "boards": ["spalding_bb2"],
        "capabilities": {
            "COMPOSITOR_USES_DMA",
            "HAS_APP_GLANCES",
            "HAS_DEFECTIVE_FW_CRC",
            "HAS_HARDWARE_PANIC_SCREEN",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_MAGNETOMETER",
            "HAS_MAPPABLE_FLASH",
            "HAS_MASKING",
            "HAS_MICROPHONE",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_VIBE_SCORES",
            "USE_PARALLEL_FLASH",
            "HAS_WEATHER",
            "HAS_FPGA_DISPLAY",
        },
    },
    {
        "boards": ["spalding"],
        "capabilities": {
            "COMPOSITOR_USES_DMA",
            "HAS_APP_GLANCES",
            "HAS_DEFECTIVE_FW_CRC",
            "HAS_HARDWARE_PANIC_SCREEN",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_MAGNETOMETER",
            "HAS_MAPPABLE_FLASH",
            "HAS_MASKING",
            "HAS_MICROPHONE",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_VIBE_SCORES",
            "USE_PARALLEL_FLASH",
            "HAS_WEATHER",
            "HAS_FPGA_DISPLAY",
        },
    },
    {
        "boards": ["spalding_gabbro"],
        "capabilities": {
            "COMPOSITOR_USES_DMA",
            "HAS_APP_GLANCES",
            "HAS_DEFECTIVE_FW_CRC",
            "HAS_HARDWARE_PANIC_SCREEN",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_MAGNETOMETER",
            "HAS_MAPPABLE_FLASH",
            "HAS_MASKING",
            "HAS_MICROPHONE",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_VIBE_SCORES",
            "USE_PARALLEL_FLASH",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
            "HAS_FPGA_DISPLAY",
            "HAS_APP_SCALING",
            "HAS_MODDABLE_XS",
        },
    },
    {
        "boards": ["silk_bb2", "silk"],
        "capabilities": {
            "HAS_APP_GLANCES",
            "HAS_BUILTIN_HRM",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            # 'HAS_MAPPABLE_FLASH' -- TODO: PBL-33860 verify memory-mappable flash works on silk before activating
            "HAS_MICROPHONE",
            # 'USE_PARALLEL_FLASH' -- FIXME hack to get the "modern" flash layout. Fix when we add support for new flash
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_TIMELINE_PEEK",
            "HAS_VIBE_SCORES",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
        },
    },
    {
        "boards": ["silk_flint"],
        "capabilities": {
            "HAS_APP_GLANCES",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            # 'HAS_MAPPABLE_FLASH' -- TODO: PBL-33860 verify memory-mappable flash works on silk before activating
            "HAS_MICROPHONE",
            # 'USE_PARALLEL_FLASH' -- FIXME hack to get the "modern" flash layout. Fix when we add support for new flash
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_TIMELINE_PEEK",
            "HAS_VIBE_SCORES",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
            "HAS_MAGNETOMETER",
            "HAS_PMIC",
            "HAS_FLASH_OTP",
            "HAS_MODDABLE_XS",
        },
    },
    {
        "boards": ["asterix"],
        "capabilities": {
            "HAS_ALS_OPT3001",
            "HAS_APP_GLANCES",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            # 'HAS_MAPPABLE_FLASH' -- TODO: PBL-33860 verify memory-mappable flash works on silk before activating
            "HAS_MICROPHONE",
            # 'USE_PARALLEL_FLASH' -- FIXME hack to get the "modern" flash layout. Fix when we add support for new flash
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_TIMELINE_PEEK",
            "HAS_VIBE_SCORES",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
            "HAS_MAGNETOMETER",
            "HAS_VIBE_DRV2604",
            "HAS_PMIC",
            "HAS_FLASH_OTP",
            "HAS_ACCEL_SENSITIVITY",
            "HAS_ORIENTATION_MANAGER",
            "HAS_PRESSURE_SENSOR",
        },
    },
    {
        "boards": ["obelix_dvt", "obelix_pvt", "obelix_bb2"],
        "capabilities": {
            "HAS_APP_GLANCES",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_TIMELINE_PEEK",
            "HAS_VIBE_SCORES",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
            "HAS_VIBE_AW86225",
            "HAS_FLASH_OTP",
            "HAS_MICROPHONE",
            "HAS_TOUCHSCREEN",
            "HAS_BUILTIN_HRM",
            "HAS_ALS_W1160",
            "HAS_MAGNETOMETER",
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
        "boards": ["getafix_evt", "getafix_dvt"],
        "capabilities": {
            "HAS_APP_GLANCES",
            "HAS_HEALTH_TRACKING",
            "HAS_ROCKY_JS",
            "HAS_LAUNCHER4",
            "HAS_PMIC",
            "HAS_SDK_SHELL4",
            "HAS_SPRF_V3",
            "HAS_TEMPERATURE",
            "HAS_THEMING",
            "HAS_VIBE_SCORES",
            "HAS_WEATHER",
            "HAS_PUTBYTES_PREACKING",
            "HAS_VIBE_AW8623X",
            "HAS_FLASH_OTP",
            "HAS_MICROPHONE",
            "HAS_TOUCHSCREEN",
            "HAS_ALS_W1160",
            "HAS_MAGNETOMETER",
            "HAS_PBLBOOT",
            "HAS_ACCEL_SENSITIVITY",
            "HAS_APP_SCALING",
            "HAS_ORIENTATION_MANAGER",
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
        capabilities_of_board.discard("HAS_ROCKY_JS")
        capabilities_of_board.discard("HAS_MODDABLE_XS")
    elif ctx.env.JS_ENGINE == "moddable":
        capabilities_of_board.discard("HAS_ROCKY_JS")
    elif ctx.env.JS_ENGINE == "rocky":
        capabilities_of_board.discard("HAS_MODDABLE_XS")

    # End overrides section

    false_capabilities = master_capability_set - capabilities_of_board
    cp_dict = {key: True for key in capabilities_of_board}
    cp_dict.update({key: False for key in false_capabilities})

    # inject expected JS bytecode version
    if cp_dict.get("HAS_ROCKY_JS", False):
        cp_dict["JAVASCRIPT_BYTECODE_VERSION"] = JAVASCRIPT_BYTECODE_VERSION

    return cp_dict
