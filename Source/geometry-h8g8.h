
/*
 *  Example of alternative board geometry
 */

enum file { fileH, fileG, fileF, fileE, fileD, fileC, fileB, fileA };
enum rank { rank8, rank7, rank6, rank5, rank4, rank3, rank2, rank1 };

#define file(square)       ((square) & 7)
#define rank(square)       ((square) >> 3)
#define square(file, rank) (((rank) << 3) + (file))

enum square {
        h8, g8, f8, e8, d8, c8, b8, a8,
        h7, g7, f7, e7, d7, c7, b7, a7,
        h6, g6, f6, e6, d6, c6, b6, a6,
        h5, g5, f5, e5, d5, c5, b5, a5,
        h4, g4, f4, e4, d4, c4, b4, a4,
        h3, g3, f3, e3, d3, c3, b3, a3,
        h2, g2, f2, e2, d2, c2, b2, a2,
        h1, g1, f1, e1, d1, c1, b1, a1,
        boardSize
};

enum { boardBits = 6 };

