from distutils.core import setup, Extension

polygon_module = Extension('polygon_neighbours',
                           sources = ['src/polygon_neighbours/find_neighbours.cpp'])

long_description = """Various CPython extensions for manipulating and analysing
polygons.
"""

setup (name = 'polygon_neighbours',
       version = '1.0',
       description = 'CPython extension for finding bordering polygons.',
       url = 'https://github.com/sgaebel/polygon_neighbours',
       author = 'Dr. Sebastian M. Gaebel',
       author_email = 'gaebel.sebastian@gmail.com',
       license = 'MIT',
       ext_modules = [polygon_module],
       long_description = long_description,
       install_requires = ['numpy'],
       classifiers = [
              'Development Status :: 5 - Production/Stable',
              'Intended Audience :: Developers',
              'Topic :: Scientific/Engineering :: Mathematics',
              'License :: OSI Approved :: MIT License',
              'Programming Language :: Python :: 3.10',
              'Operating System :: POSIX :: Linux'],
       keywords = 'geometry polygon',
       python_requires = '>=3.10')
