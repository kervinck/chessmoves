#!/usr/bin/env python

import chessmoves
import sys

def perft(pos, depth):
        moveList = chessmoves.moves(pos, notation='uci')
        if depth == 1:
               return len(moveList)
        else:
               return sum( [perft(newPos, depth-1) for newPos in moveList.values()] )

if __name__ == '__main__':
        if len(sys.argv) == 2:
                depth = int(sys.argv[1])
                assert depth >= 1
        else:
                depth = 1

        for line in sys.stdin:
                print perft(line, depth)

