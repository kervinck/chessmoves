
/*----------------------------------------------------------------------+
 |      Board.h                                                         |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      Definitions                                                     |
 +----------------------------------------------------------------------*/

#include "geometry-a1a2.h"

typedef struct board *Board_t;

struct side {
        signed char attacks[boardSize];
        int king;
};

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

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

extern const char startpos[];

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

int setup_board(Board_t self, const char *fen);
void boardToFen(Board_t self, char *fen);
void compute_side_info(Board_t self);
void generate_moves(Board_t self);
void makeMove(Board_t self, int move);
void undoMove(Board_t self);
char *moveToStandardAlgebraic(Board_t self, char *moveString, int move, short *xmoves, int xlen);
char *moveToLongAlgebraic(Board_t self, char *moveString, int move);
char *moveToUci(Board_t self, char *moveString, int move);
const char *getCheckMark(Board_t self);
int parse_move(char *line, int *num);
unsigned long long hash(Board_t self);

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

