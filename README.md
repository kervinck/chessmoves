Chessmoves
==========
Chess move and position (FEN/SAN/UCI) generation in C for use in scripting
applications.

`Chessmoves' is a small module to make fast chess tree traversal available in a
scripting environment, such as Python or sh.

In the core this is a set of C functions for chess move making, wrapped for
easy use. All input and output is in the form of FEN strings for positions.
SAN notation for moves is made available as well. The FEN output is strictly
sanitized. For example, no fake en-passant target square flag is produced
when there is no such legal capture in the position in the first place.

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
        Available notations are:
            'san': Standard Algebraic Notation (e.g. Nc3+, O-O, dxe8=Q)
            'long': Long Algebraic Notation (e.g. Nb1-c3+, O-O, d7xe8=Q)
            'uci': Universal Chess Interface computer notation (e.g. b1c3, e8g8, d7e8q)
    
    position(...)
        position(inputFen) -> standardFen
        
        Parse a FEN like string and convert it into a standardized FEN.
        For example:
         - Complete shortened ranks
         - Order castling flags
         - Remove en passant target square if there is no such legal capture
         - Remove excess data beyond the FEN

DATA
    notations = ['uci', 'san', 'long']
    startPosition = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - ...
```

Example of the Python extension:
```
>>> import chessmoves
>>> moves = chessmoves.moves(chessmoves.startPosition)
>>> print moves.keys()
['g3', 'f3', 'f4', 'h3', 'Nc3', 'h4', 'b4', 'Na3', 'a3', 'g4', 'Nf3', 'a4', 'Nh3', 'b3', 'c3', 'e4', 'd4', 'd3', 'e3', 'c4']
>>> print moves['e4']
rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq -
```
Performance of the Python extension:
```
$ echo rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - | time python Tools/perft.py 5
4865609
        3.08 real         3.06 user         0.00 sys
# --> results per second: 1,579,743
```
