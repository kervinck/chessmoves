
/*----------------------------------------------------------------------+
 |      readLine.c                                                      |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      Includes                                                        |
 +----------------------------------------------------------------------*/

/*
 *  Standard includes
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  Own include
 */

#include "readLine.h"

/*----------------------------------------------------------------------+
 |      readLine                                                        |
 +----------------------------------------------------------------------*/

extern int readLine(FILE *fp, char **pLine, int *pMallocSize)
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
 |                                                                      |
 +----------------------------------------------------------------------*/

