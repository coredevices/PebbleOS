#!/usr/bin/env python
# encoding: utf-8
# vim: sw=4 ts=4 noexpandtab

"""
LLVM Clang-CL support.

Clang-CL is supposed to be a drop-in replacement for MSVC CL, but also serves
well as a cross compiler for Windows from Linux (provided you have set up the
environment). Requires Visual Studio 2015+ to be installed.

On Windows it uses (most) MSVC tools.

Usage:
	$ waf configure
Or:
	$ LLVM_PATH=C:\\Program Files\\LLVM\\bin waf configure
Or:
	def configure(self):
		self.env.LLVM_PATH = 'C:\\Program Files\\LLVM\\bin'
		self.load('clang_cl')
"""

import os

from waflib import Utils, Errors, Logs
from waflib.Configure import conf
from waflib.Tools import msvc

def options(opt):
	msvc.options(opt)

@conf
def get_llvm_paths(self):
	llvm_path = []
	if Utils.is_win32:
		try:
			llvm_key = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Wow6432Node\\LLVM\\LLVM')
		except OSError:
			try:
				llvm_key = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\LLVM\\LLVM')
			except OSError:
				llvm_key = None

		if llvm_key:
			llvm_dir, _ = Utils.winreg.QueryValueEx(llvm_key, '')
			if llvm_dir:
				llvm_path.append(os.path.join(llvm_dir, 'bin'))

	tmp = self.environ.get('LLVM_PATH') or self.env.LLVM_PATH
	if tmp:
		llvm_path.append(tmp)
	llvm_path += self.env.PATH
	return llvm_path

@conf
def find_clang_cl(self):
	"""
	Find the program clang-cl.
	"""
	del(self.env.CC)
	del(self.env.CXX)

	paths = self.get_llvm_paths()
	cc = self.find_program('clang-cl', var='CC', path_list=paths)
	self.env.CC = self.env.CXX = cc
	self.env.CC_NAME_SECONDARY = self.env.CXX_NAME_SECONDARY = 'clang'

	if not Utils.is_win32:
		self.env.MSVC_COMPILER = 'msvc'
		self.env.MSVC_VERSION = 19

		if not self.env.LINK_CXX:
			self.find_program('lld-link', path_list=paths, errmsg='lld-link was not found (linker)', var='LINK_CXX')

		if not self.env.LINK_CC:
			self.env.LINK_CC = self.env.LINK_CXX

@conf
def find_llvm_tools(self):
	"""
	Find the librarian, manifest tool, and resource compiler.
	"""
	self.env.CC_NAME = self.env.CXX_NAME = 'msvc'

	paths = self.get_llvm_paths()
	llvm_path = self.environ.get('LLVM_PATH') or self.env.LLVM_PATH
	if llvm_path:
		paths = [llvm_path] + self.env.PATH
	else:
		paths = self.env.PATH

	if not self.env.AR:
		stliblink = self.find_program('llvm-lib', path_list=paths, var='AR')
		if not stliblink:
			self.fatal('Unable to find required program "llvm-lib"')

		self.env.ARFLAGS = ['/nologo']

	# We assume clang_cl to only be used with relatively new MSVC installations.
	self.env.MSVC_MANIFEST = True
	self.find_program('llvm-mt', path_list=paths, var='MT')
	self.env.MTFLAGS = ['/nologo']

	try:
		self.load('winres')
	except Errors.ConfigurationError:
		Logs.warn('Resource compiler not found. Compiling resource file is disabled')

def configure(self):
	if Utils.is_win32:
		self.autodetect(True)
		self.find_msvc()
	else:
		self.find_llvm_tools()

	self.find_clang_cl()
	self.msvc_common_flags()
	self.cc_load_tools()
	self.cxx_load_tools()
	self.cc_add_flags()
	self.cxx_add_flags()
	self.link_add_flags()
	self.visual_studio_add_flags()
