#!/usr/bin/env python
from distutils.core import setup, Extension
import glob
import numpy

extensions = [
    Extension('recv._recv',
        sources=['spead2/recv/recv_py.cpp'] + glob.glob('src/recv*.cpp') + glob.glob('src/common*.cpp'),
        language='c++',
        include_dirs=['src', numpy.get_include()],
        extra_compile_args=['-std=c++11'],
        libraries=['boost_python-py27', 'boost_system'])
]

setup(
    name='spead2',
    version='0.1dev',
    description='High-performance SPEAD decoder',
    ext_package='spead2',
    ext_modules=extensions)
