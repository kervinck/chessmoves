from distutils.core import setup, Extension

module1 = Extension(
        'chessmoves',
        sources = [
                'Source/chessmovesmodule.c',
                'Source/Board.c',
                'Source/readLine.c',
                'Source/stringCopy.c'])

setup(
        name         = 'chessmoves',
        version      = '1.0x',
        description  = 'Package to generate chess positions and moves (FEN/SAN/UCI)',
        author       = 'Marcel van Kervinck',
        author_email = 'marcelk@bitpit.net',
        url          = 'http://marcelk.net/chessmoves',
        ext_modules  = [module1])
