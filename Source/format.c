
/*----------------------------------------------------------------------+
 |                                                                      |
 |      format.c -- converting between internal data and text formats   |
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
 |      Includes                                                        |
 +----------------------------------------------------------------------*/

// Standard includes
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

// Own include
#include "Board.h"

// Other module includes
#include "polyglot.h"
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

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

const char startpos[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static const char pieceToChar[] = {
        [empty] = '\0',
        [whiteKing]   = 'K', [whiteQueen]  = 'Q', [whiteRook] = 'R',
        [whiteBishop] = 'B', [whiteKnight] = 'N', [whitePawn] = 'P',
        [blackKing]   = 'k', [blackQueen]  = 'q', [blackRook] = 'r',
        [blackBishop] = 'b', [blackKnight] = 'n', [blackPawn] = 'p',
};

static const char promotionPieceToChar[] = { 'Q', 'R', 'B', 'N' };

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      setupBoard                                                      |
 +----------------------------------------------------------------------*/

extern int setupBoard(Board_t self, const char *fen)
{
        int ix = 0;

        /*
         *  Squares
         */

        while (isspace(fen[ix])) ix++;

        int file = fileA, rank = rank8;
        int nrWhiteKings = 0, nrBlackKings = 0;
        memset(self->squares, empty, boardSize);
        while (rank != rank1 || file != fileH + fileStep) {
                int piece = empty;
                int count = 1;

                switch (fen[ix]) {
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
                case '/': rank -= rankStep; file = fileA; ix++; continue;
                case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8':
                        count = fen[ix] - '0';
                        break;
                default:
                        return 0; // FEN error
                }
                do {
                        self->squares[square(file,rank)] = piece;
                        file += fileStep;
                } while (--count && file != fileH + fileStep);
                ix++;
        }
        if (nrWhiteKings != 1 || nrBlackKings != 1) return 0;

        /*
         *  Side to move
         */

        self->plyNumber = 2 + (fen[ix+1] == 'b'); // 2 means full move number starts at 1
        //self->lastZeroing = self->plyNumber;
        ix += 2;

        /*
         *  Castling flags
         */

        while (isspace(fen[ix])) ix++;

        self->castleFlags = 0;
        for (;; ix++) {
                switch (fen[ix]) {
                case 'K': self->castleFlags |= castleFlagWhiteKside; continue;
                case 'Q': self->castleFlags |= castleFlagWhiteQside; continue;
                case 'k': self->castleFlags |= castleFlagBlackKside; continue;
                case 'q': self->castleFlags |= castleFlagBlackQside; continue;
                case '-': ix++; break;
                default: break;
                }
                break;
        }

        /*
         *  En passant square
         */

        while (isspace(fen[ix])) ix++;

        if ('a' <= fen[ix] && fen[ix] <= 'h') {
                file = charToFile(fen[ix]);
                ix++;

                rank = (sideToMove(self) == white) ? rank5 : rank4;
                if (isdigit(fen[ix])) ix++; // ignore what it says

                self->enPassantPawn = square(file, rank);
        } else {
                self->enPassantPawn = 0;
                if (fen[ix] == '-')
                        ix++;
        }

#ifndef NDEBUG
        self->debugSideInfoPlyNumber = -1; // side info is invalid
#endif

        // Reset the undo stack
        self->undoLen = 0;

        return ix;
}

/*----------------------------------------------------------------------+
 |      Convert a move to UCI output                                    |
 +----------------------------------------------------------------------*/

/*
 *  Convert into UCI notation
 */
extern char *moveToUci(Board_t self, char moveString[maxMoveSize], int move)
{
        int from = from(move);
        int to   = to(move);

        *moveString++ = fileToChar(file(from));
        *moveString++ = rankToChar(rank(from));
        *moveString++ = fileToChar(file(to));
        *moveString++ = rankToChar(rank(to));
        if (isPromotion(self, from, to))
                *moveString++ = tolower(promotionPieceToChar[move>>promotionBits]);
        *moveString = '\0';

        return moveString;
}

/*----------------------------------------------------------------------+
 |      Convert a move to long algebraic notation                       |
 +----------------------------------------------------------------------*/

/*
 *  Convert into long algebraic notation, but without any checkmark
 */
extern char *moveToLongAlgebraic(Board_t self, char moveString[maxMoveSize], int move)
{
        int from = from(move);
        int to   = to(move);

        // Castling
        if (move == specialMove(e1, c1) || move == specialMove(e8, c8))
                return stringCopy(moveString, "O-O-O");
        if (move == specialMove(e1, g1) || move == specialMove(e8, g8))
                return stringCopy(moveString, "O-O");

        // Piece identifier
        char pieceChar = toupper(pieceToChar[self->squares[from]]);
        if (pieceChar != 'P')
                *moveString++ = pieceChar;

        // From square
        *moveString++ = fileToChar(file(from));
        *moveString++ = rankToChar(rank(from));

        // Capture mark
        *moveString++ = (self->squares[to] == empty) ? '-' : 'x';

        // To square
        *moveString++ = fileToChar(file(to));
        *moveString++ = rankToChar(rank(to));

        // Promotion piece
        if (isPromotion(self, from, to)) {
                *moveString++ = '=';
                *moveString++ = promotionPieceToChar[move>>promotionBits];
        }

        *moveString = '\0';
        return moveString;
}

/*----------------------------------------------------------------------+
 |      Convert a move to SAN output                                    |
 +----------------------------------------------------------------------*/

/*
 *  Convert into SAN notation, but without any checkmark
 */
extern char *moveToStandardAlgebraic(Board_t self, char moveString[maxMoveSize], int move, int xMoves[maxMoves], int xlen)
{
        int from = from(move);
        int to   = to(move);

        if (move == specialMove(e1, c1) || move == specialMove(e8, c8))
                return stringCopy(moveString, "O-O-O");

        if (move == specialMove(e1, g1) || move == specialMove(e8, g8))
                return stringCopy(moveString, "O-O");

        if (self->squares[from] == whitePawn || self->squares[from] == blackPawn) {
                // Pawn moves are a bit special

                if (file(from) != file(to)) {
                        *moveString++ = fileToChar(file(from));
                        *moveString++ = 'x';
                }
                *moveString++ = fileToChar(file(to));
                *moveString++ = rankToChar(rank(to));

                // Promote to piece (=Q, =R, =B, =N)
                if (isPromotion(self, from, to)) {
                        *moveString++ = '=';
                        *moveString++ = promotionPieceToChar[move>>promotionBits];
                }
                *moveString = '\0';
                return moveString;
        }

        // Piece identifier (K, Q, R, B, N)
        *moveString++ = toupper(pieceToChar[self->squares[from]]);

        //  Disambiguate using from square information where needed
        int filex = 0, rankx = 0;
        for (int i=0; i<xlen; i++) {
                int xMove = xMoves[i];
                if (to == to(xMove)                     // Must have same destination
                 && move != xMove                       // Different move
                 && self->squares[from] == self->squares[from(xMove)] // Same piece type
                 && isLegalMove(self, xMove)            // And also legal
                ) {
                        rankx |= (rank(from) == rank(from(xMove))) + 1; // Tricky but correct
                        filex |=  file(from) == file(from(xMove));
                }
        }
        if (rankx != filex) *moveString++ = fileToChar(file(from)); // Skip if both are 0 or 1
        if (filex)          *moveString++ = rankToChar(rank(from));

        // Capture sign
        if (self->squares[to] != empty) *moveString++ = 'x';

        // To square
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
 *  The move must already be made and sideInfo computed.
 *  sideInfo might be invalid after this function.
 */
extern const char *getCheckMark(Board_t self)
{
        const char *checkmark = "";

        if (inCheck(self)) { // in check, but is it checkmate?
                checkmark = "#";
                int moveList[maxMoves];
                int nrMoves = generateMoves(self, moveList);
                for (int i=0; i<nrMoves; i++) {
                        if (isLegalMove(self, moveList[i])) {
                                checkmark = "+";
                                break;
                        }
                }
        }

        return checkmark;
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
        } else
                *fen++ = '-';

        /*
         *  En-passant square
         */
        *fen++ = ' ';
        normalizeEnPassantStatus(self);
        if (self->enPassantPawn) {
                *fen++ = fileToChar(file(self->enPassantPawn));
                *fen++ = rankToChar((sideToMove(self) == white) ? rank6 : rank3);
        } else
                *fen++ = '-';

        /*
         *  Move number (TODO)
         */

        /*
         *  Halfmove clock (TODO)
         */

        *fen = '\0';
}

/*----------------------------------------------------------------------+
 |      Move parser                                                     |
 +----------------------------------------------------------------------*/

/*
 *  Accept: "O-O" "O-O-O" "o-o" "0-0" "OO" "000" etc
 *  Reject: "OO-O" "O--O" "o-O" "o0O" etc
 */
static int parseCastling(const char *line, int *len)
{
        int nrOh = 0;
        int ix = 0;
        int oh = line[ix];
        if (oh == 'O' || oh == 'o' || oh == '0') {
                do {
                        nrOh++;
                        if (line[++ix] == '-')  ix++;
                } while (line[ix] == oh);
        }
        if (ix != nrOh && ix != 2 * nrOh - 1) { // still looks malformed
                nrOh = 0;
                ix = 0;
        }
        *len = ix;
        return nrOh;
}

static bool isPieceChar(int c)
{
        const char *s = strchr("KQRBNP", c);
        return (s != NULL) && (*s != '\0');
}

extern int parseMove(Board_t self, const char *line, int xMoves[maxMoves], int xlen, int *move)
{
        /*
         *  Phase 1: extract as many as possible move elements from input
         */

        int ix = 0; // index into line

        /*
         *  The line elements for move disambiguation
         *  Capture and checkmarks will be swallowed and are not checked
         *  for correctness either
         */
        char fromPiece = 0;
        char fromFile = 0;
        char fromRank = 0;
        char toPiece = 0;
        char toFile = 0;
        char toRank = 0;
        char promotionPiece = 0;

        while (isspace(line[ix]))       // Skip white spaces
                ix++;

        int castleLen;
        int nrOh = parseCastling(&line[ix], &castleLen);

        if (nrOh == 2) {                // King side castling
                fromPiece = 'K';
                fromFile = 'e';
                toFile = 'g';
                ix += castleLen;
        } else if (nrOh == 3) {         // Queen side castling
                fromPiece = 'K';
                fromFile = 'e';
                toFile = 'c';
                ix += castleLen;
        } else {                        // Regular move
                if (isPieceChar(line[ix])) {
                        fromPiece = line[ix++];
                        if (line[ix] == '/') ix++; // ICS madness
                }

                if ('a' <= line[ix] && line[ix] <= 'h')
                        toFile = line[ix++];

                if ('1' <= line[ix] && line[ix] <= '8')
                        toRank = line[ix++];

                switch (line[ix]) {
                case 'x': case ':':
                        if (isPieceChar(line[++ix]))
                                toPiece = line[ix++];
                        break;
                case '-':
                        ix++;
                }

                if ('a' <= line[ix] && line[ix] <= 'h') {
                        fromFile = toFile;
                        fromRank = toRank;
                        toFile = line[ix++];
                        toRank = 0;
                }

                if ('1' <= line[ix] && line[ix] <= '8') {
                        if (toRank) fromRank = toRank;
                        toRank = line[ix++];
                }

                if (line[ix] == '=')
                        ix++;

                if (isPieceChar(toupper(line[ix])))
                        promotionPiece = toupper(line[ix++]);
        }

        while (line[ix] == '+' || line[ix] == '#' || line[ix] == '!' || line[ix] == '?')
                ix++;

        /*
         *  Phase 2: Reject if it still doesn't look anything like a move
         */

        if (isalnum(line[ix]) || line[ix] == '-' || line[ix] == '=')
                return 0; // Reject garbage following the move

        if (!fromPiece && !toPiece && !promotionPiece) {
                if (!fromFile && !toFile) return 0;             // Reject "", "3", "34"
                if (fromRank && !toRank) return 0;              // Reject "3a", "a3b"
                if (toFile && !toRank && !fromFile) return 0;   // Reject "a"
        }

        /*
         *  Phase 3: Search for a unique legal matching move
         */

        int nrMatches = 0;
        int matchedMove = 0;
        int precedence = -1; // -1 no move, 0 regular move, 1 pawn move, 2 queen promotion

        for (int i=0; i<xlen; i++) {
                int xMove = xMoves[i];
                int xFrom = from(xMove);
                int xTo = to(xMove);
                int xPiece = self->squares[xFrom];
                int xPromotionPiece = 0;
                if (isPromotion(self, xFrom, xTo))
                        xPromotionPiece = promotionPieceToChar[xMove>>promotionBits];

                // Do all parsed elements match with this candidate move? And is it legal?
                if ((fromPiece      && fromPiece != toupper(pieceToChar[xPiece]))
                 || (fromFile       && fromFile  != fileToChar(file(xFrom)))
                 || (fromRank       && fromRank  != rankToChar(rank(xFrom)))
                 || (toPiece        && toPiece   != toupper(pieceToChar[self->squares[xTo]]))
                 || (toFile         && toFile    != fileToChar(file(xTo)))
                 || (toRank         && toRank    != rankToChar(rank(xTo)))
                 || (promotionPiece && promotionPiece != xPromotionPiece)
                 || !isLegalMove(self, xMove)
                ) {
                        continue;
                }
                // else: the candidate move matches

                int xPrecedence = 0;
                if (xPiece == whitePawn || xPiece == blackPawn)
                        xPrecedence = (xPromotionPiece == 'Q') ? 2 : 1;

                /*
                 *  Clash with another match is not bad if the new candidate move
                 *  is a pawn move and the previous one isn't.
                 *  This is to accept "bxc3" in the presence "Nb1xc3", for example.
                 *  Same logic to prefer queening if the promoted piece is not given.
                 */
                if (precedence < xPrecedence)
                        nrMatches = 0;

                if (precedence <= xPrecedence) {
                        matchedMove = xMove;
                        precedence = xPrecedence;
                        nrMatches++;
                }
        }

        if (nrMatches == 0)
                return -1; // Move not legal

        if (nrMatches > 1)
                return -2; // More than one legal move

        *move = matchedMove;
        return ix;
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

