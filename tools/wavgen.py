# SPDX-FileCopyrightText: 2025 Core Devices, LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import re
import struct
import wave

import serial


def main(port, output, channels):
    raw_sound = []

    with serial.Serial(port, 1000000, timeout=10) as ser:
        print("Switching to prompt")

        # switch to prompt (Ctrl+C)
        ser.write(b'\x03')
        ser.reset_input_buffer()

        print("Start test on the watch now")

        finished = False
        while not finished:
            try:
                line = ser.readline().decode().strip()
            except UnicodeDecodeError:
                continue

            if not re.match(r"[SE0-9\-]+", line):
                continue

            if line == "S":
                print("Data started, storing...")
                continue
            elif line == "E":
                print("All data received!")
                finished = True
            else:
                raw_sound.append(int(line))

    with wave.open(output, "w") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(16000)

        for value in raw_sound:
            data = struct.pack('<h', value)
            wav.writeframesraw(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", required=True, help="Serial port")
    parser.add_argument("-o", "--output", required=True, help="Output file")
    parser.add_argument("-c", "--channels", default=2, help="Channels")
    args = parser.parse_args()

    main(args.port, args.output, args.channels)
