#! /usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

# encoding: utf-8
# XCode 3/XCode 4 generator for Waf
# Nicolas Mercier 2011

"""
Usage:

def options(opt):
    opt.load('xcode')

$ waf configure xcode
"""

# TODO: support iOS projects

from waflib import Context, TaskGen, Build, Utils
import os, sys, random, time

HEADERS_GLOB = '**/(*.h|*.hpp|*.H|*.inl)'

MAP_EXT = {
    '.h' :  "sourcecode.c.h",

    '.hh':  "sourcecode.cpp.h",
    '.inl': "sourcecode.cpp.h",
    '.hpp': "sourcecode.cpp.h",

    '.c':   "sourcecode.c.c",

    '.m':   "sourcecode.c.objc",

    '.mm':  "sourcecode.cpp.objcpp",

    '.cc':  "sourcecode.cpp.cpp",

    '.cpp': "sourcecode.cpp.cpp",
    '.C':   "sourcecode.cpp.cpp",
    '.cxx': "sourcecode.cpp.cpp",
    '.c++': "sourcecode.cpp.cpp",

    '.l':   "sourcecode.lex", # luthor
    '.ll':  "sourcecode.lex",

    '.y':   "sourcecode.yacc",
    '.yy':  "sourcecode.yacc",

    '.plist': "text.plist.xml",
    ".nib":   "wrapper.nib",
    ".xib":   "text.xib",
}

SOURCE_EXT = frozenset(['.c', '.cpp', '.m', '.cxx', '.c++', '.C', '.cc', '.s', '.S'])

part1 = 0
part2 = 10000
part3 = 0
id = 562000999
def newid():
    global id
    id = id + 1
    return "%04X%04X%04X%012d" % (0, 10000, 0, id)

class XCodeNode:
    def __init__(self):
        self._id = newid()

    def tostring(self, value):
        if isinstance(value, dict):
            result = "{\n"
            for k,v in value.items():
                result = result + "\t\t\t%s = %s;\n" % (k, self.tostring(v))
            result = result + "\t\t}"
            return result
        elif isinstance(value, str):
            return "\"%s\"" % value
        elif isinstance(value, list):
            result = "(\n"
            for i in value:
                result = result + "\t\t\t%s,\n" % self.tostring(i)
            result = result + "\t\t)"
            return result
        elif isinstance(value, XCodeNode):
            return value._id
        else:
            return str(value)

    def write_recursive(self, value, file):
        if isinstance(value, dict):
            for k,v in value.items():
                self.write_recursive(v, file)
        elif isinstance(value, list):
            for i in value:
                self.write_recursive(i, file)
        elif isinstance(value, XCodeNode):
            value.write(file)

    def write(self, file):
        for attribute,value in self.__dict__.items():
            if attribute[0] != '_':
                self.write_recursive(value, file)

        w = file.write
        w("\t%s = {\n" % self._id)
        w("\t\tisa = %s;\n" % self.__class__.__name__)
        for attribute,value in self.__dict__.items():
            if attribute[0] != '_':
                w("\t\t%s = %s;\n" % (attribute, self.tostring(value)))
        w("\t};\n\n")



# Configurations
class XCBuildConfiguration(XCodeNode):
    def __init__(self, name, settings = {}, env=None):
        XCodeNode.__init__(self)
        self.baseConfigurationReference = ""
        self.buildSettings = settings
        self.name = name
        if env and env.ARCH:
            settings['ARCHS'] = " ".join(env.ARCH)
        settings['COMBINE_HIDPI_IMAGES'] = 'YES'
        settings['ONLY_ACTIVE_ARCH'] = 'YES'
    def config_octest(self):
        self.buildSettings = {'PRODUCT_NAME':'$(TARGET_NAME)', 'WRAPPER_EXTENSION':'octest', 'COMBINE_HIDPI_IMAGES':'YES', 'ONLY_ACTIVE_ARCH':'YES'}

class XCConfigurationList(XCodeNode):
    def __init__(self, settings):
        XCodeNode.__init__(self)
        self.buildConfigurations = settings
        self.defaultConfigurationIsVisible = 0
        self.defaultConfigurationName = settings and settings[0].name or ""

# Group/Files
class PBXFileReference(XCodeNode):
    def __init__(self, name, path, filetype = '', sourcetree = "<group>"):
        XCodeNode.__init__(self)
        self.fileEncoding = 4
        if not filetype:
            _, ext = os.path.splitext(name)
            filetype = MAP_EXT.get(ext, 'text')
        self.lastKnownFileType = filetype
        self.name = name
        if os.path.isabs(path):
            sourcetree = '<absolute>'
            self.path = path
        else:
            sourcetree = '<group>'
            self.path = os.path.basename(path)

class PBXGroup(XCodeNode):
    def __init__(self, name, sourcetree = "<group>"):
        XCodeNode.__init__(self)
        self.children = []
        self.name = name
        self.path = name
        self.sourceTree = sourcetree

    def add(self, root, sources):
        folders = {}
        def folder(n):
            if n == root:
                return self
            try:
                return folders[n]
            except KeyError:
                f = PBXGroup(n.name)
                p = folder(n.parent)
                folders[n] = f
                p.children.append(f)
                return f
        for s in sources:
            f = folder(s.parent)
            source = PBXFileReference(s.name, s.abspath())
            f.children.append(source)
    def add_all_files_from_folder_path(self, directory):
        files = []
        def should_skip(filepath):
            name = os.path.basename(os.path.abspath(filepath))
            return name.startswith('.') or os.path.splitext(name)[1] == '.xcodeproj' or name == 'build' # or has_hidden_attribute(filepath)
        for name in os.listdir(directory):
            path = os.path.join(directory, name)
            if should_skip(path):
                continue
            if os.path.isfile(path):
                fileref=PBXFileReference(os.path.basename(path), path)
                self.children.append(fileref)
                files.append(fileref)
            elif os.path.isdir(path):
                subgroup = PBXGroup(name)
                files.extend(subgroup.add_all_files_from_folder_path(path))
                self.children.append(subgroup)
        return files


# Targets
class PBXLegacyTarget(XCodeNode):
    def __init__(self,target=''):
        XCodeNode.__init__(self)
        self.buildConfigurationList = XCConfigurationList([XCBuildConfiguration('waf')])
        self.buildArgumentsString="$(ACTION)"
        self.buildPhases = []
        self.buildToolPath="./waf-xcode.sh"
        self.buildWorkingDirectory = ""
        self.dependencies = []
        self.name = target
        self.productName = target
        self.passBuildSettingsInEnvironment = 0

class PBXShellScriptBuildPhase(XCodeNode):
    def __init__(self, script):
        XCodeNode.__init__(self)
        self.buildActionMask = 2147483647
        self.files = []
        self.inputPaths = []
        self.outputPaths = []
        self.runOnlyForDeploymentPostProcessing = 1
        self.shellPath = "/bin/sh"
        self.shellScript = script

class PBXNativeTarget(XCodeNode):
    def __init__(self, action=None, target=None, node=None, env=None, script=None, productType="com.apple.product-type.application"):
        XCodeNode.__init__(self)
        opts = {'PRODUCT_NAME':target, 'HEADER_SEARCH_PATHS': "$(SRCROOT)/../src/**"}
        if node:
            opts['CONFIGURATION_BUILD_DIR'] = node.parent.abspath()
        conf = XCBuildConfiguration('waf', opts, env)
        self.buildConfigurationList = XCConfigurationList([conf])
        self.buildPhases = []
        if script != None:
            self.buildPhases.append(PBXShellScriptBuildPhase(script))
        self.buildRules = []
        self.dependencies = []
        self.name = target
        self.productName = target
        self.productType = productType
        if node: product_dir = node.abspath()
        else: product_dir = ""
        self.productReference = PBXFileReference(target, product_dir, 'wrapper.application', 'BUILT_PRODUCTS_DIR')
    def config_octest_target(self):
        conf = XCBuildConfiguration('waf', {}, None)
        conf.config_octest()
        self.buildConfigurationList = XCConfigurationList([conf])
        self.productType = "com.apple.product-type.bundle"

class PBXSourcesBuildPhase(XCodeNode):
    def __init__(self):
        XCodeNode.__init__(self)
        self.buildActionMask = 2147483647
        self.runOnlyForDeploymentPostprocessing = 0
        self.files = []
    def add_files(self, files):
        for f in files:
            _, ext = os.path.splitext(f.name)
            if ext in SOURCE_EXT:
                bf = PBXBuildFile(f)
                self.files.append(bf)

class PBXBuildFile(XCodeNode):
    def __init__(self, fileRef):
        XCodeNode.__init__(self)
        self.fileRef = fileRef

# Root project object
class PBXProject(XCodeNode):
    def __init__(self, name, version):
        XCodeNode.__init__(self)
        self.buildConfigurationList = XCConfigurationList([XCBuildConfiguration('waf', {})])
        self.compatibilityVersion = version[0]
        self.hasScannedForEncodings = 1;
        self.mainGroup = PBXGroup(name)
        self.projectRoot = ""
        self.projectDirPath = ""
        self.targets = []
        self._objectVersion = version[1]
        self._output = PBXGroup('out')
        self.mainGroup.children.append(self._output)

    def write(self, file):
        w = file.write
        w("// !$*UTF8*$!\n")
        w("{\n")
        w("\tarchiveVersion = 1;\n")
        w("\tclasses = {\n")
        w("\t};\n")
        w("\tobjectVersion = %d;\n" % self._objectVersion)
        w("\tobjects = {\n\n")

        XCodeNode.write(self, file)

        w("\t};\n")
        w("\trootObject = %s;\n" % self._id)
        w("}\n")

class xcode_pebble(Build.BuildContext):
    """creates an xcode project file"""
    cmd = 'xcode'
    fun = 'build'

    def collect_source(self, tg):
        source_files = tg.to_nodes(getattr(tg, 'source', []))
        plist_files = tg.to_nodes(getattr(tg, 'mac_plist', []))
        resource_files = [tg.path.find_node(i) for i in Utils.to_list(getattr(tg, 'mac_resources', []))]
        include_dirs = Utils.to_list(getattr(tg, 'includes', [])) + Utils.to_list(getattr(tg, 'export_dirs', []))
        include_files = []
        for x in include_dirs:
            if not isinstance(x, str):
                include_files.append(x)
                continue
            d = tg.path.find_node(x)
            if d:
                lst = [y for y in d.ant_glob(HEADERS_GLOB, flat=False)]
                include_files.extend(lst)

        # remove duplicates
        source = list(set(source_files + plist_files + resource_files + include_files))
        source.sort(key=lambda x: x.abspath())
        return source

    def execute(self):
        """
        Entry point
        """
        self.restore()
        if not self.all_envs:
            self.load_envs()
        self.recurse([self.run_dir])
        root = os.path.basename(self.srcnode.abspath())
        appname = getattr(Context.g_module, Context.APPNAME, root)
        p = PBXProject(appname, ('Xcode 3.2', 46))

        # Xcode Target that invokes waf-xcode.sh:
        target = PBXLegacyTarget('waf')
        p.targets.append(target)

        # Add references to all files:
        p.mainGroup.path = "../"
        files = p.mainGroup.add_all_files_from_folder_path(self.srcnode.abspath())

        # FIXME: How to get SDK path?
        sdk_path = os.path.join(os.path.dirname(Context.__file__), '..', '..')
        if sdk_path and os.path.exists(sdk_path):
            sdk_include_path = os.path.abspath(os.path.join(sdk_path, 'include'))
            if os.path.exists(sdk_include_path):
                sdk_headers = p.mainGroup.add_all_files_from_folder_path(sdk_include_path)
                files.extend(sdk_headers)
        
        # Create dummy native app that is needed to trigger Xcode's code completion + indexing:
        index_dummy_target = PBXNativeTarget(None, "index_dummy", productType="com.apple.product-type.tool")
        index_dummy_sources_phase = PBXSourcesBuildPhase()
        index_dummy_sources_phase.add_files(files)
        index_dummy_target.buildPhases.append(index_dummy_sources_phase)
        p.targets.append(index_dummy_target)
        
        # Create fake .octest bundle to invoke ./waf test:
        clar_tests_target = PBXNativeTarget(None, "clar_tests", script="export ACTION=test\n./waf-xcode.sh")
        clar_tests_target.config_octest_target()
        p.targets.append(clar_tests_target)
        
        # Xcode Target that invokes waf test
        target = PBXLegacyTarget('waf test')
        target.buildArgumentsString = "test"
        p.targets.append(target)

        # Write generated project to disk:
        node = self.srcnode.make_node('xcode/%s.xcodeproj' % appname)
        node.mkdir()
        node = node.make_node('project.pbxproj')
        p.write(open(node.abspath(), 'w'))
        
        # Generate waf-xcode.sh shim script
        xcscript_node=self.srcnode.make_node('xcode/waf-xcode.sh')
        xcscript_path=xcscript_node.abspath()
        f = open(xcscript_path,'w')
        f.write("#!/bin/bash\n\
# Expecting PebbleSDK + arm toolchain + openocd binaries to be in $PATH after sourcing .bash_profile:\n\
export PATH=`python ../tools/strip_xcode_paths.py`\n\
source ~/.bash_profile\n\
cd ..\n\
ACTION=$@\n\
if [ -z $ACTION ]; then\n\
    ACTION=build\n\
fi\n\
# Use pypy if available\n\
if ! which pypy &> /dev/null; then\n\
    # Check if waf is on the path:\n\
    if ! type \"waf\" &> /dev/null; then\n\
        ./waf $ACTION\n\
    else\n\
        waf $ACTION\n\
    fi\n\
else\n\
    echo \"Using pypy\"\n\
    pypy waf $ACTION\n\
fi\n\
")
        os.chmod(xcscript_path, 0o0755)
        f.close()
        
