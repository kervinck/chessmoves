
/*----------------------------------------------------------------------+
 |      Board.c                                                         |
 +----------------------------------------------------------------------*/

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
#include "Board.h"

/*
 *  Other module includes
 */
#include "stringCopy.h"

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
 */

/*
 *  Move integer bits are as follows:
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

enum castleFlag {
        castleFlagWhiteKside = 1 << 0,
        castleFlagWhiteQside = 1 << 1,
        castleFlagBlackKside = 1 << 2,
        castleFlagBlackQside = 1 << 3
};

// Off-board offsets for use in the undo stack
#define offsetof_castleFlags   offsetof(struct board, castleFlags)
#define offsetof_enPassantPawn offsetof(struct board, enPassantPawn)

#define sideToMove(board) ((board)->plyNumber & 1)

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

const char startpos[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

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
             dir(sq,jumpNNW,bitNNW)    +    dir(sq,jumpNNE,bitNNE)+          \
                                                                             \
  dir(sq,jumpWNW,bitWNW)   +           +           +   dir(sq,jumpENE,bitENE)\
                                                                             \
               +           +           +           +           +             \
                                                                             \
  dir(sq,jumpWSW,bitWSW)   +           +           +   dir(sq,jumpESE,bitESE)\
                                                                             \
            +dir(sq,jumpSSW,bitSSW)    +    dir(sq,jumpSSE,bitSSE))

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

extern int setup_board(Board_t self, const char *fen)
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

extern void compute_side_info(Board_t self)
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

extern void undoMove(Board_t self)
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

extern void makeMove(Board_t self, int move)
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
        compute_side_info(self);
        int legal = (self->side->attacks[self->xside->king] == 0);
        undoMove(self);
        return legal;
}

static int inCheck(Board_t self)
{
        return self->xside->attacks[self->side->king] != 0;
}

extern void generate_moves(Board_t self)
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
 |      convert a move to UCI output                                    |
 +----------------------------------------------------------------------*/

/*
 *  Convert into UCI notation
 */

extern char *moveToUci(Board_t self, char *moveString, int move)
{
        int from = from(move);
        int to   = to(move);

        *moveString++ = fileToChar(file(from));
        *moveString++ = rankToChar(rank(from));
        *moveString++ = fileToChar(file(to));
        *moveString++ = rankToChar(rank(to));

        if ((self->squares[from] == whitePawn && rank(to)==rank8)
         || (self->squares[from] == blackPawn && rank(to)==rank1)
        ) {
                *moveString++ = tolower(promotionPieceToChar[move>>promotionBits]);
        }
        *moveString = '\0';

        return moveString;
}

/*----------------------------------------------------------------------+
 |      convert a move to long algebraic notation                       |
 +----------------------------------------------------------------------*/

/*
 *  Convert into long algebraic notation, but without any checkmark
 */
extern char *moveToLongAlgebraic(Board_t self, char *moveString, int move)
{
        int from = from(move);
        int to   = to(move);

        // Castling
        if (move == specialMove(e1, c1) || move == specialMove(e8, c8)) {
                return stringCopy(moveString, "O-O-O");
        }
        if (move == specialMove(e1, g1) || move == specialMove(e8, g8)) {
                return stringCopy(moveString, "O-O");
        }

        // Piece identifier
        char pieceChar = toupper(pieceToChar[self->squares[from]]);
        if (pieceChar != 'P') {
                *moveString++ = pieceChar;
        }

        // From square
        *moveString++ = fileToChar(file(from));
        *moveString++ = rankToChar(rank(from));

        // Capture mark
        *moveString++ = (self->squares[to] == empty) ? '-' : 'x';

        // To square
        *moveString++ = fileToChar(file(to));
        *moveString++ = rankToChar(rank(to));

        // Promotion piece
        if ((self->squares[from] == whitePawn && rank(to) == rank8)
         || (self->squares[from] == blackPawn && rank(to) == rank1)
        ) {
                *moveString++ = '=';
                *moveString++ = promotionPieceToChar[move>>promotionBits];
        }

        *moveString = '\0';
        return moveString;
}
/*----------------------------------------------------------------------+
 |      convert a move to SAN output                                    |
 +----------------------------------------------------------------------*/

/*
 *  Convert into SAN notation, but without any checkmark
 */
extern char *moveToStandardAlgebraic(Board_t self, char *moveString, int move, short *xmoves, int xlen)
{
        int from = from(move);
        int to   = to(move);

        if (move == specialMove(e1, c1) || move == specialMove(e8, c8)) {
                return stringCopy(moveString, "O-O-O");
        }
        if (move == specialMove(e1, g1) || move == specialMove(e8, g8)) {
                return stringCopy(moveString, "O-O");
        }

        if (self->squares[from] == whitePawn || self->squares[from] == blackPawn) {
                /*
                 *  Pawn moves are a bit special
                 */
                if (file(from) != file(to)) {
                        *moveString++ = fileToChar(file(from));
                        *moveString++ = 'x';
                }
                *moveString++ = fileToChar(file(to));
                *moveString++ = rankToChar(rank(to));

                /*
                 *  Promote to piece (=Q, =R, =B, =N)
                 */
                if (rank(to)==rank1 || rank(to)==rank8) {
                        *moveString++ = '=';
                        *moveString++ = promotionPieceToChar[move>>promotionBits];
                }
                *moveString = '\0';
                return moveString;
        }

        /*
         *  Piece identifier (K, Q, R, B, N)
         */
        *moveString++ = toupper(pieceToChar[self->squares[from]]);

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
        if (rankx != filex) *moveString++ = fileToChar(file(from)); // Skip if both are 0 or 1
        if (filex)          *moveString++ = rankToChar(rank(from));

        /*
         *  Capture sign
         */
        if (self->squares[to] != empty) *moveString++ = 'x';

        /*
         *  To square
         */
        *moveString++ = fileToChar(file(to));
        *moveString++ = rankToChar(rank(to));

        *moveString = '\0';
        return moveString;
}

/*----------------------------------------------------------------------+
 |      getCheckMark                                                    |
 +----------------------------------------------------------------------*/

/*
 *  Produce a checkmark ('+' or '#')
 */
extern char *getCheckMark(Board_t self, char *out, int move)
{
        makeMove(self, move);
        compute_side_info(self);

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

extern void boardToFen(Board_t self, char *fen)
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
 |      move parser                                                     |
 +----------------------------------------------------------------------*/

#if 0
extern int parse_move(char *line, int *num)
{
        int                     move, matches;
        int                     n = 0;
        short                   *m;
        char                    *piece = NULL;
        char                    *fr_file = NULL;
        char                    *fr_rank = NULL;
        char                    *to_file = NULL;
        char                    *to_rank = NULL;
        char                    *prom_piece = NULL;
        char                    *s;

        while (isspace(line[n]))      /* skip white space */
                n++;

        if (!strncmp(line+n, "o-o-o", 5)
        ||  !strncmp(line+n, "O-O-O", 5)
        ||  !strncmp(line+n, "0-0-0", 5)) {
                piece = "K"; fr_file = "e"; to_file = "c";
                n+=5;
        }
        else if (!strncmp(line+n, "o-o", 3)
        ||  !strncmp(line+n, "O-O", 3)
        ||  !strncmp(line+n, "0-0", 3)) {
                piece = "K"; fr_file = "e"; to_file = "g";
                n+=3;
        }
        else {
                s = strchr("KQRBNP", line[n]);

                if (s && *s) {
                        piece = s;
                        n++;
                }

                /* first square */

                s = strchr("abcdefgh", line[n]);
                if (s && *s) {
                        to_file = s;
                        n++;
                }

                s = strchr("12345678", line[n]);
                if (s && *s) {
                        to_rank = s;
                        n++;
                }

                if (line[n] == '-' || line[n] == 'x') {
                        n++;
                }

                s = strchr("abcdefgh", line[n]);
                if (s && *s) {
                        fr_file = to_file;
                        fr_rank = to_rank;
                        to_file = s;
                        to_rank = NULL;
                        n++;
                }

                s = strchr("12345678", line[n]);
                if (s && *s) {
                        to_rank = s;
                        n++;
                }

                if (line[n] == '=') {
                        n++;
                }
                s = strchr("QRBNqrbn", line[n]);
                if (s && *s) {
                        prom_piece = s;
                        n++;
                }
        }

        while (line[n] == '+' || line[n] == '#'
        || line[n] == '!' || line[n] == '?') {
                n++;
        }

        *num = n;
        if (!piece && !fr_file && !fr_rank
        && !to_file && !to_rank && !prom_piece)
                return 0;

        move = 0;
        matches = 0;

        m = move_sp;
        compute_attacks();
        generate_moves();
        while (move_sp > m) {
                int fr, to;

                move_sp--;

                fr = FR(*move_sp);
                to = TO(*move_sp);

                /* does this move match? */

                if ((piece && *piece != toupper(PIECE2CHAR(board[fr])))
                 || (to_file && *to_file != FILE2CHAR(F(to)))
                 || (to_rank && *to_rank != RANK2CHAR(R(to)))
                 || (fr_file && *fr_file != FILE2CHAR(F(fr)))
                 || (fr_rank && *fr_rank != RANK2CHAR(R(fr)))
                 || (prom_piece &&
                        (toupper(*prom_piece) != "QRBN"[(*move_sp)>>13])))
                {
                        continue;
                }

                /* if so, is it legal? */
                if (test_illegal(*move_sp)) {
                        continue;
                }
                /* probably.. */

                /* do we already have a match? */
                if (move) {
                        int old_is_p, new_is_p;

                        if (piece) {
                                move_sp = m;
                                return 0;
                        }
                        /* if this move is a pawn move and the previously
                           found isn't, we accept it */
                        old_is_p = (board[FR(move)]==WHITE_PAWN) ||
                                (board[FR(move)]==BLACK_PAWN);
                        new_is_p = (board[fr]==WHITE_PAWN) ||
                                (board[fr]==BLACK_PAWN);

                        if (new_is_p && !old_is_p) {
                                move = *move_sp;
                                matches = 1;
                        } else if (old_is_p && !new_is_p) {
                        } else if (!old_is_p && !new_is_p)  {
                                matches++;
                        } else if (old_is_p && new_is_p)  {
                                move_sp = m;
                                return 0;
                        }
                } else {
                        move = *move_sp;
                        matches = 1;
                }
        }
        if (matches <= 1)
                return move;

        return 0;
}
#endif

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

