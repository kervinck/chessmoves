
/*----------------------------------------------------------------------+
 |      chessmovesmodule.c                                              |
 +----------------------------------------------------------------------*/

#include "Python.h"

#include <stdbool.h>

#include "Board.h"

/*----------------------------------------------------------------------+
 |      module                                                          |
 +----------------------------------------------------------------------*/

PyDoc_STRVAR(chessmoves_doc,
"Chess move and position generation (SAN/FEN/UCI).");

/*----------------------------------------------------------------------+
 |      Notation keywords                                               |
 +----------------------------------------------------------------------*/

static char *notations[] = {
        "uci", "san", "long"
};
enum { nrNotations = sizeof notations / sizeof notations[0] };

/*----------------------------------------------------------------------+
 |      moves()                                                         |
 +----------------------------------------------------------------------*/

PyDoc_STRVAR(moves_doc,
"moves(position, notation='san') -> { move : newPosition, ... }\n"
"\n"
"Generate all legal moves from a position.\n"
"Return the result as a dictionary, mapping moves to positions.\n"
"\n"
"The `notation' keyword controls the output move syntax.\n"
"Available are:\n"
" - san:  Standard Algebraic Notation (e.g. Nc3+, O-O, dxe8=Q)\n"
" - long: Long Algebraic Notation (e.g. Nb1-c3+, O-O, d7xe8=Q)\n"
" - uci:  Universal Chess Interface computer notation (e.g. b1c3, e8g8, d7e8q)"
);

static PyObject *
chessmovesmodule_moves(PyObject *self, PyObject *args, PyObject *keywords)
{
        char *fen;
        char *notation = "san"; // default

        static char *kwlist[] = { "fen", "notation", NULL };

        if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|s:moves", kwlist, &fen, &notation)) {
                return NULL;
        }

        struct board board;
        int len = setup_board(&board, fen);
        if (len <= 0) {
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL; // TODO raise proper exception saying what's wrong
        }

        int notationIndex;
        for (notationIndex=0; notationIndex<nrNotations; notationIndex++) {
                if (0==strcmp(notations[notationIndex], notation)) {
                        break; // found
                }
        }
        if (notationIndex >= nrNotations) { // not found
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL; // TODO raise proper exception saying what's wrong
        }

        short *start_moves = board.move_sp;
        compute_side_info(&board);
        generate_moves(&board);

        int nr_pmoves = board.move_sp - start_moves;

        PyObject *dict = PyDict_New();
        if (!dict) {
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL;
        }

        for (int i=0; i<nr_pmoves; i++) {
                int move = start_moves[i];

                makeMove(&board, move);
                compute_side_info(&board);

                int isLegal = board.side->attacks[board.xside->king] == 0;
                if (isLegal) {
                        // value is new position
                        char newFen[128];
                        boardToFen(&board, newFen);

                        undoMove(&board);

                        // key is move
                        char moveString[16];
                        char *s = moveString;

                        switch (notationIndex) {
                        case 0:
                                s = moveToUci(&board, s, move);
                                break;
                        case 1:
                                s = moveToStandardAlgebraic(&board, s, move, start_moves, nr_pmoves);
                                s = getCheckMark(&board, s, move);
                                break;
                        case 2:
                                s = moveToLongAlgebraic(&board, s, move);
                                s = getCheckMark(&board, s, move);
                                break;
                        default:
                                assert(0);
                        }

                        PyObject *key = PyString_FromString(moveString);
                        if (!key) {
                                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                                return NULL;
                        }

                        PyObject *value = PyString_FromString(newFen);
                        if (!value) {
                                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                                return NULL;
                        }

                        if (PyDict_SetItem(dict, key, value)) {
                                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                                return NULL;
                        }
                        Py_DECREF(key);
                        Py_DECREF(value);

                } else {
                        undoMove(&board);
                }
        }

	return dict;
}

/*----------------------------------------------------------------------+
 |      position()                                                      |
 +----------------------------------------------------------------------*/

PyDoc_STRVAR(position_doc,
"position(inputFen) -> standardFen\n"
"\n"
"Parse a FEN like string and convert it into a standardized FEN.\n"
"For example:\n"
" - Complete short ranks\n"
" - Order castling flags\n"
" - Remove en passant target square if there is no such legal capture\n"
" - Remove excess data beyond the FEN"
);

static PyObject *
chessmovesmodule_position(PyObject *self, PyObject *args)
{
        char *fen;

        if (!PyArg_ParseTuple(args, "s", &fen)) {
                return NULL;
        }

        struct board board;
        int len = setup_board(&board, fen);
        if (len <= 0) {
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL; // TODO raise proper exception saying what's wrong
        }

        char newFen[128];
        boardToFen(&board, newFen);

        return PyString_FromString(newFen);
}

/*----------------------------------------------------------------------+
 |      move()                                                          |
 +----------------------------------------------------------------------*/

#if 0
PyDoc_STRVAR(move_doc,
"move(position, move, notation='san') -> move\n"
"\n"
"Parse a move and check if it is legal in the position.\n"
"If so, return the move in the selected notation.\n"
"Return None otherwise." // TODO: how about an exception with a message?
);

static PyObject *
chessmovesmodule_moves(PyObject *self, PyObject *args, PyObject *keywords)
{
        char *fen;
        char *move;
        char *notation = "san"; // default

        static char *kwlist[] = { "fen", "move", "notation", NULL };

        if (!PyArg_ParseTupleAndKeywords(args, keywords, "ss|s:move",
             kwlist, &fen, &move, &notation)) {
                return NULL;
        }

        struct board board;
        int len = setup_board(&board, fen);
        if (len <= 0) {
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL; // TODO raise proper exception saying what's wrong
        }

        int notationIndex;
        for (notationIndex=0; notationIndex<nrNotations; notationIndex++) {
                if (0==strcmp(notations[notationIndex], notation)) {
                        break; // found
                }
        }
        if (notationIndex >= nrNotations) { // not found
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL; // TODO raise proper exception saying what's wrong
        }

        short *start_moves = board.move_sp;
        compute_side_info(&board);
        generate_moves(&board);

        int nr_pmoves = board.move_sp - start_moves;

        PyObject *dict = PyDict_New();
        if (!dict) {
                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                return NULL;
        }

        for (int i=0; i<nr_pmoves; i++) {
                int move = start_moves[i];

                makeMove(&board, move);
                compute_side_info(&board);

                int isLegal = board.side->attacks[board.xside->king] == 0;
                if (isLegal) {
                        // value is new position
                        char newFen[128];
                        boardToFen(&board, newFen);

                        undoMove(&board);

                        // key is move
                        char moveString[16];
                        char *s = moveString;

                        switch (notationIndex / 2) {
                        case 0:
                                s = moveToUci(&board, s, move);
                                break;
                        case 1:
                                s = moveToStandardAlgebraic(&board, s, move, start_moves, nr_pmoves);
                                break;
                        case 2:
                                s = moveToLongAlgebraic(&board, s, move);
                                break;
                        default:
                                assert(0);
                        }

                        if (notationIndex & 1) {
                                s = getCheckMark(&board, s, move);
                        }

                        PyObject *key = PyString_FromString(moveString);
                        if (!key) {
                                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                                return NULL;
                        }

                        PyObject *value = PyString_FromString(newFen);
                        if (!value) {
                                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                                return NULL;
                        }

                        if (PyDict_SetItem(dict, key, value)) {
                                fprintf(stderr, "%d: *** HUH ***\n", __LINE__);
                                return NULL;
                        }
                        Py_DECREF(key);
                        Py_DECREF(value);

                } else {
                        undoMove(&board);
                }
        }

	return dict;
}
#endif

/*----------------------------------------------------------------------+
 |      method table                                                    |
 +----------------------------------------------------------------------*/

static PyMethodDef chessmovesMethods[] = {
	{ "moves", (PyCFunction)chessmovesmodule_moves, METH_VARARGS|METH_KEYWORDS, moves_doc },
	{ "position", chessmovesmodule_position, METH_VARARGS, position_doc },
	//{ "move", (PyCFunction)chessmovesmodule_move, METH_VARARGS|METH_KEYWORDS, move_doc },
	{ NULL, }
};

/*
        moves
        move
        position

        generate
        parseMove
        parsePosition
*/

/*----------------------------------------------------------------------+
 |      initchessmoves                                                  |
 +----------------------------------------------------------------------*/

PyMODINIT_FUNC
initchessmoves(void)
{
	PyObject *module;

        // Create the module and add the functions
        module = Py_InitModule3("chessmoves", chessmovesMethods, chessmoves_doc);
        if (module == NULL) {
                return;
        }

        // Add startPosition as a string constant
        if (PyModule_AddStringConstant(module, "startPosition", startpos)) {
                return;
        }

        /*
         *  Add a list of available move notations
         */

        PyObject *list = PyList_New(nrNotations);
        if (!list) {
                return;
        }

        for (int i=0; i<nrNotations; i++) {
                PyList_SetItem(list, i, PyString_FromString(notations[i])); // TODO: error checking
        }

        if (PyModule_AddObject(module, "notations", list)) {
                Py_DECREF(list);
                return;
        }
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

