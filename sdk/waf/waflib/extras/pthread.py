#! /usr/bin/env python
# encoding: UTF-8
# Thomas Nagy 2020 (ita)

from waflib import Utils
from waflib.Configure import conf

PTHREAD_CHECK = '''
#include <pthread.h>

static void* fun(void* params) {
    (void)params;
    return NULL;
}

int main(int argc, char **argv) {
    pthread_t thread;
    (void)argc;
    (void)argv;
    pthread_create(&thread, NULL, &fun, NULL);
    pthread_join(thread, NULL);
    return 0;
}
'''

@conf
def check_pthreads(self, mode=None):
	if not mode:
		mode = 'cxx' if self.env.CXX else 'c'

	if Utils.unversioned_sys_platform() == 'sunos':
		flags = ['-pthreads', '-lpthread', '-mt', '-pthread']
	else:
		flags = ['', '-lpthreads', '-Kthread', '-kthread', '-llthread', '-pthread', '-pthreads', '-mthreads', '-lpthread', '--thread-safe', '-mt']

	features = mode
	for flag in flags:
		self.env.stash()

		self.env[mode.upper() + 'FLAGS_PTHREAD'] = [flag]

		if flag:
			msg = ' -> Trying pthread compilation flag %s' % flag
			okmsg = 'needs %s' % flag
		else:
			msg = 'Checking if a pthread flag is necessary for compiling'
			okmsg = 'None'

		try:
			self.check(features=features, msg=msg, okmsg=okmsg, use='PTHREAD', fragment=PTHREAD_CHECK)
		except self.errors.ConfigurationError:
			self.env.revert()
			continue
		else:
			break
	else:
		self.fatal('Could not find a suitable pthreads flag for compiling')

	features = '%s %sprogram' % (mode, mode)
	for flag in flags:
		self.env.stash()

		self.env.LINKFLAGS_PTHREAD = [flag]

		if flag:
			msg = ' -> Trying pthread link flag %s' % flag
			okmsg = 'needs %s' % flag
		else:
			msg = 'Checking if a pthread flag is necessary for linking'
			okmsg = 'None'

		try:
			self.check(features=features, msg=msg, okmsg=okmsg, use='PTHREAD', fragment=PTHREAD_CHECK)
		except self.errors.ConfigurationError:
			self.env.revert()
			continue
		else:
			break
	else:
		self.fatal('Could not find a suitable pthreads flag for linking')


def configure(self):
	self.check_pthreads()

