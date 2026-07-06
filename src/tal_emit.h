/* tal_emit.h -- TAL -> C backend entry point */
#ifndef TAL_EMIT_H
#define TAL_EMIT_H

#include "tal.h"
#include <stdio.h>

void tal_emit_c(FILE *out, const parse_ctx_t *P, const tal_lex_t *L, uint32_t root);

#endif
