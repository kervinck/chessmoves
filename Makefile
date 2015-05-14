CFLAGS=-std=c99 -pedantic -Wall -O3

all: chessmoves module

# python module
module:
	python setup.py build

# command line tool
chessmoves:
	$(CC) $(CFLAGS) -o chessmoves Source/chessmoves.c Source/stringCopy.c Source/Board.c Source/readLine.c

test:
	echo rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - | Tools/perft.sh 5
	env PATH=.:Tools:$$PATH Tools/run-perft 4 < Data/perft-random.epd

install:
	python setup.py install --user

clean:
	python setup.py clean
	rm -f chessmoves

# vi: noexpandtab
