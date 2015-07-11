
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
 *  Open issues:
 *  TODO: cleanup: memory for stacks?
 *  TODO: cleanup: add descriptions to all functions
 *  TODO: make function call order dependencies easier to understand
 *        (= rethinking functions with old naming)
 *  TODO: cleanup: error handling
 *  TODO: option to choose delimiter (for example: -d,). default ' '
 *  TODO: consider profiling
 *  TODO: consider speedup with quick_makeMove in isLegalMove
 *  TODO: handle 6-field FEN, detect automatically
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

#include "readLine.h"
#include "stringCopy.h"

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

static int compare_strcmp(const void *ap, const void *bp)
{
        const char * const *a = ap;
        const char * const *b = bp;
        return strcmp(*a, *b);
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
        compute_side_info(self);
        generate_moves(self);

        int nr_pmoves = self->move_sp - start_moves;

        /*
         *  Try all moves
         */

        for (int i=0; i<nr_pmoves; i++) {
                int move = start_moves[i];

                makeMove(self, move);
                compute_side_info(self);

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

                        const char *checkmark = getCheckMark(self);

                        undoMove(self);

                        /*
                         *  Put SAN and FEN in output buffer
                         */

                        lines[n++] = out; // new output line

                        out = moveToStandardAlgebraic(self, out, move, start_moves, nr_pmoves);
                        out = stringCopy(out, checkmark);
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
 |                                                                      |
 +----------------------------------------------------------------------*/

