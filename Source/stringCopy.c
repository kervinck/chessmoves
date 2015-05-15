
/*----------------------------------------------------------------------+
 |      stringCopy.c                                                    |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      Includes                                                        |
 +----------------------------------------------------------------------*/

#include "stringCopy.h"

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      stringCopy                                                      |
 +----------------------------------------------------------------------*/

extern char *stringCopy(char *s, const char *t)
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
