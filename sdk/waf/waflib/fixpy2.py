#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2010-2018 (ita)

import os

all_modifs = {}

def fixdir(dir):
	"""Call all substitution functions on Waf folders"""
	for y in '. Tools extras'.split():
		for x in os.listdir(os.path.join(dir, 'waflib', y)):
			if x.endswith('.py'):
				filename = os.path.join(dir, 'waflib', y, x)
				update(filename)

def update(filename):
	with open(filename, 'r') as f:
		txt = f.read()

	txt = txt.replace(".decode(sys.stdout.encoding or'latin-1',errors='replace')", '')
	txt = txt.replace('.encode()', '')
	txt = txt.replace('class Task(metaclass=store_task_type):', "class Task(object):%s\t__metaclass__=store_task_type" % os.linesep)

	with open(filename, 'w') as f:
		f.write(txt)

	for k in all_modifs:
		for v in all_modifs[k]:
			modif(os.path.join(dir, 'waflib'), k, v)
