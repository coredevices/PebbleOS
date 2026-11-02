# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import json
import logging

logger = logging.getLogger(__name__)

INTERNAL_REVISION = 999


class Export:
    def __init__(self, v, app_only, worker_only, deprecated):
        self.name = v["name"]
        self.type = v["type"]

        self.comment = None
        self.app_only = v.get("appOnly", app_only)
        self.worker_only = v.get("workerOnly", worker_only)
        self.include_after = v.get("includeAfter", [])
        self.deprecated = deprecated or v.get("deprecated", False)

    def complete(self):
        return True


class FullExport(Export):
    def __init__(self, v, app_only, worker_only, deprecated):
        super().__init__(v, app_only, worker_only, deprecated)

        self.full_definition = None
        self.impl_name = v.get("implName", self.name)

    def complete(self):
        return self.full_definition is not None


class FunctionExport(FullExport):
    def __init__(self, v, app_only, worker_only, deprecated):
        super().__init__(v, app_only, worker_only, deprecated)

        self.removed = False
        self.skip_definition = v.get("skipDefinition", False)
        self.added_revision = int(v["addedRevision"])
        self.sort_name = v.get("sortName", self.name)
        # The function this one is declared like in headers, see supersede_functions().
        self.declared_as = None
        self.declared_definition = None

        if v.get("removed", False):
            self.removed = True
        else:
            self.impl_name = v.get("implName", self.name)

    def complete(self):
        if self.removed or self.skip_definition:
            return True

        return super().complete()


class StubbedFunctionExport(Export):
    def __init__(self, v, app_only, worker_only, deprecated):
        super().__init__(
            v, app_only, worker_only, deprecated
        )

        self.stub_return = v.get("stubReturn", "0")
        # The function implementing it, for declarations only: a stub has no implementation.
        self.declared_name = v.get("implName", self.name)
        self.stub_definition = v.get("stubDefinition", None)
        self.added_revision = int(v["addedRevision"])
        self.removed = v.get("removed", False)
        self.skip_definition = v.get("skipDefinition", False)
        self.sort_name = v.get("sortName", self.name)
        self.api_exists = v.get("apiExists", False)

    def complete(self):
        return True


class Group:
    def __init__(
        self,
        export,
        parent_group,
        app_only,
        worker_only,
        current_revision,
        deprecated,
        frozen_revision=None,
    ):
        self.name = export["name"]
        self.parent_group = parent_group
        self.app_only = export.get("appOnly", app_only)
        self.worker_only = export.get("workerOnly", worker_only)
        self.deprecated = export.get("deprecated", False) or deprecated
        self.exports = parse_exports_list(
            export["exports"],
            current_revision,
            self,
            self.app_only,
            self.worker_only,
            self.deprecated,
            frozen_revision=frozen_revision,
        )
        self.comment = None
        self.display_name = None

    def group_stack(self):
        stack = [
            self.name,
        ]
        parent_iter = self.parent_group
        while parent_iter is not None:
            stack.insert(0, parent_iter.name)
            parent_iter = parent_iter.parent_group
        return stack

    def qualified_name(self):
        return self.group_stack.join("_")


def parse_exports_list(
    export_definition_list,
    current_revision,
    parent_group=None,
    app_only=False,
    worker_only=False,
    deprecated=False,
    frozen_revision=None,
):
    exports = []

    for e in export_definition_list:
        # Functions marked internal must be assigned a revision number to be sorted later
        if e.get("internal", False):
            if "addedRevision" in e:
                raise RuntimeError(
                    "Internal symbol {} should not have an addedRevision property".format(e["name"])
                )
            else:
                e["addedRevision"] = INTERNAL_REVISION
        if "addedRevision" in e:
            added_revision = int(e["addedRevision"])
            if added_revision > current_revision:
                if added_revision != INTERNAL_REVISION:
                    logger.warning(
                        "Omitting '%s' from SDK export because its revision (%d) "
                        "is higher than the current revision (%d)",
                        e["name"],
                        added_revision,
                        current_revision,
                    )
                continue

        should_stub = (
            frozen_revision is not None
            and "addedRevision" in e
            and int(e["addedRevision"]) > frozen_revision
        )

        # An entry superseded at a revision the SDK has reached keeps its jump table slot for
        # existing apps but leaves the headers, like a deprecated one. Platforms frozen before
        # that revision keep it instead of the entry that replaces it.
        superseded_revision = e.get("supersededRevision")
        visible_revision = (
            frozen_revision if frozen_revision is not None else current_revision
        )
        superseded = (
            superseded_revision is not None
            and int(superseded_revision) <= visible_revision
        )

        if e["type"] == "group":
            exports.append(
                Group(
                    e,
                    parent_group,
                    app_only,
                    worker_only,
                    current_revision,
                    deprecated,
                    frozen_revision=frozen_revision,
                )
            )
        elif e["type"] == "forward_struct":
            exports.append(Export(e, app_only, worker_only, deprecated))
        elif e["type"] == "function":
            if should_stub:
                exports.append(
                    StubbedFunctionExport(e, app_only, worker_only, deprecated)
                )
            else:
                f = FunctionExport(e, app_only, worker_only, deprecated or superseded)
                if superseded:
                    # Its slot keeps the name apps know it by out of the shim library.
                    f.name = "{}__pre{}".format(e["name"], superseded_revision)
                exports.append(f)
        elif e["type"] == "type" or e["type"] == "define":
            exports.append(FullExport(e, app_only, worker_only, deprecated))
        else:
            raise RuntimeError('Unknown type "{}" in export "{}"'.format(e["type"], e["name"]))

    return exports


def parse_export_file(filename, internal_sdk_build, frozen_revision=None):
    with open(filename, "r") as f:
        shim_defs = json.load(f)
        file_revision = int(shim_defs["revision"])

        if file_revision >= INTERNAL_REVISION - 12:
            raise RuntimeError(
                f"File revision at {file_revision:d} is approaching INTERNAL_REVISION at {INTERNAL_REVISION:d}"
            )

        current_revision = INTERNAL_REVISION if internal_sdk_build else file_revision
        exports = parse_exports_list(
            shim_defs["exports"],
            current_revision,
            frozen_revision=frozen_revision,
        )
        supersede_functions(exports)

    return shim_defs["files"], exports


def supersede_functions(exports_tree):
    """Settle functions that share a name because one supersedes another.

    Only one of them may be declared in a header: the one already visible on this platform wins
    over a stub of its replacement. It is declared and documented like the replacement, so apps
    see the same declaration on every platform; only the jump table slot differs.
    """
    by_name = {}

    def collect(e):
        if isinstance(e, (FunctionExport, StubbedFunctionExport)) and not e.deprecated:
            by_name.setdefault(e.name, []).append(e)

    walk_tree(exports_tree, collect)

    for entries in by_name.values():
        if len(entries) < 2:
            continue
        entries.sort(
            key=lambda e: (isinstance(e, StubbedFunctionExport), e.added_revision)
        )
        kept = entries[0]
        for e in entries[1:]:
            e.skip_definition = True
            if isinstance(kept, FunctionExport):
                kept.declared_as = getattr(e, "impl_name", None) or getattr(
                    e, "declared_name", None
                )


def walk_tree(exports_tree, func, include_groups=False):
    """Call func on every Export in our tree"""
    for e in exports_tree:
        if isinstance(e, Group):
            if include_groups:
                func(e)
            walk_tree(e.exports, func, include_groups)
        else:
            func(e)
