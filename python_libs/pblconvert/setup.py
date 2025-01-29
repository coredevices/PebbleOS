from setuptools import setup, find_packages

__version__= None  # Overwritten by executing version.py.
with open('pblconvert/version.py') as f:
    exec(f.read())

requires = [
    'lxml==3.4.4',
    'cairosvg',
    'mock',
    'Pillow',
]

setup(name='pblconvert',
      version=__version__,
      description='Tools to convert files into valid Pebble resources',
      long_description=open('README.rst').read(),
      url='https://github.com/pebble/pblconvert',
      author='Pebble Technology Corporation',
      license='MIT',
      packages=find_packages(exclude=['tests', 'tests.*']),
      package_data={'': ['bin/*']},
      entry_points={'console_scripts': ['pblconvert = pblconvert.pblconvert:main']},
      install_requires=requires,
      test_suite='nose.collector',
)