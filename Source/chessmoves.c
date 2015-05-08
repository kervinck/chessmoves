
/*----------------------------------------------------------------------+
 |                                                                      |
 |      chessmoves.c                                                    |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*
 *  chessmoves -- list all chess moves and resulting positions from FENs
 *
 *  Description:
 *      Input are chess positions in FEN/EPD format.
 *      For each position, print all moves in SAN and the resulting FEN.
 *
 *  Example:
 *      chessmoves < in.epd > out.csv
 *
 *  History:
 *      2015-05-05 (marcelk)
 *          Initial version based on move2san.c (which was based on MSCP).
 *      2015-05-06 (marcelk)
 *          Cleanup for better readability and no more global variables.
 *      2015-05-07 (marcelk)
 *          Fix refactoring bug in SAN disambiguation. Some more cleanup.
 *      2015-05-08 (marcelk)
 *          More cleanup.
 *
 *  TODO: deploy for bookie-update.service
 *  TODO: cleanup: split chess and main
 *  TODO: cleanup: memory for stacks?
 *  TODO: make python extension interface. Return list of tuples
 *  TODO: cleanup: add descriptions to all functions
 *  TODO: make function call order dependencies easier to understand
 *        (= rethinking functions with old naming)
 *  TODO: cleanup: error handling
 *  TODO: option to choose delimiter (for example: -d,). default ' '
 *  TODO: consider profiling
 *  TODO: consider speedup with quick_makeMove in isLegalMove
 *  TODO: handle 6-field FEN, detect automatically
 *  TODO: add notation options (simple algebraic, uci, ics, san, san+, san, ...)
 *  TODO: add normalization options (w/b, x, y, xy)
 *  TODO: accept sloppy FENs with incomplete data, for example "k///////K w"
 *  TODO: add XFEN support
 */

/*----------------------------------------------------------------------+
 |      Copyright                                                       |
 +----------------------------------------------------------------------*/

/*
 *  Copyright (C)1998-2015 by Marcel van Kervinck
 *  All rights reserved
 */

/*----------------------------------------------------------------------+
 |      Includes                                                        |
 +----------------------------------------------------------------------*/

/*
 *  Standard includes
 */

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  Own includes
 */

#include "geometry-a1a2.h"

// Everything below here ought to be reasonably geometry-agnostic

/*----------------------------------------------------------------------+
 |      Definitions                                                     |
 +----------------------------------------------------------------------*/

/*
 *  Square notation
 */

#define rankToChar(rank)        ('1'   + rankStep * ((rank) - rank1))
#define charToRank(c)           (rank1 + rankStep * ((c) - '1'))
#define fileToChar(file)        ('a'   + fileStep * ((file) - fileA))
#define charToFile(c)           (fileA + fileStep * ((c) - 'a'))

enum { rankStep = rank2 - rank1, fileStep = fileB - fileA }; // don't confuse with stepN, stepE

/*
 *  Chess pieces
 */

enum piece {
        empty,
        whiteKing, whiteQueen, whiteRook, whiteBishop, whiteKnight, whitePawn,
        blackKing, blackQueen, blackRook, blackBishop, blackKnight, blackPawn
};

enum pieceColor { white = 0, black = 1 };

#define pieceColor(piece) ((piece) >= blackKing) // piece must not be 'empty'
/*
 *  Moves
 *
 *  move integer bits are as follows:
 *  0-5         to square
 *  6-11        from square
 *  12          special flag (castling, promotion, en passant capture, double pawn push)
 *  13-14       promotion: Q=0, R=1, B=2, N=3
 */

#define move(from, to)          (((from) << boardBits) | (to))
enum moveFlags {
        specialMoveFlag         = 1 << (2*boardBits),
        promotionBits           = 2*boardBits + 1,
        queenPromotionFlags     = 0 << promotionBits,
        rookPromotionFlags      = 1 << promotionBits,
        bishopPromotionFlags    = 2 << promotionBits,
        knightPromotionFlags    = 3 << promotionBits
};
#define specialMove(from, to)   (specialMoveFlag | move(from, to))

#define from(move)              (((move) >> boardBits) & ~(~0<<boardBits))
#define to(move)                ( (move)               & ~(~0<<boardBits))

/*
 *  Move and attack directions
 */

enum {
        stepN = a2 - a1,          stepE = b1 - a1,
        stepS = -stepN,           stepW = -stepE,
        stepNE = stepN + stepE,   stepSE = stepS + stepE,
        stepSW = stepS + stepW,   stepNW = stepN + stepW,
        jumpNNE = stepN + stepNE, jumpENE = stepE + stepNE,
        jumpESE = stepE + stepSE, jumpSSE = stepS + stepSE,
        jumpSSW = stepS + stepSW, jumpWSW = stepW + stepSW,
        jumpWNW = stepW + stepNW, jumpNNW = stepN + stepNW,
};

enum stepBit { bitN, bitNE, bitE, bitSE, bitS, bitSW, bitW, bitNW };
enum jumpBit { bitNNE, bitENE, bitESE, bitSSE, bitSSW, bitWSW, bitWNW, bitNNW };

enum {
        dirsRook   = (1 << bitN)  | (1 << bitE)  | (1 << bitS)  | (1 << bitW),
        dirsBishop = (1 << bitNE) | (1 << bitSE) | (1 << bitSW) | (1 << bitNW),
        dirsQueen  = dirsRook | dirsBishop
};

/*
 *  Game state
 */

struct side {
        signed char attacks[boardSize];
        int king;
};

enum castleFlag {
        castleFlagWhiteKside = 1 << 0,
        castleFlagWhiteQside = 1 << 1,
        castleFlagBlackKside = 1 << 2,
        castleFlagBlackQside = 1 << 3
};

typedef struct board *Board_t;

struct board {
        signed char squares[boardSize];

        signed char castleFlags;
        signed char enPassantPawn;
        //signed char lastZeroing; // TODO: change to halfmoveClock

        int plyNumber; // holds both side to move and full move number

        /*
         *  Side data
         */
        struct side *side, *xside;
        struct side whiteSide;
        struct side blackSide;

        /*
         *  Move undo administration
         */
        signed char undoStack[6*1024], *undo_sp; // TODO: get better sizes
        short moveStack[1024], *move_sp; // TODO: move to stack frames
};

// Off-board offsets for use in the undo stack
#define offsetof_castleFlags   offsetof(struct board, castleFlags)
#define offsetof_enPassantPawn offsetof(struct board, enPassantPawn)

#define sideToMove(board) ((board)->plyNumber & 1)

/*----------------------------------------------------------------------+
 |      Forward declarations                                            |
 +----------------------------------------------------------------------*/

static int readLine(FILE *fp, char **pLine, int *pMallocSize);
static char *stringCopy(char *s, const char *t);
static int compare_strcmp(const void *ap, const void *bp);

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

static const char pieceToChar[] = {
        [whiteKing]   = 'K', [whiteQueen]  = 'Q', [whiteRook] = 'R',
        [whiteBishop] = 'B', [whiteKnight] = 'N', [whitePawn] = 'P',
        [blackKing]   = 'k', [blackQueen]  = 'q', [blackRook] = 'r',
        [blackBishop] = 'b', [blackKnight] = 'n', [blackPawn] = 'p'
};

static const char promotionPieceToChar[] = { 'Q', 'R', 'B', 'N' };

/*
 *  Which castle bits to clear for a move's from and to
 */
static const char castleFlagsClear[boardSize] = {
        [a8] = castleFlagBlackQside,
        [e8] = castleFlagBlackKside | castleFlagBlackQside,
        [h8] = castleFlagBlackKside,
        [a1] = castleFlagWhiteQside,
        [e1] = castleFlagWhiteKside | castleFlagWhiteQside,
        [h1] = castleFlagWhiteKside,
};

static const signed char kingStep[] = { // Offsets for king moves
        [1<<bitN] = stepN, [1<<bitNE] = stepNE,
        [1<<bitE] = stepE, [1<<bitSE] = stepSE,
        [1<<bitS] = stepS, [1<<bitSW] = stepSW,
        [1<<bitW] = stepW, [1<<bitNW] = stepNW,
};

static const signed char knightJump[] = { // Offsets for knight jumps
        [1<<bitNNE] = jumpNNE, [1<<bitENE] = jumpENE,
        [1<<bitESE] = jumpESE, [1<<bitSSE] = jumpSSE,
        [1<<bitSSW] = jumpSSW, [1<<bitWSW] = jumpWSW,
        [1<<bitWNW] = jumpWNW, [1<<bitNNW] = jumpNNW,
};

/*
 *  Some 0x88 preprocessor logic to compute tables for king steps and knight jumps
 */

// Unsigned 0x88 for squares:
#define x88u(square) ((square) + ((square) & ~7))

// Sign-extended 0x88 for vectors:
#define x88s(vector) (x88u(vector) + (((vector) << 1) & 8))

// Move stays inside board?
#define onBoard(square, vector) (((x88u(square) + x88s(vector)) & 0x88) == 0)

// If so, turn it into a flag for in the directions table
#define dir(square, vector, bit) (onBoard(square, vector) << (bit))

// Collect king step flags
#define K(sq) [sq]=(\
        dir(sq,stepNW,bitNW)+  dir(sq,stepN,bitN)+  dir(sq,stepNE,bitNE)+\
                                                                         \
        dir(sq,stepW,bitW)             +              dir(sq,stepE,bitE)+\
                                                                         \
        dir(sq,stepSW,bitSW)+  dir(sq,stepS,bitS)+  dir(sq,stepSE,bitSE))

// Collect knight jump flags
#define N(sq) [sq]=(\
             dir(sq,jumpNNW,bitNNW)    +     dir(sq,jumpNNE,bitNNE)+        \
                                                                            \
  dir(sq,jumpWNW,bitWNW)   +           +           +  dir(sq,jumpENE,bitENE)\
                                                                            \
               +           +           +           +           +            \
                                                                            \
  dir(sq,jumpWSW,bitWSW)   +           +           +  dir(sq,jumpESE,bitESE)\
                                                                            \
            +dir(sq,jumpSSW,bitSSW)    +     dir(sq,jumpSSE,bitSSE))

// 8 bits per square representing which directions a king can step to
static const unsigned char kingDirections[] = {
        K(a1), K(a2), K(a3), K(a4), K(a5), K(a6), K(a7), K(a8),
        K(b1), K(b2), K(b3), K(b4), K(b5), K(b6), K(b7), K(b8),
        K(c1), K(c2), K(c3), K(c4), K(c5), K(c6), K(c7), K(c8),
        K(d1), K(d2), K(d3), K(d4), K(d5), K(d6), K(d7), K(d8),
        K(e1), K(e2), K(e3), K(e4), K(e5), K(e6), K(e7), K(e8),
        K(f1), K(f2), K(f3), K(f4), K(f5), K(f6), K(f7), K(f8),
        K(g1), K(g2), K(g3), K(g4), K(g5), K(g6), K(g7), K(g8),
        K(h1), K(h2), K(h3), K(h4), K(h5), K(h6), K(h7), K(h8),
};

// 8 bits per square representing which directions a knight can jump to
static const unsigned char knightDirections[] = {
        N(a1), N(a2), N(a3), N(a4), N(a5), N(a6), N(a7), N(a8),
        N(b1), N(b2), N(b3), N(b4), N(b5), N(b6), N(b7), N(b8),
        N(c1), N(c2), N(c3), N(c4), N(c5), N(c6), N(c7), N(c8),
        N(d1), N(d2), N(d3), N(d4), N(d5), N(d6), N(d7), N(d8),
        N(e1), N(e2), N(e3), N(e4), N(e5), N(e6), N(e7), N(e8),
        N(f1), N(f2), N(f3), N(f4), N(f5), N(f6), N(f7), N(f8),
        N(g1), N(g2), N(g3), N(g4), N(g5), N(g6), N(g7), N(g8),
        N(h1), N(h2), N(h3), N(h4), N(h5), N(h6), N(h7), N(h8),
};

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      setup_board                                                     |
 +----------------------------------------------------------------------*/

static int setup_board(Board_t self, char *fen)
{
        int len = 0;

        /*
         *  Squares
         */

        while (isspace(fen[len])) len++;

        int file = fileA, rank = rank8;
        int nrWhiteKings = 0, nrBlackKings = 0;
        memset(self->squares, empty, boardSize);
        while (rank != rank1 || file != fileH + fileStep) {
                int piece = empty;
                int count = 1;

                switch (fen[len]) {
                case 'K': piece = whiteKing; nrWhiteKings++; break;
                case 'Q': piece = whiteQueen; break;
                case 'R': piece = whiteRook; break;
                case 'B': piece = whiteBishop; break;
                case 'N': piece = whiteKnight; break;
                case 'P': piece = whitePawn; break;
                case 'k': piece = blackKing; nrBlackKings++; break;
                case 'r': piece = blackRook; break;
                case 'q': piece = blackQueen; break;
                case 'b': piece = blackBishop; break;
                case 'n': piece = blackKnight; break;
                case 'p': piece = blackPawn; break;
                case '/': rank -= rankStep; file = fileA; len++; continue;
                case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8':
                        count = fen[len] - '0';
                        break;
                default:
                        return 0; // FEN error
                }
                do {
                        self->squares[square(file,rank)] = piece;
                        file += fileStep;
                } while (--count && file != fileH + fileStep);
                len++;
        }
        if (nrWhiteKings != 1 || nrBlackKings != 1) return 0;

        /*
         *  Side to move
         */

        self->plyNumber = 2 + (fen[len+1] == 'b'); // 2 means full move number starts at 1
        //self->lastZeroing = self->plyNumber;
        len += 2;

        /*
         *  Castling flags
         */

        while (isspace(fen[len])) len++;

        self->castleFlags = 0;
        for (;; len++) {
                switch (fen[len]) {
                case 'K': self->castleFlags |= castleFlagWhiteKside; continue;
                case 'Q': self->castleFlags |= castleFlagWhiteQside; continue;
                case 'k': self->castleFlags |= castleFlagBlackKside; continue;
                case 'q': self->castleFlags |= castleFlagBlackQside; continue;
                case '-': len++; break;
                default: break;
                }
                break;
        }

        /*
         *  En passant square
         */

        while (isspace(fen[len])) len++;

        if (fen[len] == '-') {
                self->enPassantPawn = 0;
                len++;
        } else {
                file = charToFile(fen[len]);
                len++;

                rank = (sideToMove(self) == white) ? rank5 : rank4;
                if (isdigit(fen[len])) len++; // ignore what it says

                self->enPassantPawn = square(file, rank);
        }

        // Reset move stacks
        self->move_sp = self->moveStack;
        self->undo_sp = self->undoStack;

        return len;
}

/*----------------------------------------------------------------------+
 |      attack tables                                                   |
 +----------------------------------------------------------------------*/

static void atk_slide(Board_t self, int from, int dirs, struct side *side)
{
        dirs &= kingDirections[from];
        int dir = 0;
        do {
                dir -= dirs; // pick next
                dir &= dirs;
                int to = from;
                do {
                        to += kingStep[dir];
                        side->attacks[to] = 1;
                        if (self->squares[to] != empty) break;
                } while (dir & kingDirections[to]);
        } while (dirs -= dir); // remove and go to next
}

static void compute_attacks(Board_t self)
{
        memset(&self->whiteSide, 0, sizeof self->whiteSide);
        memset(&self->blackSide, 0, sizeof self->blackSide);

        self->side  = (sideToMove(self) == white) ? &self->whiteSide : &self->blackSide;
        self->xside = (sideToMove(self) == white) ? &self->blackSide : &self->whiteSide;

        for (int from=0; from<boardSize; from++) {
                int piece = self->squares[from];
                if (piece == empty) continue;

                switch (piece) {
                        int dir, dirs;

                case whiteKing:
                        dirs = kingDirections[from];
                        dir = 0;
                        do {
                                dir -= dirs; // pick next
                                dir &= dirs;
                                int to = from + kingStep[dir];
                                self->whiteSide.attacks[to] = 1;
                        } while (dirs -= dir); // remove and go to next
                        self->whiteSide.king = from;
                        break;

                case blackKing:
                        dirs = kingDirections[from];
                        dir = 0;
                        do {
                                dir -= dirs; // pick next
                                dir &= dirs;
                                int to = from + kingStep[dir];
                                self->blackSide.attacks[to] = 1;
                        } while (dirs -= dir); // remove and go to next
                        self->blackSide.king = from;
                        break;

                case whiteQueen:
                        atk_slide(self, from, dirsQueen, &self->whiteSide);
                        break;

                case blackQueen:
                        atk_slide(self, from, dirsQueen, &self->blackSide);
                        break;

                case whiteRook:
                        atk_slide(self, from, dirsRook, &self->whiteSide);
                        break;

                case blackRook:
                        atk_slide(self, from, dirsRook, &self->blackSide);
                        break;

                case whiteBishop:
                        atk_slide(self, from, dirsBishop, &self->whiteSide);
                        break;

                case blackBishop:
                        atk_slide(self, from, dirsBishop, &self->blackSide);
                        break;

                case whiteKnight:
                        dirs = knightDirections[from];
                        dir = 0;
                        do {
                                dir -= dirs; // pick next
                                dir &= dirs;
                                int to = from + knightJump[dir];
                                self->whiteSide.attacks[to] = 1;
                        } while (dirs -= dir); // remove and go to next
                        break;

                case blackKnight:
                        dirs = knightDirections[from];
                        dir = 0;
                        do {
                                dir -= dirs; // pick next
                                dir &= dirs;
                                int to = from + knightJump[dir];
                                self->blackSide.attacks[to] = 1;
                        } while (dirs -= dir); // remove and go to next
                        break;

                case whitePawn:
                        if (file(from) != fileH) {
                                self->whiteSide.attacks[from+stepNE] = 1;
                        }
                        if (file(from) != fileA) {
                                self->whiteSide.attacks[from+stepNW] = 1;
                        }
                        break;

                case blackPawn:
                        if (file(from) != fileH) {
                                self->blackSide.attacks[from+stepSE] = 1;
                        }
                        if (file(from) != fileA) {
                                self->blackSide.attacks[from+stepSW] = 1;
                        }
                        break;
                }
        }
}

/*----------------------------------------------------------------------+
 |      make/unmake move                                                |
 +----------------------------------------------------------------------*/

static void undoMove(Board_t self)
{
        signed char *bytes = (signed char *)self;
        signed char *sp = self->undo_sp;
        for (;;) {
                int offset = *--sp;
                if (offset < 0) break;          // Found sentinel
                bytes[offset] = *--sp;
        }
        self->undo_sp = sp;
        self->plyNumber--;
}

static void makeMove(Board_t self, int move)
{
        signed char *sp = self->undo_sp;

        #define push(offset, value) do{                         \
                *sp++ = (value);                                \
                *sp++ = (offset);                               \
        }while(0)

        #define makeSimpleMove(from, to) do{                    \
                push(to, self->squares[to]);                    \
                push(from, self->squares[from]);                \
                self->squares[to] = self->squares[from];        \
                self->squares[from] = empty;                    \
        }while(0)

        *sp++ = -1;                             // Place sentinel

        if (self->enPassantPawn) {              // Always clear en-passant info
                push(offsetof_enPassantPawn, self->enPassantPawn);
                self->enPassantPawn = 0;
        }

        int to   = to(move);
        int from = from(move);

        if (move & specialMoveFlag) {           // Handle specials first
                switch (rank(from)) {
                case rank8:
                        // Black castles. Insert the corresponding rook move
                        if (to == g8) {
                                makeSimpleMove(h8, f8);
                        } else {
                                makeSimpleMove(a8, d8);
                        }
                        break;

                case rank7:
                        if (self->squares[from] == blackPawn) { // Set en-passant flag
                                push(offsetof_enPassantPawn, 0);
                                self->enPassantPawn = to;
                        } else {
                                // White promotes
                                push(from, self->squares[from]);
                                self->squares[from] = whiteQueen + (move >> promotionBits);
                        }
                        break;

                case rank5:                    // White captures en-passant
                case rank4:                    // Black captures en-passant
                        ;
                        int square = square(file(to), rank(from));
                        push(square, self->squares[square]);
                        self->squares[square] = empty;
                        break;

                case rank2:
                        if (self->squares[from] == whitePawn) { // Set en-passant flag
                                push(offsetof_enPassantPawn, 0);
                                self->enPassantPawn = to;
                        } else {
                                // Black promotes
                                push(from, self->squares[from]);
                                self->squares[from] = blackQueen + (move >> promotionBits);
                        }
                        break;

                case rank1:
                        // White castles. Insert the corresponding rook move
                        if (to == g1) {
                                makeSimpleMove(h1, f1);
                        } else {
                                makeSimpleMove(a1, d1);
                        }
                        break;

                default:
                        break;
                }
        }

        self->plyNumber++;

#if 0 // lastZeroing
        if (self->squares[to] != empty
         || self->squares[from] == whitePawn
         || self->squares[from] == blackPawn
        ) {
                push(offsetof_lastZeroing, self->lastZeroing);
                self->lastZeroing = self->plyNumber;
        }
#endif

        makeSimpleMove(from, to);

        int flagsToClear = castleFlagsClear[from] | castleFlagsClear[to];
        if (self->castleFlags & flagsToClear) {
                push(offsetof_castleFlags, self->castleFlags);
                self->castleFlags &= ~flagsToClear;
        }

        self->undo_sp = sp;
}

/*----------------------------------------------------------------------+
 |      move generator                                                  |
 +----------------------------------------------------------------------*/

static void pushMove(Board_t self, int from, int to)
{
        *self->move_sp++ = move(from, to);
}

static void pushSpecialMove(Board_t self, int from, int to)
{
        *self->move_sp++ = specialMove(from, to);
}

static void pushPawnMove(Board_t self, int from, int to)
{
        if (rank(to) == rank8 || rank(to) == rank1) {
                pushSpecialMove(self, from, to);
                self->move_sp[-1] += queenPromotionFlags;

                pushSpecialMove(self, from, to);
                self->move_sp[-1] += rookPromotionFlags;

                pushSpecialMove(self, from, to);
                self->move_sp[-1] += bishopPromotionFlags;

                pushSpecialMove(self, from, to);
                self->move_sp[-1] += knightPromotionFlags;
        } else {
                pushMove(self, from, to); // normal pawn move
        }
}

static void generateSlides(Board_t self, int from, int dirs)
{
        dirs &= kingDirections[from];
        int dir = 0;
        do {
                dir -= dirs; // pick next
                dir &= dirs;
                int vector = kingStep[dir];
                int to = from;
                do {
                        to += vector;
                        if (self->squares[to] != empty) {
                                if (pieceColor(self->squares[to]) != sideToMove(self)) {
                                        pushMove(self, from, to);
                                }
                                break;
                        }
                        pushMove(self, from, to);
                } while (dir & kingDirections[to]);
        } while (dirs -= dir); // remove and go to next
}

static int isLegalMove(Board_t self, int move)
{
        makeMove(self, move);
        compute_attacks(self);
        int legal = (self->side->attacks[self->xside->king] == 0);
        undoMove(self);
        return legal;
}

static int inCheck(Board_t self)
{
        return self->xside->attacks[self->side->king] != 0;
}

static void generate_moves(Board_t self)
{
        for (int from=0; from<boardSize; from++) {
                int piece = self->squares[from];
                if (piece == empty || pieceColor(piece) != sideToMove(self)) continue;

                int to;

                /*
                 *  Generate moves for this piece
                 */
                switch (piece) {
                        int dir, dirs;

                case whiteKing:
                case blackKing:
                        dirs = kingDirections[from];
                        dir = 0;
                        do {
                                dir -= dirs; // pick next
                                dir &= dirs;
                                to = from + kingStep[dir];
                                if (self->squares[to] != empty
                                 && pieceColor(self->squares[to]) == sideToMove(self)) continue;
                                pushMove(self, from, to);
                        } while (dirs -= dir); // remove and go to next
                        break;

                case whiteQueen:
                case blackQueen:
                        generateSlides(self, from, dirsQueen);
                        break;

                case whiteRook:
                case blackRook:
                        generateSlides(self, from, dirsRook);
                        break;

                case whiteBishop:
                case blackBishop:
                        generateSlides(self, from, dirsBishop);
                        break;

                case whiteKnight:
                case blackKnight:
                        dirs = knightDirections[from];
                        dir = 0;
                        do {
                                dir -= dirs; // pick next
                                dir &= dirs;
                                to = from + knightJump[dir];
                                if (self->squares[to] != empty
                                 && pieceColor(self->squares[to]) == sideToMove(self)) {
                                        continue;
                                }
                                pushMove(self, from, to);
                        } while (dirs -= dir); // remove and go to next
                        break;

                case whitePawn:
                        if (file(from) != fileH) {
                                to = from + stepNE;
                                if (self->squares[to] >= blackKing) {
                                        pushPawnMove(self, from, to);
                                }
                        }
                        if (file(from) != fileA) {
                                to = from + stepNW;
                                if (self->squares[to] >= blackKing) {
                                        pushPawnMove(self, from, to);
                                }
                        }
                        to = from + stepN;
                        if (self->squares[to] != empty) {
                                break;
                        }
                        pushPawnMove(self, from, to);
                        if (rank(from) == rank2) {
                                to += stepN;
                                if (self->squares[to] == empty) {
                                        pushMove(self, from, to);
                                        if (self->blackSide.attacks[to+stepS]) {
                                                self->move_sp[-1] |= specialMoveFlag;
                                        }
                                }
                        }
                        break;

                case blackPawn:
                        if (file(from) != fileH) {
                                to = from + stepSE;
                                if (self->squares[to] && self->squares[to] < blackKing) {
                                        pushPawnMove(self, from, to);
                                }
                        }
                        if (file(from) != fileA) {
                                to = from + stepSW;
                                if (self->squares[to] && self->squares[to] < blackKing) {
                                        pushPawnMove(self, from, to);
                                }
                        }
                        to = from + stepS;
                        if (self->squares[to] != empty) {
                                break;
                        }
                        pushPawnMove(self, from, to);
                        if (rank(from) == rank7) {
                                to += stepS;
                                if (self->squares[to] == empty) {
                                        pushMove(self, from, to);
                                        if (self->whiteSide.attacks[to+stepN]) {
                                                self->move_sp[-1] |= specialMoveFlag;
                                        }
                                }
                        }
                        break;
                }
        }

        /*
         *  Generate castling moves
         */
        if (self->castleFlags && !inCheck(self)) {
                if (sideToMove(self) == white) {
                        if ((self->castleFlags & castleFlagWhiteKside)
                         && self->squares[f1] == empty
                         && self->squares[g1] == empty
                         && !self->xside->attacks[f1]
                        ) {
                                pushSpecialMove(self, e1, g1);
                        }
                        if ((self->castleFlags & castleFlagWhiteQside)
                         && self->squares[d1] == empty
                         && self->squares[c1] == empty
                         && self->squares[b1] == empty
                         && !self->xside->attacks[d1]
                        ) {
                                pushSpecialMove(self, e1, c1);
                        }
                } else {
                        if ((self->castleFlags & castleFlagBlackKside)
                         && self->squares[f8] == empty
                         && self->squares[g8] == empty
                         && !self->xside->attacks[f8]
                        ) {
                                pushSpecialMove(self, e8, g8);
                        }
                        if ((self->castleFlags & castleFlagBlackQside)
                         && self->squares[d8] == empty
                         && self->squares[c8] == empty
                         && self->squares[b8] == empty
                         && !self->xside->attacks[d8]
                        ) {
                                pushSpecialMove(self, e8, c8);
                        }
                }
        }

        /*
         *  Generate en-passant captures
         */
        if (self->enPassantPawn) {
                int ep = self->enPassantPawn;

                if (sideToMove(self) == white) {
                        if (file(ep) != fileA && self->squares[ep+stepW] == whitePawn) {
                                pushSpecialMove(self, ep + stepW, ep + stepN);
                        }
                        if (file(ep) != fileH && self->squares[ep+stepE] == whitePawn) {
                                pushSpecialMove(self, ep + stepE, ep + stepN);
                        }
                } else {
                        if (file(ep) != fileA && self->squares[ep+stepW] == blackPawn) {
                                pushSpecialMove(self, ep + stepW, ep + stepS);
                        }
                        if (file(ep) != fileH && self->squares[ep+stepE] == blackPawn) {
                                pushSpecialMove(self, ep + stepE, ep + stepS);
                        }
                }
        }
}

/*----------------------------------------------------------------------+
 |      convert a move to SAN output                                    |
 +----------------------------------------------------------------------*/

/*
 *  Convert into SAN notation, but without any checkmark
 */

static char *moveToSan(Board_t self, char *san, int move, short *xmoves, int xlen)
{
        int from = from(move);
        int to   = to(move);

        if (move == specialMove(e1, c1) || move == specialMove(e8, c8)) {
                return stringCopy(san, "O-O-O");
        }
        if (move == specialMove(e1, g1) || move == specialMove(e8, g8)) {
                return stringCopy(san, "O-O");
        }

        if (self->squares[from] == whitePawn || self->squares[from] == blackPawn) {
                /*
                 *  Pawn moves are a bit special
                 */
                if (file(from) != file(to)) {
                        *san++ = fileToChar(file(from));
                        *san++ = 'x';
                }
                *san++ = fileToChar(file(to));
                *san++ = rankToChar(rank(to));

                /*
                 *  Promote to piece (=Q, =R, =B, =N)
                 */
                if (rank(to)==rank1 || rank(to)==rank8) {
                        *san++ = '=';
                        *san++ = promotionPieceToChar[move>>promotionBits];
                }
                *san = '\0';
                return san;
        }

        /*
         *  Piece identifier (K, Q, R, B, N)
         */
        *san++ = toupper(pieceToChar[self->squares[from]]);

        /*
         *  Disambiguate using from square information where needed
         */
        int filex = 0, rankx = 0;
        for (int i=0; i<xlen; i++) {
                int xmove = xmoves[i];
                if (to == to(xmove)                     // Must have same destination
                 && move != xmove                       // Different move
                 && self->squares[from] == self->squares[from(xmove)] // Same piece type
                 && isLegalMove(self, xmove)            // And also legal
                ) {
                        rankx |= (rank(from) == rank(from(xmove))) + 1; // Tricky but correct
                        filex |=  file(from) == file(from(xmove));
                }
        }
        if (rankx != filex) *san++ = fileToChar(file(from)); // Skip if both are 0 or 1
        if (filex)          *san++ = rankToChar(rank(from));

        /*
         *  Capture sign
         */
        if (self->squares[to] != empty) *san++ = 'x';

        /*
         *  To square
         */
        *san++ = fileToChar(file(to));
        *san++ = rankToChar(rank(to));

        *san = '\0';
        return san;
}

/*
 *  Produce a checkmark ('+' or '#')
 */

static char *getCheckMark(Board_t self, char *out, int move)
{
        makeMove(self, move);
        compute_attacks(self);

        if (inCheck(self)) { // in check, but is it checkmate?
                int mark = '#';
                short *start_moves = self->move_sp;
                generate_moves(self);
                while (self->move_sp > start_moves) {
                        if (isLegalMove(self, *--self->move_sp)) {
                                mark = '+';
                                self->move_sp = start_moves; // breaks the loop
                        }
                }
                *out++ = mark;
        }

        undoMove(self);

        *out = '\0';
        return out;
}

/*----------------------------------------------------------------------+
 |      helper to normalize the enPassantPawn field                     |
 +----------------------------------------------------------------------*/

static void normalizeEnPassantStatus(Board_t self)
{
        int square = self->enPassantPawn;
        if (!square) return;

        if (sideToMove(self) == white) {
                if (file(square) != fileA && self->squares[square+stepW] == whitePawn) {
                        int move = specialMove(square + stepW, square + stepN);
                        if (isLegalMove(self, move)) return;
                }
                if (file(square) != fileH && self->squares[square+stepE] == whitePawn) {
                        int move = specialMove(square + stepE, square + stepN);
                        if (isLegalMove(self, move)) return;
                }
        } else {
                if (file(square) != fileA && self->squares[square+stepW] == blackPawn) {
                        int move = specialMove(square + stepW, square + stepS);
                        if (isLegalMove(self, move)) return;
                }
                if (file(square) != fileH && self->squares[square+stepE] == blackPawn) {
                        int move = specialMove(square + stepE, square + stepS);
                        if (isLegalMove(self, move)) return;
                }
        }

        self->enPassantPawn = 0; // Clear en passant flag if there is no such legal capture
}

/*----------------------------------------------------------------------+
 |      Convert board to FEN notation                                   |
 +----------------------------------------------------------------------*/

static void boardToFen(Board_t self, char *fen)
{
        /*
         *  Squares
         */
        for (int rank=rank8; rank!=rank1-rankStep; rank-=rankStep) {
                int emptySquares = 0;
                for (int file=fileA; file!=fileH+fileStep; file+=fileStep) {
                        int square = square(file, rank);
                        int piece = self->squares[square];

                        if (piece == empty) {
                                emptySquares++;
                                continue;
                        }

                        if (emptySquares > 0) *fen++ = '0' + emptySquares;
                        *fen++ = pieceToChar[piece];
                        emptySquares = 0;
                }
                if (emptySquares > 0) *fen++ = '0' + emptySquares;
                if (rank != rank1) *fen++ = '/';
        }

        /*
         *  Side to move
         */
        *fen++ = ' ';
        *fen++ = (sideToMove(self) == white) ? 'w' : 'b';

        /*
         *  Castling flags
         */
        *fen++ = ' ';
        if (self->castleFlags) {
                if (self->castleFlags & castleFlagWhiteKside) *fen++ = 'K';
                if (self->castleFlags & castleFlagWhiteQside) *fen++ = 'Q';
                if (self->castleFlags & castleFlagBlackKside) *fen++ = 'k';
                if (self->castleFlags & castleFlagBlackQside) *fen++ = 'q';
        } else {
                *fen++ = '-';
        }

        /*
         *  En-passant square
         */
        *fen++ = ' ';
        normalizeEnPassantStatus(self);
        if (self->enPassantPawn) {
                *fen++ = fileToChar(file(self->enPassantPawn));
                *fen++ = rankToChar((sideToMove(self) == white) ? rank6 : rank3);
        } else {
                *fen++ = '-';
        }

        *fen = '\0';
}

/*----------------------------------------------------------------------+
 |      chessmoves                                                      |
 +----------------------------------------------------------------------*/

static void chessmoves(Board_t self)
{
        char old_fen[128];
        boardToFen(self, old_fen); // re-create from board for normalized output
        printf("fen,%s\n", old_fen);

        char buffer[256*128], *out = buffer; // raw storage for output lines
        char *lines[256]; // output lines, will be sorted
        int n = 0;

        /*
         *  Generate pseudo-legal moves
         */

        short *start_moves = self->move_sp;
        compute_attacks(self);
        generate_moves(self);

        int nr_pmoves = self->move_sp - start_moves;

        /*
         *  Try all moves
         */

        for (int i=0; i<nr_pmoves; i++) {
                int move = start_moves[i];

                makeMove(self, move);
                compute_attacks(self);

                /*
                 *  Test each pseudo-move for legality
                 */

                int isLegal = self->side->attacks[self->xside->king] == 0;
                if (isLegal) {

                        /*
                         *  If legal print move and resulting position
                         */

                        char new_fen[128];
                        boardToFen(self, new_fen);

                        undoMove(self);

                        /*
                         *  Put SAN and FEN in output buffer
                         */

                        lines[n++] = out; // new output line

                        out = moveToSan(self, out, move, start_moves, nr_pmoves);
                        out = getCheckMark(self, out, move);
                        *out++ = ',';
                        out = stringCopy(out, new_fen);
                        out++; // past the zero

                } else {
                        /*
                         *  If not legal skip it
                         */
                        undoMove(self);
                }
        }

        self->move_sp = start_moves; // clean move stack

        // Sort lines alphabetically (so by SAN)
        qsort(lines, n, sizeof lines[0], compare_strcmp);

        // Output the sorted lines
        for (int i=0; i<n; i++) {
                printf("move,%s\n", lines[i]);
        }

        printf("end\n"); // mark end of list
        fflush(stdout);
}

/*----------------------------------------------------------------------+
 |      main                                                            |
 +----------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
        char *line = NULL;
        int mallocSize = 0;
        int lineNumber = 0;

        struct board board;

        while (readLine(stdin, &line, &mallocSize) > 0) {
                lineNumber += 1;

                int len = setup_board(&board, line);
                if (len <= 0) {
                        fprintf(stderr, "*** Warning: no valid FEN on line %d\n", lineNumber);
                } else {
                        chessmoves(&board);
                }
        }

        free(line);
        return 0;
}

/*----------------------------------------------------------------------+
 |      readLine                                                        |
 +----------------------------------------------------------------------*/

static int readLine(FILE *fp, char **pLine, int *pMallocSize)
{
        char *line = *pLine;
        int mallocSize = *pMallocSize;
        int len = 0;

        for (;;) {
                /*
                 *  Ensure there is enough space for the next character and a terminator
                 */
                if (len + 1 >= mallocSize) {
                        int newMallocSize = (mallocSize > 0) ? (2 * mallocSize) : 128;
                        char *newLine = realloc(line, newMallocSize);

                        if (newLine == NULL) {
                                fprintf(stderr, "*** Error: %s\n", strerror(errno));
                                exit(EXIT_FAILURE);
                        }

                        line = newLine;
                        mallocSize = newMallocSize;
                }

                /*
                 *  Process next character from file
                 */
                int c = getc(fp);
                if (c == EOF) {
                        if (ferror(fp)) {
                                fprintf(stderr, "*** Error: %s\n", strerror(errno));
                                exit(EXIT_FAILURE);
                        } else {
                                break;
                        }
                }
                line[len++] = c;

                if (c == '\n') break; // End of line found
        }

        line[len] = '\0';
        *pLine = line;
        *pMallocSize = mallocSize;

        return len;
}

/*----------------------------------------------------------------------+
 |      compare_strcmp                                                  |
 +----------------------------------------------------------------------*/

static int compare_strcmp(const void *ap, const void *bp)
{
        const char * const *a = ap;
        const char * const *b = bp;
        return strcmp(*a, *b);
}

/*----------------------------------------------------------------------+
 |      stringCopy                                                      |
 +----------------------------------------------------------------------*/

static char *stringCopy(char *s, const char *t)
{
        while ((*s = *t)) {
                s++;
                t++;
        }
        return s; // give pointer to terminating zero for easy concatenation
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

