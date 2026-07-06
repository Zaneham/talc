/* talc -- the open TAL compiler (TAL -> C front).
 *
 * Reuses the tal-lsp lexer + parser, adds the C backend.
 * Usage: talc <file.tal> [-o out.c]   (default: C to stdout)
 */

#include "tal.h"
#include "tal_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: talc <file.tal> [-o out.c]\n");
        return 2;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = malloc((size_t)sz + 1);
    if (!src || fread(src, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); return 2; }
    src[sz] = '\0';
    fclose(f);

    tal_lex_t   *L = calloc(1, sizeof(*L));
    parse_ctx_t *P = calloc(1, sizeof(*P));
    if (!L || !P) { fprintf(stderr, "out of memory\n"); return 2; }

    tal_lex(L, src, (uint32_t)sz);
    uint32_t root = tal_parse(P, L->toks, L->n_toks, L);

    int errs = 0;
    for (int i = 0; i < P->n_diags; i++) {
        fprintf(stderr, "%s:%u:%u: %s\n", argv[1],
                P->diags[i].line, P->diags[i].col, P->diags[i].msg);
        if (P->diags[i].severity == 1) errs++;
    }

    FILE *out = stdout;
    if (argc >= 4 && strcmp(argv[2], "-o") == 0) {
        out = fopen(argv[3], "wb");
        if (!out) { perror(argv[3]); return 2; }
    }
    tal_emit_c(out, P, L, root);
    if (out != stdout) fclose(out);

    free(src); free(L); free(P);
    return errs ? 1 : 0;
}
