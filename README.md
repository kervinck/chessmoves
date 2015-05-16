Chessmoves
==========
Chess move and position (SAN/FEN) generation in C for use in scripting applications.

`Chessmoves' is a small kit to make fast chess tree traversal available in a
scripting environment, such as Python or sh.

In the core this is a set of C functions for chess move making, wrapped for
easy use. All input and output is in the form of FEN strings for positions.
SAN notation for moves is made available as well.

The FEN output is stricty sanitized. For example, no fake en-passant target
square flag is produced when there is no such legal capture in the position
in the first place.

The output is comma-separated by default and designed to be easy to
process by other text processing programs.

Python module interface:
------------------------

```
NAME
    chessmoves - Chess move and position generation (SAN/FEN/UCI).

FUNCTIONS
    moves(...)
        moves(position, notation='san') -> { move : newPosition, ... }
        
        Generate all legal moves from a position.
        Return the result as a dictionary, mapping moves to positions.
        
        The `notation' keyword controls the output move syntax.
        Available are:
         - san:  Standard Algebraic Notation (e.g. Nc3+, O-O, dxe8=Q)
         - long: Long Algebraic Notation (e.g. Nb1-c3+, O-O, d7xe8=Q)
         - uci:  Universal Chess Interface computer notation (e.g. b1c3, e8g8, d7e8q)
    
    position(...)
        position(inputFen) -> standardFen
        
        Parse a FEN like string and convert it into a standardized FEN.
        For example:
         - Complete short ranks
         - Order castling flags
         - Remove en passant target square if there is no such legal capture
         - Remove excess data beyond the FEN

DATA
    notations = ['uci', 'san', 'long']
    startPosition = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - ...
```

Example:

```
$ cat in.epd
rnbqk2r/ppp1bppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 b kq -
7r/5P2/3k4/4N1Kb/8/8/8/8 w - -
$ chessmoves < in.epd
fen,rnbqk2r/ppp1bppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 b kq -
move,Ba3,rnbqk2r/ppp2ppp/4pn2/3p4/2P5/b4NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Bb4,rnbqk2r/ppp2ppp/4pn2/3p4/1bP5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Bc5,rnbqk2r/ppp2ppp/4pn2/2bp4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Bd6,rnbqk2r/ppp2ppp/3bpn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Bd7,rn1qk2r/pppbbppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Bf8,rnbqkb1r/ppp2ppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Kd7,rnbq3r/pppkbppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w - -
move,Kf8,rnbq1k1r/ppp1bppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w - -
move,Na6,r1bqk2r/ppp1bppp/n3pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Nbd7,r1bqk2r/pppnbppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Nc6,r1bqk2r/ppp1bppp/2n1pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Ne4,rnbqk2r/ppp1bppp/4p3/3p4/2P1n3/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Nfd7,rnbqk2r/pppnbppp/4p3/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Ng4,rnbqk2r/ppp1bppp/4p3/3p4/2P3n1/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Ng8,rnbqk1nr/ppp1bppp/4p3/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Nh5,rnbqk2r/ppp1bppp/4p3/3p3n/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,O-O,rnbq1rk1/ppp1bppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w - -
move,Qd6,rnb1k2r/ppp1bppp/3qpn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Qd7,rnb1k2r/pppqbppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,Rf8,rnbqkr2/ppp1bppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w q -
move,Rg8,rnbqk1r1/ppp1bppp/4pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w q -
move,a5,rnbqk2r/1pp1bppp/4pn2/p2p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,a6,rnbqk2r/1pp1bppp/p3pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,b5,rnbqk2r/p1p1bppp/4pn2/1p1p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,b6,rnbqk2r/p1p1bppp/1p2pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,c5,rnbqk2r/pp2bppp/4pn2/2pp4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,c6,rnbqk2r/pp2bppp/2p1pn2/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,d4,rnbqk2r/ppp1bppp/4pn2/8/2Pp4/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,dxc4,rnbqk2r/ppp1bppp/4pn2/8/2p5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,e5,rnbqk2r/ppp1bppp/5n2/3pp3/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,g5,rnbqk2r/ppp1bp1p/4pn2/3p2p1/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,g6,rnbqk2r/ppp1bp1p/4pnp1/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,h5,rnbqk2r/ppp1bpp1/4pn2/3p3p/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
move,h6,rnbqk2r/ppp1bpp1/4pn1p/3p4/2P5/5NP1/PP1PPPBP/RNBQ1RK1 w kq -
end
fen,7r/5P2/3k4/4N1Kb/8/8/8/8 w - -
move,Kf4,7r/5P2/3k4/4N2b/5K2/8/8/8 b - -
move,Kf5,7r/5P2/3k4/4NK1b/8/8/8/8 b - -
move,Kf6,7r/5P2/3k1K2/4N2b/8/8/8/8 b - -
move,Kh4,7r/5P2/3k4/4N2b/7K/8/8/8 b - -
move,Nc4+,7r/5P2/3k4/6Kb/2N5/8/8/8 b - -
move,Nc6,7r/5P2/2Nk4/6Kb/8/8/8/8 b - -
move,Nd3,7r/5P2/3k4/6Kb/8/3N4/8/8 b - -
move,Nd7,7r/3N1P2/3k4/6Kb/8/8/8/8 b - -
move,Nf3,7r/5P2/3k4/6Kb/8/5N2/8/8 b - -
move,Ng4,7r/5P2/3k4/6Kb/6N1/8/8/8 b - -
move,Ng6,7r/5P2/3k2N1/6Kb/8/8/8/8 b - -
move,f8=B+,5B1r/8/3k4/4N1Kb/8/8/8/8 b - -
move,f8=N,5N1r/8/3k4/4N1Kb/8/8/8/8 b - -
move,f8=Q+,5Q1r/8/3k4/4N1Kb/8/8/8/8 b - -
move,f8=R,5R1r/8/3k4/4N1Kb/8/8/8/8 b - -
end
$
```

Another example, making a 'perft' counter in shell script:

```
$ cat perft.sh
#!/bin/sh
startpos='rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -'
perft() {
        [ $1 -le 0 ] && wc -l && exit
        chessmoves | grep ^move, | cut -d, -f3- | perft `expr $1 - 1`
}
echo $startpos | perft ${1:-1}

$ time perft.sh 5
 4865609
31.410 user, 0.803 sys, 0m20.35s real (100.00% cpu)

$ python -c 'print 4865609 / 20.35' # --> results per second
239096.265356
```

