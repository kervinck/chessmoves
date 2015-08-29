
/*----------------------------------------------------------------------+
 |                                                                      |
 |      Board.h                                                         |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*
 *  Copyright (C) 1998-2015, Marcel van Kervinck
 *  All rights reserved
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its
 *  contributors may be used to endorse or promote products derived
 *  from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*----------------------------------------------------------------------+
 |      Definitions                                                     |
 +----------------------------------------------------------------------*/

#include "geometry-a1a2.h"

typedef struct board *Board_t;

struct side {
        signed char attacks[boardSize];
        int king;
};

#define maxMoves 256
#define maxMoveLen sizeof("a7-a8=N+")

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
#ifndef NDEBUG
        int debugSideInfoPlyNumber; // for sanity checking
#endif

        /*
         *  Move undo administration
         */
        signed char undoStack[256];
        int undoLen;
        int *movePtr; // For in move generation
};

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

extern const char startpos[];

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

/*
 *  Setup chess board from position description in FEN notation
 *
 *  Return the length of the FEN on success, or 0 on failure.
 */
int setupBoard(Board_t self, const char *fen);

/*
 *  Convert the current position to FEN
 */
void boardToFen(Board_t self, char *fen);

/*
 *  Compute a 64-bit hash for the current position using Polyglot-Zobrist hashing
 */
unsigned long long hash64(Board_t self);

void updateSideInfo(Board_t self);

/*
 *  Generate all pseudo-legal moves for the position and return the move count
 */
int generateMoves(Board_t self, int moveList[maxMoves]);

/*
 *  Make the move on the board
 */
void makeMove(Board_t self, int move);

/*
 *  Retract the last move and restore the previous position
 *
 *  All moves can be undone all the way back to the setup position
 */
void undoMove(Board_t self);

/*
 *  Convert move to standard algebraic notation, without checkmark
 *
 *  A movelist must be prepared by the caller for disambiguation,
 *  which may include the move itself.
 */
char *moveToStandardAlgebraic(Board_t self, char moveString[maxMoveLen], int move, int xmoves[], int xlen);

/*
 *  Convert move to long algebraic notation, without checkmark
 */
char *moveToLongAlgebraic(Board_t self, char moveString[maxMoveLen], int move);

/*
 *  Convert move to computer notation (UCI)
 */
char *moveToUci(Board_t self, char moveString[maxMoveLen], int move);

/*
 *  Determine the check status for the current position ("", "+" or "#")
 */
const char *getCheckMark(Board_t self);

/*
 *  Parse move input, disambiguate abbreviated notations
 *  A movelist must be prepared by the caller for disambiguation.
 *
 *  Return the length of the move on success, or <= 0 on failure:
 *   0: Invalid move syntax
 *  -1: Not a legal move in this position
 *  -2: Ambiguous move
 *
 */
extern int parseMove(Board_t self, const char *line, int xmoves[maxMoves], int xlen, int *move);


/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

