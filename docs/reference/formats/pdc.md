# Draw commands (PDC)

Pebble Draw Commands are the vector-graphics format behind
`GDrawCommand`: an image (`.pdc` / PDCI) or animation sequence (PDCS) of
stroke/fill commands rendered at runtime. The packed structs and file
magics are defined in `src/fw/applib/graphics/gdraw_command_private.h`;
files are generated from SVG or JSON by `tools/generate_pdcs/` (the
serializers and format docstring live in
`tools/generate_pdcs/pebble_commands.py`), and `tools/pdc2png` renders
them back to PNG. All multi-byte fields are little-endian.

## Draw command

| Field          | Type              | Notes                                                |
| -------------- | ----------------- | ---------------------------------------------------- |
| type           | `uint8_t`         | 1 = path, 2 = circle, 3 = precise path (0 = invalid) |
| flags          | `uint8_t`         | bit 0: hidden; bits 1–7 reserved                     |
| stroke color   | `uint8_t`         | ARGB8                                                |
| stroke width   | `uint8_t`         |                                                      |
| fill color     | `uint8_t`         | ARGB8                                                |
| path: open     | `uint8_t` ×2      | 1 = open path, 0 = closed; second byte unused        |
| circle: radius | `uint16_t`        | (shares the two bytes above)                         |
| num_points     | `uint16_t`        | 1 for circles                                        |
| points         | `int16_t` ×2 each | x, y per point                                       |

Path points are whole pixels; precise-path points are 13.3 fixed-point
(eighths of a pixel). A command list is a `uint16_t` command count
followed by that many commands.

## File containers

**PDCI (image)**:

| Field                  | Type                    |
| ---------------------- | ----------------------- |
| magic                  | `"PDCI"`                |
| payload size           | `uint32_t`              |
| version                | `uint8_t` (currently 1) |
| reserved               | `uint8_t`               |
| view-box width, height | `int16_t` ×2            |
| command list           |                         |

**PDCS (sequence)** replaces the command list with animation frames:

| Field                       | Type                                          |
| --------------------------- | --------------------------------------------- |
| magic                       | `"PDCS"`                                      |
| payload size                | `uint32_t`                                    |
| version, reserved, view-box | as PDCI                                       |
| play count                  | `uint16_t`                                    |
| frame count                 | `uint16_t`                                    |
| frames                      | `uint16_t` duration (ms) + command list, each |
