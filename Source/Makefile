CFLAGS=-std=c99 -pedantic -Wall -O3

chessmoves:

clean:
	rm -f chessmoves

test:
	env PATH=.:$$PATH ../Tools/run-perft 3 < ../Data/perft-random.epd

# vi: noexpandtab
