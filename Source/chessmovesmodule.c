
/*----------------------------------------------------------------------+
 |      chessmovesmodule.c                                              |
 +----------------------------------------------------------------------*/

#include "Python.h"

#include <stdbool.h>

#include "Board.h"

#include "stringCopy.h"

/*----------------------------------------------------------------------+
 |      module                                                          |
 +----------------------------------------------------------------------*/

PyDoc_STRVAR(chessmoves_doc,
        "Chess move and position generation (SAN/FEN/UCI).");

/*----------------------------------------------------------------------+
 |      Notation keywords                                               |
 +----------------------------------------------------------------------*/

enum {
        uciNotation, sanNotation, longNotation,
        nrNotations
};

static char *notations[] = {
        [uciNotation] = "uci",
        [sanNotation] = "san",
        [longNotation] = "long"
};

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
        "Available notations are:\n"
        "    'san': Standard Algebraic Notation (e.g. Nc3+, O-O, dxe8=Q)\n"
        "    'long': Long Algebraic Notation (e.g. Nb1-c3+, O-O, d7xe8=Q)\n"
        "    'uci': Universal Chess Interface computer notation (e.g. b1c3, e8g8, d7e8q)"
);

static PyObject *
chessmovesmodule_moves(PyObject *self, PyObject *args, PyObject *keywords)
{
        char *fen;
        char *notation = "san"; // default

        static char *keywordList[] = { "fen", "notation", NULL };

        if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|s:moves", keywordList, &fen, &notation)) {
                return NULL;
        }

        struct board board;
        int len = setup_board(&board, fen);
        if (len <= 0) {
                PyErr_SetString(PyExc_ValueError, "Invalid FEN");
                return NULL;
        }

        int notationIndex;
        for (notationIndex=0; notationIndex<nrNotations; notationIndex++) {
                if (0==strcmp(notations[notationIndex], notation)) {
                        break; // found
                }
        }

        if (notationIndex >= nrNotations) { // not found
                PyErr_SetString(PyExc_ValueError, "Invalid notation");
                return NULL;
        }

        short *start_moves = board.move_sp;
        compute_side_info(&board);
        generate_moves(&board);

        int nr_pmoves = board.move_sp - start_moves;

        PyObject *dict = PyDict_New();
        if (!dict) {
                return NULL;
        }

        for (int i=0; i<nr_pmoves; i++) {
                int move = start_moves[i];

                makeMove(&board, move);
                compute_side_info(&board);

                int isLegal = board.side->attacks[board.xside->king] == 0;
                if (!isLegal) {
                        undoMove(&board);
                        continue;
                }

                // value is new position
                char newFen[128];
                boardToFen(&board, newFen);

                // key is move
                char moveString[16];
                char *s = moveString;
                const char *checkmark;

                switch (notationIndex) {
                case uciNotation:
                        undoMove(&board);
                        s = moveToUci(&board, s, move);
                        break;
                case sanNotation:
                        checkmark = getCheckMark(&board);
                        undoMove(&board);
                        s = moveToStandardAlgebraic(&board, s, move, start_moves, nr_pmoves);
                        s = stringCopy(s, checkmark);
                        break;
                case longNotation:
                        checkmark = getCheckMark(&board);
                        undoMove(&board);
                        s = moveToLongAlgebraic(&board, s, move);
                        s = stringCopy(s, checkmark);
                        break;
                default:
                        assert(0);
                }

                PyObject *key = PyString_FromString(moveString);
                if (!key) {
                        return NULL;
                }

                PyObject *value = PyString_FromString(newFen);
                if (!value) {
                        return NULL;
                }

                if (PyDict_SetItem(dict, key, value)) {
                        return NULL;
                }

                Py_DECREF(key);
                Py_DECREF(value);
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
        " - Complete shortened ranks\n"
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
                PyErr_SetString(PyExc_ValueError, "Invalid FEN");
                return NULL;
        }

        char newFen[128];
        boardToFen(&board, newFen);

        return PyString_FromString(newFen);
}

/*----------------------------------------------------------------------+
 |      method table                                                    |
 +----------------------------------------------------------------------*/

static PyMethodDef chessmovesMethods[] = {
	{ "moves", (PyCFunction)chessmovesmodule_moves, METH_VARARGS|METH_KEYWORDS, moves_doc },
	{ "position", chessmovesmodule_position, METH_VARARGS, position_doc },
	{ NULL, }
};

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
                if (PyList_SetItem(list, i, PyString_FromString(notations[i]))) {
                        return;
                }
        }

        if (PyModule_AddObject(module, "notations", list)) {
                Py_DECREF(list);
                return;
        }
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

