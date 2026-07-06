/* tal_rt.h -- minimal TAL runtime.
 *
 * The seed of the Guardian shim: the procedures a transpiled TAL program
 * calls. Grows as the switch needs sockets, timers, file I/O, etc. For now,
 * just enough to make a program observably do something.
 */
#ifndef TAL_RT_H
#define TAL_RT_H

#include <stdio.h>

static long putint(long n) { printf("%ld\n", n); return 0; }
static long putstr(const char *s) { fputs(s, stdout); return 0; }

#endif
