# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""
Adds `waf clangdb`, which writes build/compile_commands.json for editors such
as clangd. The entries are the commands the app build runs, taken from its
own compile tasks for a single target platform, so flags, include paths and
sources set in the project's wscript all show up. Nothing gets compiled: only
the tasks that generate headers (resource IDs, message keys) are run.
"""

import os
import shutil

from waflib import Build, Errors, Logs, Runner, Task


def options(opt):
    opt.add_option(
        "--clangdb-platform",
        dest="clangdb_platform",
        default=None,
        help="Platform whose compile commands `waf clangdb` writes (default: "
        "emery if targeted, else the first target platform)",
    )


def _default_platform(platforms):
    # emery has the widest API surface, so it suits completion best.
    return "emery" if "emery" in platforms else platforms[0]


def _compile_command(task):
    """The command a compile task would run, without running it."""

    def exec_command(*k, **kw):
        return 0

    task.exec_command = exec_command
    try:
        task.run()
    finally:
        del task.exec_command
    cwd = task.get_cwd()
    src = task.inputs[0]

    # waf passes paths relative to the build directory; spell them out so the
    # entries don't depend on it.
    def absolute(arg):
        if arg == src.path_from(cwd):
            return src.abspath()
        for flag in ("-I", "-o"):
            path = arg[len(flag) :]
            if arg.startswith(flag) and path and not os.path.isabs(path):
                return flag + os.path.normpath(os.path.join(cwd.abspath(), path))
        return arg

    cmd = [absolute(arg) for arg in task.last_cmd]

    # Editors query the compiler for its builtin headers, so a compiler the
    # wscript configured by name gets resolved too.
    if not os.path.isabs(cmd[0]):
        path = (task.env.env or os.environ).get("PATH")
        cmd[0] = shutil.which(cmd[0], path=path) or cmd[0]
    return cmd


class ClangDbContext(Build.BuildContext):
    """writes build/compile_commands.json for one target platform"""

    cmd = "clangdb"
    to_run = ()

    def total(self):
        return len(self.to_run)

    def compile(self):
        for i, group in enumerate(self.groups):
            self.current_group = i
            for tg in group:
                post = getattr(tg, "post", None)
                if post:
                    post()

        platforms = self.all_envs[""].TARGET_PLATFORMS
        if not platforms:
            self.fatal("This project has no target platforms to compile for")
        platform = self.options.clangdb_platform or _default_platform(platforms)
        if platform not in platforms:
            self.fatal(
                f"This project doesn't target '{platform}'. "
                f"Available platforms: {', '.join(platforms)}."
            )

        tasks = [
            t
            for i in range(len(self.groups))
            for t in self.get_tasks_group(i)
            if t.env.PLATFORM_NAME in (platform, [])
        ]
        compile_classes = tuple(
            cls for cls in map(Task.classes.get, ("c", "cxx")) if cls
        )
        compiles = [t for t in tasks if isinstance(t, compile_classes)]

        self.write_compilation_database(compiles, platform)
        self.generate_headers(tasks, compiles)

    def write_compilation_database(self, compiles, platform):
        entries = {}
        for task in compiles:
            src = task.inputs[0].abspath()
            if src in entries:
                continue
            entries[src] = {
                "directory": task.get_cwd().abspath(),
                "file": src,
                "arguments": _compile_command(task),
                "output": task.outputs[0].abspath(),
            }

        database = self.bldnode.make_node("compile_commands.json")
        database.write_json(list(entries.values()))
        Logs.info(
            "Wrote %s (%d files, platform '%s')",
            database.abspath(),
            len(entries),
            platform,
        )

    def generate_headers(self, tasks, compiles):
        """Run the tasks headers come out of, and whatever they need first,
        so the sources' generated includes resolve."""
        producers = {node: t for t in tasks for node in t.outputs}
        candidates = set(tasks) - set(compiles)
        needed = set()

        def need(task):
            if task in needed or task not in candidates:
                return
            needed.add(task)
            for node in task.inputs + task.dep_nodes:
                if node in producers:
                    need(producers[node])
            for prev in task.run_after:
                need(prev)

        for task in tasks:
            if any(node.name.endswith(".h") for node in task.outputs):
                need(task)

        to_run = [t for t in tasks if t in needed]
        for task in to_run:
            task.run_after = {t for t in task.run_after if t in needed}
        Task.set_file_constraints(to_run)
        Task.set_precedence_constraints(to_run)

        def build_iterator():
            yield to_run
            while True:
                yield []

        self.to_run = to_run
        self.producer = Runner.Parallel(self, self.jobs)
        self.producer.biter = build_iterator()
        self.producer.start()
        if self.is_dirty():
            self.store()
        if self.producer.error:
            raise Errors.BuildError(self.producer.error)
