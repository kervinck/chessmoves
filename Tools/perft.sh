#!/bin/sh
perft() {
        [ $1 -le 0 ] && wc -l && exit
        chessmoves | grep ^move, | cut -d, -f3- | perft `expr $1 - 1`
}
perft ${1:-1}
