#!/usr/bin/env python3
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

"""
Coredump analyzer for ARM embedded systems using arm-none-eabi-gdb.
Loads a coredump file and extracts relevant debugging information to a text file.
"""

import subprocess
import sys
import os
import argparse
import tempfile
from datetime import datetime


class CoredumpAnalyzer:
    def __init__(self, elf_file, coredump_file, output_file=None):
        self.elf_file = elf_file
        self.coredump_file = coredump_file
        self.output_file = (
            output_file
            or f"coredump_analysis_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        )
        self.gdb_executable = "arm-none-eabi-gdb"

    def create_gdb_script(self):
        """Create a GDB script with commands to extract relevant information."""
        commands = [
            # Load the files
            f"file {self.elf_file}",
            f"core-file {self.coredump_file}",
            "",
            # Basic info
            "echo \\n=== BASIC INFORMATION ===\\n",
            "info target",
            "",
            # Thread information
            "echo \\n=== THREAD INFORMATION ===\\n",
            "info threads",
            "thread apply all bt",
            "",
            # Current thread detailed backtrace
            "echo \\n=== CURRENT THREAD DETAILED BACKTRACE ===\\n",
            "bt full",
            "",
            # Register state
            "echo \\n=== REGISTER STATE ===\\n",
            "info registers",
            "info all-registers",
            "",
            # Memory mappings
            "echo \\n=== MEMORY MAPPINGS ===\\n",
            "info proc mappings",
            "maintenance info sections",
            "",
            # Stack examination
            "echo \\n=== STACK EXAMINATION ===\\n",
            "x/32xw $sp",
            "",
            # Local variables in current frame
            "echo \\n=== LOCAL VARIABLES (CURRENT FRAME) ===\\n",
            "info locals",
            "",
            # Arguments in current frame
            "echo \\n=== FUNCTION ARGUMENTS (CURRENT FRAME) ===\\n",
            "info args",
            "",
            # Source information
            "echo \\n=== SOURCE INFORMATION ===\\n",
            "info source",
            "info line",
            "",
            # Disassembly around current PC
            "echo \\n=== DISASSEMBLY AROUND PC ===\\n",
            "x/20i $pc-40",
            "",
            # Frame information for all threads
            "echo \\n=== DETAILED FRAME INFO FOR ALL THREADS ===\\n",
            "thread apply all frame",
            "thread apply all info frame",
            "",
            # Exit
            "quit",
        ]

        return "\n".join(commands)

    def analyze(self):
        """Run GDB with the script and capture output."""
        print(f"Analyzing coredump: {self.coredump_file}")
        print(f"Using ELF file: {self.elf_file}")
        print(f"Output will be saved to: {self.output_file}")

        # Create GDB script
        gdb_script = self.create_gdb_script()

        # Write script to temporary file
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".gdb", delete=False
        ) as script_file:
            script_file.write(gdb_script)
            script_path = script_file.name

        # Run GDB with the script
        try:
            result = subprocess.run(
                [self.gdb_executable, "--batch", "--quiet", "-x", script_path],
                capture_output=True,
                text=True,
                check=False,
            )

            # Clean up temporary file
            os.unlink(script_path)

            # Prepare output
            output_lines = []
            output_lines.append("=" * 80)
            output_lines.append(f"COREDUMP ANALYSIS REPORT")
            output_lines.append(
                f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
            )
            output_lines.append(f"ELF File: {self.elf_file}")
            output_lines.append(f"Coredump File: {self.coredump_file}")
            output_lines.append("=" * 80)
            output_lines.append("")

            # Add GDB output
            if result.stdout:
                output_lines.append(result.stdout)

            # Add any errors
            if result.stderr:
                output_lines.append("\n=== GDB ERRORS/WARNINGS ===")
                output_lines.append(result.stderr)

            # Write to file
            with open(self.output_file, "w") as f:
                f.write("\n".join(output_lines))

            print("\nAnalysis complete!")

            return result.returncode == 0

        except FileNotFoundError:
            print(
                f"Error: {self.gdb_executable} not found. Please ensure arm-none-eabi-gdb is installed and in PATH."
            )
            return False
        except Exception as e:
            print(f"Error during analysis: {e}")
            return False


def main():
    parser = argparse.ArgumentParser(
        description="Analyze ARM coredump files using arm-none-eabi-gdb",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s firmware.elf core.dump
  %(prog)s firmware.elf core.dump -o crash_analysis.txt
  %(prog)s --elf app.elf --core coredump.bin
  %(prog)s --elf app.elf --core coredump.bin --output report.txt
        """,
    )

    parser.add_argument("elf", nargs="?", help="ELF file with debug symbols")
    parser.add_argument("coredump", nargs="?", help="Coredump file")
    parser.add_argument(
        "-e",
        "--elf",
        dest="elf_file",
        help="ELF file with debug symbols (alternative syntax)",
    )
    parser.add_argument(
        "-c", "--core", dest="core_file", help="Coredump file (alternative syntax)"
    )
    parser.add_argument(
        "-o", "--output", help="Output file name (default: timestamped file)"
    )

    args = parser.parse_args()

    # Determine which arguments to use
    elf_file = args.elf_file or args.elf
    coredump_file = args.core_file or args.coredump

    if not elf_file or not coredump_file:
        parser.print_help()
        sys.exit(1)

    # Validate input files exist
    if not os.path.exists(elf_file):
        print(f"Error: ELF file '{elf_file}' not found")
        sys.exit(1)

    if not os.path.exists(coredump_file):
        print(f"Error: Coredump file '{coredump_file}' not found")
        sys.exit(1)

    # Run analysis
    analyzer = CoredumpAnalyzer(elf_file, coredump_file, args.output)
    success = analyzer.analyze()

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
