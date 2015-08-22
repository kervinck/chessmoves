CFLAGS=-std=c99 -pedantic -Wall -O3

all: module

# python module
module:
	python setup.py build

test:
	python Tools/quicktest.py
	echo rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - | time python Tools/perft.py 5
	env PATH=.:Tools:$$PATH Tools/run-perft 4 < Data/perft-random.epd

install:
	python setup.py install --user

clean:
	python setup.py clean

# vi: noexpandtab
