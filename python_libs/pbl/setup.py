__author__ = 'katharine'

import sys
from setuptools import setup, find_packages

requires = [
    'pebble_tool==4.4-dp2',
    'libpebble2[pulse]>=0.0.23',
]

if sys.version_info < (3, 4, 0):
    requires.append('enum34==1.0.4')

__version__ = None  # Overwritten by executing version.py.
with open('pbl/version.py') as f:
    exec(f.read())

setup(name='pbl-tool',
      version=__version__,
      description='Internal tool for interacting with pebbles.',
      url='https://github.com/pebble/pbl',
      author='Pebble Technology Corporation',
      author_email='katharine@pebble.com',
      license='MIT',
      packages=find_packages(),
      install_requires=requires,
      entry_points={
          'console_scripts': ['pbl=pbl:run_tool'],
      },
      zip_safe=False)
