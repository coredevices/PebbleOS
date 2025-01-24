#! /usr/bin/env python
# encoding: utf-8
# Detection of the flang Fortran compiler

import re
from waflib.Tools import fc,fc_config,fc_scan
from waflib.Configure import conf
from waflib.Tools.compiler_fc import fc_compiler
fc_compiler['linux'].append('fc_flang')

@conf
def find_flang(conf):
	fc = conf.find_program(['flang'], var='FC')
	conf.get_nfort_version(fc)
	conf.env.FC_NAME = 'FLANG'
	conf.env.FC_MOD_CAPITALIZATION = 'lower'

@conf
def flang_flags(conf):
	v = conf.env
	v['_FCMODOUTFLAGS'] = []
	v['FCFLAGS_DEBUG'] = []
	v['FCFLAGS_fcshlib'] = []
	v['LINKFLAGS_fcshlib'] = []
	v['FCSTLIB_MARKER'] = ''
	v['FCSHLIB_MARKER'] = ''

@conf
def get_flang_version(conf, fc):
	cmd = fc + ['-dM', '-E', '-']
	env = conf.env.env or None

	try:
		out, err = conf.cmd_and_log(cmd, output=0, input='\n'.encode(), env=env)
	except Errors.WafError:
		conf.fatal('Could not determine the FLANG compiler version for %r' % cmd)
	if out.find('__clang__') < 0:
		conf.fatal('Not a flang compiler')

	k = {}
	out = out.splitlines()
	for line in out:
		lst = shlex.split(line)
		if len(lst)>2:
			key = lst[1]
			val = lst[2]
			k[key] = val

	def isD(var):
		return var in k

	# Some documentation is available at http://predef.sourceforge.net
	# The names given to DEST_OS must match what Utils.unversioned_sys_platform() returns.
	if not conf.env.DEST_OS:
		conf.env.DEST_OS = ''
	for i in MACRO_TO_DESTOS:
		if isD(i):
			conf.env.DEST_OS = MACRO_TO_DESTOS[i]
			break
	else:
		if isD('__APPLE__') and isD('__MACH__'):
			conf.env.DEST_OS = 'darwin'
		elif isD('__unix__'): # unix must be tested last as it's a generic fallback
			conf.env.DEST_OS = 'generic'

	if isD('__ELF__'):
		conf.env.DEST_BINFMT = 'elf'
	elif isD('__WINNT__') or isD('__CYGWIN__') or isD('_WIN32'):
		conf.env.DEST_BINFMT = 'pe'
		if not conf.env.IMPLIBDIR:
			conf.env.IMPLIBDIR = conf.env.LIBDIR # for .lib or .dll.a files
		conf.env.LIBDIR = conf.env.BINDIR
	elif isD('__APPLE__'):
		conf.env.DEST_BINFMT = 'mac-o'

	if not conf.env.DEST_BINFMT:
		# Infer the binary format from the os name.
		conf.env.DEST_BINFMT = Utils.destos_to_binfmt(conf.env.DEST_OS)

	for i in MACRO_TO_DEST_CPU:
		if isD(i):
			conf.env.DEST_CPU = MACRO_TO_DEST_CPU[i]
			break

	Logs.debug('fc_flang: dest platform: ' + ' '.join([conf.env[x] or '?' for x in ('DEST_OS', 'DEST_BINFMT', 'DEST_CPU')]))
	conf.env.FC_VERSION = (k['__clang_major__'], k['__clang_minor__'], k['__clang_patchlevel__'])

	return k


def configure(conf):
	conf.find_flang()
	conf.find_ar()
	conf.fc_flags()
	conf.fc_add_flags()
	conf.flang_flags()
