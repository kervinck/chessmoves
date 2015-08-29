from distutils.core import setup, Extension

module1 = Extension(
        'chessmoves',
        sources = [
                'Source/chessmovesmodule.c',
                'Source/Board-format.c',
                'Source/Board-moves.c',
                'Source/polyglot.c',
                'Source/stringCopy.c' ],
        extra_compile_args = ['-O3', '-std=c99', '-Wall', '-pedantic'],
        undef_macros = ['NDEBUG']
)

setup(
        name         = 'chessmoves',
        version      = '1.0a',
        description  = 'Package to generate chess positions and moves (FEN/SAN/UCI)',
        author       = 'Marcel van Kervinck',
        author_email = 'marcelk@bitpit.net',
        url          = 'http://marcelk.net/chessmoves',
        ext_modules  = [module1])
