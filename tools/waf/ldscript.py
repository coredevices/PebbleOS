# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from waflib import Task, Utils, Errors
from waflib.TaskGen import after, feature


class ldscript_cpp(Task.Task):
    """Preprocess a linker script with the C preprocessor before it's handed
    to the linker, the same way Zephyr does. This gives linker scripts access
    to Kconfig CONFIG_* defines (via autoconf.h), #include, #if, etc."""

    run_str = (
        "${CC} -x assembler-with-cpp -nostdinc -undef -E -P "
        "-include ${AUTOCONF_H} ${SRC} -o ${TGT}"
    )
    color = "PINK"


@after("apply_link")
@feature("cprogram", "cshlib")
def process_ldscript(self):
    if not getattr(self, "ldscript", None) or self.env.CC_NAME != "gcc":
        return

    def convert_to_node(node_or_path_str):
        if isinstance(node_or_path_str, str):
            return self.path.make_node(node_or_path_str)
        else:
            return node_or_path_str

    if isinstance(self.ldscript, str) or isinstance(self.ldscript, list):
        ldscripts = Utils.to_list(self.ldscript)
    else:  # Assume Nod3
        ldscripts = [self.ldscript]
    nodes = [convert_to_node(node) for node in ldscripts]

    for node in nodes:
        if not node:
            raise Errors.WafError("could not find %r" % self.ldscript)

        if self.env.AUTOCONF_H:
            preprocessed = node.parent.find_or_declare(node.name + ".pre")
            tsk = self.create_task("ldscript_cpp", node, preprocessed)
            tsk.dep_nodes.append(self.bld.bldnode.find_or_declare("autoconf.h"))
            node = preprocessed

        self.link_task.env.append_value("LINKFLAGS", "-T%s" % node.abspath())
        self.link_task.dep_nodes.append(node)
