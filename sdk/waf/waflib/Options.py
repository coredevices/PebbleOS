#!/usr/bin/env python
# encoding: utf-8
# Scott Newton, 2005 (scottn)
# Thomas Nagy, 2006-2018 (ita)

"""
Support for waf command-line options

Provides default and command-line options, as well the command
that reads the ``options`` wscript function.
"""

import os, tempfile, argparse, sys, re
from waflib import Logs, Utils, Context, Errors


class OptionValues:
	def __str__(self):
		return str(self.__dict__)

options = OptionValues()
"""
A global dictionary representing user-provided command-line options::

	$ waf --foo=bar
"""

commands = []
"""
List of commands to execute extracted from the command-line. This list
is consumed during the execution by :py:func:`waflib.Scripting.run_commands`.
"""

lockfile = os.environ.get('WAFLOCK', '.lock-waf_%s_build' % sys.platform)
"""
Name of the lock file that marks a project as configured
"""

class ArgParser(argparse.ArgumentParser):
	"""
	Command-line options parser.
	"""
	def __init__(self, ctx):
		argparse.ArgumentParser.__init__(self, add_help=False, conflict_handler='resolve')
		self.ctx = ctx

	def format_help(self):
		self.usage = self.get_usage()
		return super(ArgParser, self).format_help()

	def format_usage(self):
		return self.format_help()

	def _get_formatter(self):
		"""Initialize the argument parser to the adequate terminal width"""
		return self.formatter_class(prog=self.prog, width=Logs.get_term_cols())

	def get_option(self, name):
		if name in self._option_string_actions:
			return self._option_string_actions[name]

	def remove_option(self, name):
		if name in self._option_string_actions:
			action = self._option_string_actions[name]
			self._remove_action(action)
			action.option_strings.remove(name)
			self._option_string_actions.pop(name, None)
			for group in self._action_groups:
				try:
					group._group_actions.remove(action)
				except ValueError:
					pass

	def get_usage(self):
		"""
		Builds the message to print on ``waf --help``

		:rtype: string
		"""
		cmds_str = {}
		for cls in Context.classes:
			if not cls.cmd or cls.cmd == 'options' or cls.cmd.startswith( '_' ):
				continue

			s = cls.__doc__ or ''
			cmds_str[cls.cmd] = s

		if Context.g_module:
			for (k, v) in Context.g_module.__dict__.items():
				if k in ('options', 'init', 'shutdown'):
					continue

				if type(v) is type(Context.create_context):
					if v.__doc__ and len(v.__doc__.splitlines()) < 3 and not k.startswith('_'):
						cmds_str[k] = v.__doc__

		just = 0
		for k in cmds_str:
			just = max(just, len(k))

		lst = ['  %s: %s' % (k.ljust(just), v) for (k, v) in cmds_str.items()]
		lst.sort()
		ret = '\n'.join(lst)

		return '''%s [commands] [options]

Main commands (example: ./%s build -j4)
%s
''' % (Context.WAFNAME, Context.WAFNAME, ret)


class OptionsContext(Context.Context):
	"""
	Collects custom options from wscript files and parses the command line.
	Sets the global :py:const:`waflib.Options.commands` and :py:const:`waflib.Options.options` values.
	"""
	cmd = 'options'
	fun = 'options'

	def __init__(self, **kw):
		super(OptionsContext, self).__init__(**kw)

		self.parser = ArgParser(self)
		"""Instance of :py:class:`waflib.Options.ArgParser`"""

		self.option_groups = {}

		jobs = self.jobs()
		p = self.add_option
		color = os.environ.get('NOCOLOR', '') and 'no' or 'auto'
		if os.environ.get('CLICOLOR', '') == '0':
			color = 'no'
		elif os.environ.get('CLICOLOR_FORCE', '') == '1':
			color = 'yes'
		p('-c', '--color',    dest='colors',  default=color, action='store', help='whether to use colors (yes/no/auto) [default: auto]', choices=('yes', 'no', 'auto'))
		p('-j', '--jobs',     dest='jobs',    default=jobs,  type=int, help='amount of parallel jobs (%r)' % jobs)
		p('-k', '--keep',     dest='keep',    default=0,     action='count', help='continue despite errors (-kk to try harder)')
		p('-v', '--verbose',  dest='verbose', default=0,     action='count', help='verbosity level -v -vv or -vvv [default: 0]')
		p('--zones',          dest='zones',   default='',    action='store', help='debugging zones (task_gen, deps, tasks, etc)')
		p('--profile',        dest='profile', default=0,     action='store_true', help=argparse.SUPPRESS)
		p('--pdb',            dest='pdb',     default=0,     action='store_true', help=argparse.SUPPRESS)
		p('-h', '--help',     dest='whelp',   default=0,     action='store_true', help='show this help message and exit')
		p('--version',        dest='version', default=False, action='store_true', help='show the Waf version and exit')

		gr = self.add_option_group('Configuration options')

		gr.add_option('-o', '--out', action='store', default='', help='build dir for the project', dest='out')
		gr.add_option('-t', '--top', action='store', default='', help='src dir for the project', dest='top')

		gr.add_option('--no-lock-in-run', action='store_true', default=os.environ.get('NO_LOCK_IN_RUN', ''), help=argparse.SUPPRESS, dest='no_lock_in_run')
		gr.add_option('--no-lock-in-out', action='store_true', default=os.environ.get('NO_LOCK_IN_OUT', ''), help=argparse.SUPPRESS, dest='no_lock_in_out')
		gr.add_option('--no-lock-in-top', action='store_true', default=os.environ.get('NO_LOCK_IN_TOP', ''), help=argparse.SUPPRESS, dest='no_lock_in_top')

		default_prefix = getattr(Context.g_module, 'default_prefix', os.environ.get('PREFIX'))
		if not default_prefix:
			if Utils.unversioned_sys_platform() == 'win32':
				d = tempfile.gettempdir()
				default_prefix = d[0].upper() + d[1:]
				# win32 preserves the case, but gettempdir does not
			else:
				default_prefix = '/usr/local/'
		gr.add_option('--prefix', dest='prefix', default=default_prefix, help='installation prefix [default: %r]' % default_prefix)
		gr.add_option('--bindir', dest='bindir', help='bindir')
		gr.add_option('--libdir', dest='libdir', help='libdir')

		gr = self.add_option_group('Build and installation options')
		gr.add_option('-p', '--progress', dest='progress_bar', default=0, action='count', help= '-p: progress bar; -pp: ide output')
		gr.add_option('--targets',        dest='targets', default='', action='store', help='task generators, e.g. "target1,target2"')

		gr = self.add_option_group('Step options')
		gr.add_option('--files',          dest='files', default='', action='store', help='files to process, by regexp, e.g. "*/main.c,*/test/main.o"')

		default_destdir = os.environ.get('DESTDIR', '')

		gr = self.add_option_group('Installation and uninstallation options')
		gr.add_option('--destdir', help='installation root [default: %r]' % default_destdir, default=default_destdir, dest='destdir')
		gr.add_option('-f', '--force', dest='force', default=False, action='store_true', help='disable file installation caching')
		gr.add_option('--distcheck-args', metavar='ARGS', help='arguments to pass to distcheck', default=None, action='store')

	def jobs(self):
		"""
		Finds the optimal amount of cpu cores to use for parallel jobs.
		At runtime the options can be obtained from :py:const:`waflib.Options.options` ::

			from waflib.Options import options
			njobs = options.jobs

		:return: the amount of cpu cores
		:rtype: int
		"""
		count = int(os.environ.get('JOBS', 0))
		if count < 1:
			if 'NUMBER_OF_PROCESSORS' in os.environ:
				# on Windows, use the NUMBER_OF_PROCESSORS environment variable
				count = int(os.environ.get('NUMBER_OF_PROCESSORS', 1))
			else:
				# on everything else, first try the POSIX sysconf values
				if hasattr(os, 'sysconf_names'):
					if 'SC_NPROCESSORS_ONLN' in os.sysconf_names:
						count = int(os.sysconf('SC_NPROCESSORS_ONLN'))
					elif 'SC_NPROCESSORS_CONF' in os.sysconf_names:
						count = int(os.sysconf('SC_NPROCESSORS_CONF'))
				if not count and os.name not in ('nt', 'java'):
					try:
						tmp = self.cmd_and_log(['sysctl', '-n', 'hw.ncpu'], quiet=0)
					except Errors.WafError:
						pass
					else:
						if re.match('^[0-9]+$', tmp):
							count = int(tmp)
		if count < 1:
			count = 1
		elif count > 1024:
			count = 1024
		return count

	def add_option(self, *k, **kw):
		if 'type' in kw and type(kw['type']) == str:
			Logs.warn('Invalid "type=str" in add_option (must be a class, not a string)')
			if kw['type'] == 'int':
				kw['type'] = int
			elif kw['type'] == 'string':
				kw['type'] = str
		return self.add_argument(*k, **kw)

	def add_argument(self, *k, **kw):
		"""
		Wraps ``argparse.add_argument``::

			def options(ctx):
				ctx.add_option('-u', '--use', dest='use', default=False,
					action='store_true', help='a boolean option')

		:rtype: argparse option object
		"""
		return self.parser.add_argument(*k, **kw)

	def add_option_group(self, *k, **kw):
		"""
		Wraps ``optparse.add_option_group``::

			def options(ctx):
				gr = ctx.add_option_group('some options')
				gr.add_option('-u', '--use', dest='use', default=False, action='store_true')

		:rtype: optparse option group object
		"""
		gr = self.get_option_group(k[0])
		if not gr:
			gr = self.parser.add_argument_group(*k, **kw)
			gr.add_option = gr.add_argument
			self.option_groups[k[0]] = gr
		return gr

	def get_option_group(self, opt_str):
		"""
		Wraps ``optparse.get_option_group``::

			def options(ctx):
				gr = ctx.get_option_group('configure options')
				gr.add_option('-o', '--out', action='store', default='',
					help='build dir for the project', dest='out')

		:rtype: optparse option group object
		"""
		try:
			return self.option_groups[opt_str]
		except KeyError:
			for group in self.parser._action_groups:
				if group.title == opt_str:
					return group
			return None

	def sanitize_path(self, path, cwd=None):
		if not cwd:
			cwd = Context.launch_dir
		p = os.path.expanduser(path)
		p = os.path.join(cwd, p)
		p = os.path.normpath(p)
		p = os.path.abspath(p)
		return p

	def parse_cmd_args(self, _args=None, cwd=None, allow_unknown=False):
		"""
		Just parse the arguments
		"""
		(options, leftover_args) = self.parser.parse_known_args(args=_args)
		commands = []
		for arg in leftover_args:
			if not allow_unknown and arg.startswith('-'):
				self.parser.print_help()
				raise Errors.WafError('Unknown option: %r' % arg)
			commands.append(arg)

		if options.jobs < 1:
			options.jobs = 1
		for name in 'top out destdir prefix bindir libdir'.split():
			# those paths are usually expanded from Context.launch_dir
			if getattr(options, name, None):
				path = self.sanitize_path(getattr(options, name), cwd)
				setattr(options, name, path)
		return options, commands

	def init_logs(self, options, commands):
		Logs.verbose = options.verbose
		if options.verbose >= 1:
			self.load('errcheck')

		colors = {'yes' : 2, 'auto' : 1, 'no' : 0}[options.colors]
		Logs.enable_colors(colors)

		if options.zones:
			Logs.zones = options.zones.split(',')
			if not Logs.verbose:
				Logs.verbose = 1
		elif Logs.verbose > 0:
			Logs.zones = ['runner']
		if Logs.verbose > 2:
			Logs.zones = ['*']

	def parse_args(self, _args=None):
		"""
		Parses arguments from a list which is not necessarily the command-line.
		Initializes the module variables options and commands
		If help is requested, prints it and exit the application

		:param _args: arguments
		:type _args: list of strings
		"""
		arg_options, arg_commands = self.parse_cmd_args(_args)
		self.init_logs(arg_options, commands)

		options.__dict__.clear()
		del commands[:]

		options.__dict__.update(arg_options.__dict__)
		commands.extend(arg_commands)

	def execute(self):
		"""
		See :py:func:`waflib.Context.Context.execute`
		"""
		super(OptionsContext, self).execute()
		self.parse_args()
		Utils.alloc_process_pool(options.jobs)
