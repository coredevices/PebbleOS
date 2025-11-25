# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


def write(output_file, bytes, var_name):
    output_file.write("static const uint8_t {var_name}[] = {{\n  ".format(var_name=var_name))
    for byte, index in zip(bytes, list(range(0, len(bytes)))):
        if index != 0 and index % 16 == 0:
            output_file.write("/* bytes {0} - {1} */\n  ".format(index - 16, index))
        output_file.write("0x%02x, " % byte)
    output_file.write("\n};\n")
