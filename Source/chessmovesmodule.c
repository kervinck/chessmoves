
/*----------------------------------------------------------------------+
 |                                                                      |
 |      chessmovesmodule.c                                              |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*
 *  Copyright (C) 2015, Marcel van Kervinck
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

// Python include (must come first)
#include "Python.h"

// Standard includes
#include <stdbool.h>

// Other module includes
#include "Board.h"
#include "stringCopy.h"

/*----------------------------------------------------------------------+
 |      Module                                                          |
 +----------------------------------------------------------------------*/

// Module docstring
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
 |      moves(...)                                                      |
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

        if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|s:moves", keywordList,
                                         &fen, &notation))
                return NULL;

        struct board board;
        int len = setupBoard(&board, fen);
        if (len <= 0)
                return PyErr_Format(PyExc_ValueError, "Invalid FEN");

        int notationIndex;
        for (notationIndex=0; notationIndex<nrNotations; notationIndex++)
                if (0==strcmp(notations[notationIndex], notation))
                        break; // found

        if (notationIndex >= nrNotations) // not found
                return PyErr_Format(PyExc_ValueError, "Invalid notation");

        int moveList[maxMoves];
        updateSideInfo(&board);
        int nrMoves = generateMoves(&board, moveList);

        PyObject *dict = PyDict_New();
        if (!dict)
                return NULL;

        for (int i=0; i<nrMoves; i++) {
                int move = moveList[i];

                makeMove(&board, move);

                updateSideInfo(&board);
                bool isLegal = board.side->attacks[board.xside->king] == 0;
                if (!isLegal) {
                        undoMove(&board);
                        continue;
                }

                // key is move
                char moveString[maxMoveSize];
                char *s = moveString;
                const char *checkmark;

                // value is new position
                char newFen[maxFenSize];

                switch (notationIndex) {
                case uciNotation:
                        boardToFen(&board, newFen);
                        undoMove(&board);
                        s = moveToUci(&board, s, move);
                        break;
                case sanNotation:
                        checkmark = getCheckMark(&board);
                        boardToFen(&board, newFen);
                        undoMove(&board);
                        s = moveToStandardAlgebraic(&board, s, move, moveList, nrMoves);
                        s = stringCopy(s, checkmark);
                        break;
                case longNotation:
                        checkmark = getCheckMark(&board);
                        boardToFen(&board, newFen);
                        undoMove(&board);
                        s = moveToLongAlgebraic(&board, s, move);
                        s = stringCopy(s, checkmark);
                        break;
                default:
                        assert(0);
                }

                PyObject *key = PyString_FromString(moveString);
                if (!key) {
                        Py_DECREF(dict);
                        return NULL;
                }

                PyObject *value = PyString_FromString(newFen);
                if (!value) {
                        Py_DECREF(dict);
                        Py_DECREF(key);
                        return NULL;
                }

                if (PyDict_SetItem(dict, key, value)) {
                        Py_DECREF(dict);
                        Py_DECREF(key);
                        Py_DECREF(value);
                        return NULL;
                }

                Py_DECREF(key);
                Py_DECREF(value);
        }

	return dict;
}

/*----------------------------------------------------------------------+
 |      position(...)                                                   |
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

        if (!PyArg_ParseTuple(args, "s", &fen))
                return NULL;

        struct board board;
        int len = setupBoard(&board, fen);
        if (len <= 0)
                return PyErr_Format(PyExc_ValueError, "Invalid FEN (%s)", fen);

        char newFen[maxFenSize];
        boardToFen(&board, newFen);

        return PyString_FromString(newFen);
}

/*----------------------------------------------------------------------+
 |      move(...)                                                       |
 +----------------------------------------------------------------------*/

PyDoc_STRVAR(move_doc,
        "move(inputFen, inputMove, notation='san') -> (move, fen)\n"
        "\n"
        "Try to parse the input move and return it as a normalized string\n"
        "if successful, legal and unambiguous. \n"
        "\n"
        "The parser accepts a wide variety of formats. The only restriction\n"
        "is that piece identifiers, other than promotion pieces, must always\n"
        "be in upper case, and file letters must always be in lower case.\n"
        "Input capture signs, check marks, checkmate marks, annotations\n"
        "(x, +, !, ?, etc) are all swallowed and ignored: these are not used\n"
        "for disambiguation and also not checked for correctness.\n"
        "When a promotion piece is missing, queening is assumed.\n"
        "The output fen is the position after the move.\n"
        "\n"
        "The `notation' keyword controls the output move syntax. See moves(...)\n"
        "for details.\n"
);

static PyObject *
chessmovesmodule_move(PyObject *self, PyObject *args, PyObject *keywords)
{
        char *fen;
        char *moveString;
        char *notation = "san"; // default
        int len;

        static char *keywordList[] = { "fen", "move", "notation", NULL };

        if (!PyArg_ParseTupleAndKeywords(args, keywords, "ss|s:move", keywordList,
                                         &fen, &moveString, &notation))
                return NULL;

        struct board board;
        len = setupBoard(&board, fen);
        if (len <= 0)
                return PyErr_Format(PyExc_ValueError, "Invalid FEN (%s)", fen);

        int notationIndex;
        for (notationIndex=0; notationIndex<nrNotations; notationIndex++)
                if (0==strcmp(notations[notationIndex], notation))
                        break; // found

        if (notationIndex >= nrNotations) // not found
                return PyErr_Format(PyExc_ValueError, "Invalid notation (%s)", notation);

        int moveList[maxMoves];
        updateSideInfo(&board);
        int nrMoves = generateMoves(&board, moveList);

        int move;
        len = parseMove(&board, moveString, moveList, nrMoves, &move);
        if (len == 0)
                return PyErr_Format(PyExc_ValueError, "Invalid move syntax (%s)", moveString);
        if (len == -1)
                return PyErr_Format(PyExc_ValueError, "Illegal move (%s)", moveString);
        if (len == -2)
                return PyErr_Format(PyExc_ValueError, "Ambiguous move (%s)", moveString);

        char newFen[maxFenSize];

        char newMoveString[maxMoveSize];
        char *s = newMoveString;
        const char *checkmark;

        makeMove(&board, move);
        boardToFen(&board, newFen);

        switch (notationIndex) {
        case uciNotation:
                undoMove(&board);
                s = moveToUci(&board, s, move);
                break;
        case sanNotation:
                updateSideInfo(&board);
                checkmark = getCheckMark(&board);
                undoMove(&board);
                s = moveToStandardAlgebraic(&board, s, move, moveList, nrMoves);
                s = stringCopy(s, checkmark);
                break;
        case longNotation:
                undoMove(&board);
                checkmark = getCheckMark(&board);
                undoMove(&board);
                s = moveToLongAlgebraic(&board, s, move);
                s = stringCopy(s, checkmark);
                break;
        default:
                assert(0);
        }

        PyObject *result = PyTuple_New(2);
        if (!result)
                return NULL;

        if (PyTuple_SetItem(result, 0, PyString_FromString(newMoveString))) {
                Py_DECREF(result);
                return NULL;
        }

        if (PyTuple_SetItem(result, 1, PyString_FromString(newFen))) {
                Py_DECREF(result);
                return NULL;
        }

        return result;
}

/*----------------------------------------------------------------------+
 |      hash(...)                                                       |
 +----------------------------------------------------------------------*/

PyDoc_STRVAR(hash_doc,
        "hash(fen) -> hash\n"
        "\n"
        "Compute the Zobrist-Polyglot hash for the position."
);

static PyObject *
chessmovesmodule_hash(PyObject *self, PyObject *args)
{
        char *fen;

        if (!PyArg_ParseTuple(args, "s", &fen)) {
                return NULL;
        }

        struct board board;
        int len = setupBoard(&board, fen);
        if (len <= 0) {
                return PyErr_Format(PyExc_ValueError, "Invalid FEN");
        }

        unsigned long long hashkey = hash64(&board);

        return PyLong_FromUnsignedLongLong(hashkey);
}

/*----------------------------------------------------------------------+
 |      Method table                                                    |
 +----------------------------------------------------------------------*/

static PyMethodDef chessmovesMethods[] = {
	{ "moves",    (PyCFunction)chessmovesmodule_moves, METH_VARARGS|METH_KEYWORDS, moves_doc },
	{ "position", chessmovesmodule_position,           METH_VARARGS,               position_doc },
	{ "hash",     chessmovesmodule_hash,               METH_VARARGS,               hash_doc },
	{ "move",     (PyCFunction)chessmovesmodule_move,  METH_VARARGS|METH_KEYWORDS, move_doc },
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
                        Py_DECREF(list);
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

