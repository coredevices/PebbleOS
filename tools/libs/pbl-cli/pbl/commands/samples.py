# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import os
import time

from pbl import runners, samples
from pbl.command import PblCommand
from pbl.device import make_runner
from pbl.errors import CommandContextError

SAMPLES_DIR = "samples"
DEFAULT_BUILD_DIR = "build-samples"
DEFAULT_BOARD = "qemu_emery"
LOG_FILE = "samples.log"


class Samples(PblCommand):
    group = "build"

    def __init__(self):
        super().__init__(
            "samples",
            "Build the samples and check what they print",
            "Build each sample for a board, run it where the board runs on "
            "the host, such as an emulator, and check its console against "
            "the sample's sample.yml. Samples are built only for the other "
            "boards.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "samples",
            nargs="*",
            metavar="SAMPLE",
            help="Sample, or directory of samples, e.g. samples/kernel "
            f"(default: all of {SAMPLES_DIR}/)",
        )
        parser.add_argument(
            "--board",
            default=DEFAULT_BOARD,
            metavar="BOARD",
            help="Board to build for (default: %(default)s)",
        )
        parser.add_argument(
            "-l", "--list", action="store_true", help="List the samples and exit"
        )
        parser.add_argument(
            "--build-only",
            action="store_true",
            help="Build the samples without running",
        )
        parser.add_argument(
            "-v",
            "--verbose",
            action="store_true",
            help="Show the console of the samples that pass too",
        )
        return parser

    def do_run(self, args, unknown):
        root = os.path.join(self.topdir, SAMPLES_DIR)
        try:
            found = samples.find(root, args.samples)
        except samples.SampleError as e:
            self.die(str(e))

        if args.list:
            width = max(len(s.name) for s in found)
            for sample in found:
                print(f"{sample.name:<{width}}  {sample.description}")
            return 0

        base = getattr(args, "build_dir", None) or os.path.join(
            self.topdir, DEFAULT_BUILD_DIR
        )
        failed = []
        counts = {}
        for sample in found:
            if not sample.supports(args.board):
                outcome = "skipped"
            else:
                build_dir = os.path.join(
                    base, args.board.replace("@", "_"), *sample.name.split(os.sep)
                )
                start = time.monotonic()
                outcome = self._one(sample, build_dir, args)
                outcome_time = time.monotonic() - start
            counts[outcome] = counts.get(outcome, 0) + 1
            if outcome == "skipped":
                self.inf(f"SKIP  {sample.name} (not for {args.board})", color="yellow")
            elif outcome == "failed":
                failed.append(sample.name)
                self.inf(f"FAIL  {sample.name} ({outcome_time:.1f} s)", color="red")
            else:
                label = "PASS " if outcome == "passed" else "BUILT"
                self.inf(f"{label} {sample.name} ({outcome_time:.1f} s)", color="green")

        summary = ", ".join(f"{n} {outcome}" for outcome, n in sorted(counts.items()))
        self.inf(f"{args.board}: {summary}", color="red" if failed else "green")
        return 1 if failed else 0

    def _one(self, sample, build_dir, args):
        """Build and run one sample: "passed", "built" or "failed"."""
        from pbl.build import BuildDir

        build = BuildDir(build_dir, self.topdir)
        if self.dry_run:
            self.cmake_configure(build, args.board, app=sample.path)
            self.cmake_build(build)
            return "built"

        os.makedirs(build.path, exist_ok=True)
        log_path = build.join(LOG_FILE)
        with open(log_path, "w") as log:
            quiet = {"stdout": log, "stderr": log}
            rc = self.cmake_configure(build, args.board, app=sample.path, **quiet)
            rc = rc or self.run_cmd(["cmake", "--build", str(build)], **quiet)
        if rc:
            self._show(log_path)
            return "failed"
        if args.build_only:
            return "built"

        try:
            runner = make_runner(build, args, ("run",))
            with open(log_path, "a") as log:
                # Runners print the commands they run on the way.
                argv, env = self._quietly(log, runner.console_command)
        except (runners.UnsupportedOperation, CommandContextError):
            return "built"
        except runners.RunnerError as e:
            self.wrn(str(e))
            self._show(log_path)
            return "failed"

        result = samples.check(
            argv,
            sample.expect,
            sample.timeout,
            env=env,
            cwd=build.path,
            echo=print if args.verbose else None,
        )
        if result.passed:
            return "passed"

        if not args.verbose:
            print("\n".join(result.output), flush=True)
        if result.rebooted:
            why = "it booted again"
        elif result.returncode is not None:
            why = f"exited with status {result.returncode}"
        else:
            why = f"nothing in {sample.timeout:g} s"
        self.wrn(f"{sample.name}: no line matching {result.missing!r} ({why})")
        return "failed"

    @staticmethod
    def _quietly(log, fn):
        """Call ``fn`` with the output of the processes it starts, and its
        own prints, going to ``log``."""
        stdout = os.dup(1)
        try:
            with contextlib.redirect_stdout(log):
                os.dup2(log.fileno(), 1)
                return fn()
        finally:
            log.flush()
            os.dup2(stdout, 1)
            os.close(stdout)

    @staticmethod
    def _show(log_path):
        with open(log_path, errors="replace") as log:
            print("".join(log.readlines()[-60:]), end="", flush=True)
