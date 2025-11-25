#!/usr/bin/env python
# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import subprocess
import sys
from pathlib import Path


def find_board_config(board_name):
    """Find the nrfjprog config file for the specified board."""
    script_dir = Path(__file__).parent.parent
    config_path = (
        script_dir
        / "src/fw/board/boards/support"
        / f"board_{board_name}_nrfjprog_config.toml"
    )

    if not config_path.exists():
        print(f"Error: Configuration file not found: {config_path}", file=sys.stderr)
        sys.exit(1)

    return config_path


def dump_qspi(board_name, output_file):
    """Dump QSPI contents to a file using nrfjprog."""

    if output_file.suffix.lower() != '.hex':
        print(f"Error: output file must have .hex extension: {output_file}", file=sys.stderr)
        sys.exit(1)

    print(f"Dumping QSPI contents to: {output_file}")

    subprocess.run(
        [
            "nrfjprog",
            "--readqspi",
            output_file,
            "--config",
            str(find_board_config(board_name)),
        ],
        check=True,
    )

    print("Done!")


def program_qspi(board_name, input_file):
    """Program QSPI contents from a file using nrfjprog."""

    if input_file.suffix.lower() != '.hex':
        print(f"Error: output file must have .hex extension: {input_file}", file=sys.stderr)
        sys.exit(1)

    print(f"Programming QSPI contents from: {input_file}")

    subprocess.run(
        [
            "nrfjprog",
            "--program",
            input_file,
            "--qspichiperase",
            "--config",
            str(find_board_config(board_name)),
        ],
        check=True,
    )

    print("Done!")


def main():
    parser = argparse.ArgumentParser(
        description="Dump or program QSPI flash contents using nrfjprog",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Dump QSPI contents for asterix board
  %(prog)s dump asterix qspi_dump.hex

  # Program QSPI contents for asterix board
  %(prog)s program asterix qspi_image.hex
        """,
    )

    subparsers = parser.add_subparsers(dest="command", help="Command to execute")
    subparsers.required = True

    # Dump command
    dump_parser = subparsers.add_parser("dump", help="Dump QSPI contents to a file")
    dump_parser.add_argument("board", help="Board name (e.g., asterix)")
    dump_parser.add_argument("output", type=Path, help="Output file path")

    # Program command
    program_parser = subparsers.add_parser(
        "program", help="Program QSPI contents from a file"
    )
    program_parser.add_argument("board", help="Board name (e.g., asterix)")
    program_parser.add_argument("input", type=Path, help="Input file path")

    args = parser.parse_args()

    if args.command == "dump":
        dump_qspi(args.board, args.output)
    elif args.command == "program":
        program_qspi(args.board, args.input)


if __name__ == "__main__":
    main()
