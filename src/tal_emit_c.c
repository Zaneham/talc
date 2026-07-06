/* tal_emit_c.c -- TAL AST -> C backend (talc).
 *
 * Walks the pn_node_t tree the parser builds and emits portable C.
 * Transpiling to C lets the host C compiler do codegen, so this stays
 * small and the language grows feature-by-feature as the switch needs it.
 *
 * MVP subset: procs, INT/STRING/etc declarations, assignment, IF/WHILE,
 * RETURN, CALL, arithmetic/relational/boolean expressions, $std funcs.
 * Known frontend limit: pn_node_t has kids[8], so blocks/files cap at 8
 * children for now (next job: overflow pool, like Karearea's ovfl[]).
 */

#include "tal_emit.h"
#include <ctype.h>

typedef struct {
    const parse_ctx_t *P;
    const tal_lex_t   *L;
    FILE              *out;
} emit_t;

/* ---- small helpers ---- */

static const tal_tok_t *ntok(emit_t *E, uint32_t node) {
    return &E->P->toks[E->P->nodes[node].tok];
}

/* emit an identifier's text, sanitising TAL's '^' to C's '_' */
static void emit_ident(emit_t *E, uint32_t tokidx) {
    const tal_tok_t *t = &E->P->toks[tokidx];
    const char *s = tal_str(E->L, t->off);
    for (uint32_t i = 0; i < t->len; i++)
        fputc(s[i] == '^' ? '_' : s[i], E->out);
}

static const char *op_str(uint16_t k) {
    switch (k) {
    case TK_PLUS: case TK_PLUS_UNSIGNED:   return "+";
    case TK_MINUS: case TK_MINUS_UNSIGNED: return "-";
    case TK_STAR: case TK_STAR_UNSIGNED:   return "*";
    case TK_SLASH: case TK_SLASH_UNSIGNED: return "/";
    case TK_LSHIFT: return "<<";
    case TK_RSHIFT: return ">>";
    case TK_EQ:  return "==";
    case TK_NEQ: return "!=";
    case TK_LT:  return "<";
    case TK_GT:  return ">";
    case TK_LE:  return "<=";
    case TK_GE:  return ">=";
    case TK_AND: return "&&";
    case TK_OR:  return "||";
    case TK_LAND: return "&";
    case TK_LOR:  return "|";
    case TK_XOR:  return "^";
    case TK_NOT:  return "!";
    default: return "/*op?*/+";
    }
}

static const char *type_str(uint16_t k) {
    switch (k) {
    case TK_INT:      return "short";          /* TAL INT = 16-bit word */
    case TK_INT32:    return "int";            /* INT(32) */
    case TK_STRING:   return "unsigned char";  /* a byte */
    case TK_REAL:     return "float";
    case TK_REAL64:   return "double";
    case TK_FIXED:    return "long long";      /* TODO: real fixed-point scaling */
    case TK_UNSIGNED: return "unsigned";
    default:          return "long";
    }
}

/* ---- expressions ---- */

static void emit_expr(emit_t *E, uint32_t idx) {
    const pn_node_t *N = &E->P->nodes[idx];
    switch (N->kind) {
    case PN_INTLIT:
        fprintf(E->out, "%lld", (long long)ntok(E, idx)->ival);
        break;
    case PN_REALLIT: {
        const tal_tok_t *t = ntok(E, idx);
        fprintf(E->out, "%.*s", (int)t->len, tal_str(E->L, t->off));
        break;
    }
    case PN_STRLIT: {
        const tal_tok_t *t = ntok(E, idx);
        fprintf(E->out, "\"%.*s\"", (int)t->len, tal_str(E->L, t->off));
        break;
    }
    case PN_IDENT:
        emit_ident(E, N->tok);
        break;
    case PN_BINOP:
        fputc('(', E->out);
        emit_expr(E, N->kids[0]);
        fprintf(E->out, " %s ", op_str(ntok(E, idx)->kind));
        emit_expr(E, N->kids[1]);
        fputc(')', E->out);
        break;
    case PN_UNOP:
        fputc('(', E->out);
        fputs(op_str(ntok(E, idx)->kind), E->out);
        emit_expr(E, N->kids[0]);
        fputc(')', E->out);
        break;
    case PN_ARRREF:
        emit_expr(E, N->kids[0]);
        fputc('[', E->out);
        if (N->n_kids > 1) emit_expr(E, N->kids[1]);
        fputc(']', E->out);
        break;
    case PN_DOTREF:
        emit_expr(E, N->kids[0]);
        fputc('.', E->out);
        emit_expr(E, N->kids[1]);
        break;
    case PN_DEREF:
        fprintf(E->out, "(*");
        emit_expr(E, N->kids[0]);
        fputc(')', E->out);
        break;
    case PN_STDFUNC: {
        const tal_tok_t *t = ntok(E, idx);
        const char *s = tal_str(E->L, t->off);
        uint32_t i = (t->len && s[0] == '$') ? 1 : 0;
        fputs("talstd_", E->out);
        for (; i < t->len; i++) fputc(tolower((unsigned char)s[i]), E->out);
        fputc('(', E->out);
        for (uint32_t k = 0; k < N->n_kids; k++) {
            if (k) fputs(", ", E->out);
            emit_expr(E, pn_kid(E->P, idx, k));
        }
        fputc(')', E->out);
        break;
    }
    case PN_FUNCCALL:
        emit_ident(E, E->P->nodes[pn_kid(E->P, idx, 0)].tok);
        fputc('(', E->out);
        for (uint32_t k = 1; k < N->n_kids; k++) {
            if (k > 1) fputs(", ", E->out);
            emit_expr(E, pn_kid(E->P, idx, k));
        }
        fputc(')', E->out);
        break;
    case PN_EMPTY:
        fputc('0', E->out);
        break;
    default:
        fprintf(E->out, "0 /*expr?%d*/", N->kind);
        break;
    }
}

/* ---- declarations ---- */

static void emit_decl(emit_t *E, uint32_t idx) {
    const pn_node_t *N = &E->P->nodes[idx];
    fprintf(E->out, "%s ", type_str(ntok(E, idx)->kind));
    if (N->n_kids >= 1) emit_ident(E, E->P->nodes[N->kids[0]].tok);
    if (N->n_kids >= 2) { fputs(" = ", E->out); emit_expr(E, N->kids[1]); }
    fputs(";\n", E->out);
}

/* ---- statements ---- */

static void emit_stmt(emit_t *E, uint32_t idx) {
    const pn_node_t *N = &E->P->nodes[idx];
    switch (N->kind) {
    case PN_BEGIN:
        fputs("{\n", E->out);
        for (uint32_t k = 0; k < N->n_kids; k++) emit_stmt(E, pn_kid(E->P, idx, k));
        fputs("}\n", E->out);
        break;
    case PN_DECL:
        emit_decl(E, idx);
        break;
    case PN_ASSIGN:
        emit_expr(E, N->kids[0]);
        fputs(" = ", E->out);
        emit_expr(E, N->kids[1]);
        fputs(";\n", E->out);
        break;
    case PN_IF:
        fputs("if (", E->out);
        emit_expr(E, N->kids[0]);
        fputs(") ", E->out);
        emit_stmt(E, N->kids[1]);
        if (N->n_kids >= 3) { fputs("else ", E->out); emit_stmt(E, N->kids[2]); }
        break;
    case PN_WHILE:
        fputs("while (", E->out);
        emit_expr(E, N->kids[0]);
        fputs(") ", E->out);
        emit_stmt(E, N->kids[1]);
        break;
    case PN_RETURN:
        if (N->n_kids) { fputs("return ", E->out); emit_expr(E, N->kids[0]); fputs(";\n", E->out); }
        else fputs("return 0;\n", E->out);
        break;
    case PN_CALL:
        if (N->n_kids) {
            emit_ident(E, E->P->nodes[N->kids[0]].tok);
            fputc('(', E->out);
            for (uint32_t k = 1; k < N->n_kids; k++) {
                if (k > 1) fputs(", ", E->out);
                emit_expr(E, pn_kid(E->P, idx, k));
            }
            fputs(");\n", E->out);
        }
        break;
    case PN_LABEL:
        emit_ident(E, N->tok);
        fputs(":;\n", E->out);
        break;
    case PN_GOTO:
        fputs("goto ", E->out);
        if (N->n_kids) emit_ident(E, E->P->nodes[N->kids[0]].tok);
        fputs(";\n", E->out);
        break;
    case PN_EMPTY:
        fputs(";\n", E->out);
        break;
    case PN_IDENT: case PN_BINOP: case PN_STDFUNC: case PN_FUNCCALL:
        emit_expr(E, idx);
        fputs(";\n", E->out);
        break;
    default:
        fprintf(E->out, "/* unsupported stmt %d */;\n", N->kind);
        break;
    }
}

/* ---- procedures ---- */

static int proc_name_node(emit_t *E, uint32_t proc) {
    const pn_node_t *N = &E->P->nodes[proc];
    for (uint32_t k = 0; k < N->n_kids; k++) {
        uint32_t kid = pn_kid(E->P, proc, k);
        if (E->P->nodes[kid].kind == PN_IDENT) return (int)kid;
    }
    return -1;
}

static void emit_proc(emit_t *E, uint32_t proc, int proto_only) {
    const pn_node_t *N = &E->P->nodes[proc];
    int name = proc_name_node(E, proc);

    uint32_t params[256]; int np = 0;
    int body = -1;
    for (uint32_t k = 0; k < N->n_kids; k++) {
        uint32_t kid = pn_kid(E->P, proc, k);
        uint8_t ck = E->P->nodes[kid].kind;
        if (ck == PN_PARAM) { if (np < 256) params[np++] = kid; }
        else if (ck == PN_BEGIN) body = (int)kid;
    }

    fputs("long ", E->out);
    if (name >= 0) emit_ident(E, E->P->nodes[name].tok);
    fputc('(', E->out);
    if (np == 0) fputs("void", E->out);
    else for (int i = 0; i < np; i++) {
        if (i) fputs(", ", E->out);
        fputs("long ", E->out);
        emit_ident(E, E->P->nodes[params[i]].tok);
    }
    fputc(')', E->out);

    if (proto_only) { fputs(";\n", E->out); return; }

    fputs(" {\n", E->out);
    if (body >= 0) {
        const pn_node_t *B = &E->P->nodes[body];
        for (uint32_t k = 0; k < B->n_kids; k++) emit_stmt(E, pn_kid(E->P, (uint32_t)body, k));
    }
    fputs("return 0;\n}\n\n", E->out);
}

/* ---- program ---- */

void tal_emit_c(FILE *out, const parse_ctx_t *P, const tal_lex_t *L, uint32_t root) {
    emit_t E = { P, L, out };
    const pn_node_t *R = &P->nodes[root];

    fputs("/* Generated from TAL by talc. Do not edit. */\n", out);
    fputs("#include \"tal_rt.h\"\n\n", out);

    /* global declarations */
    for (uint32_t k = 0; k < R->n_kids; k++) {
        uint32_t kid = pn_kid(P, root, k);
        if (P->nodes[kid].kind == PN_DECL) emit_decl(&E, kid);
    }
    fputc('\n', out);

    /* prototypes */
    for (uint32_t k = 0; k < R->n_kids; k++) {
        uint32_t kid = pn_kid(P, root, k);
        if (P->nodes[kid].kind == PN_PROC) emit_proc(&E, kid, 1);
    }
    fputc('\n', out);

    /* definitions; entry = the MAIN-flagged proc, else the last one */
    int entry = -1, last = -1;
    for (uint32_t k = 0; k < R->n_kids; k++) {
        uint32_t kid = pn_kid(P, root, k);
        if (P->nodes[kid].kind != PN_PROC) continue;
        emit_proc(&E, kid, 0);
        if (P->nodes[kid].flags & 1u) entry = (int)kid;
        last = (int)kid;
    }
    if (entry < 0) entry = last;

    /* TAL procs can't be named 'main' (it's a keyword), so always wrap */
    if (entry >= 0) {
        int nm = proc_name_node(&E, (uint32_t)entry);
        fputs("int main(void) { return (int)", out);
        if (nm >= 0) emit_ident(&E, P->nodes[nm].tok);
        fputs("(); }\n", out);
    }
}
