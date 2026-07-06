# talc

talc is an open compiler for TAL (Transaction Application Language), the
systems language on HP NonStop / Tandem, the machines behind a lot of ATMs
and payment switches. It compiles TAL to C, which your C compiler then builds.

## Build

    make

## Usage

    talc program.tal -o program.c
    cc -Isrc program.c -o program

With no -o it writes the C to stdout. Samples are in samples/, and `make test`
builds and runs them all.

## What works

Front end (lexer, parser, symbol table) covers most of the language: the type
set, PROC/SUBPROC, structs, pointers, LITERAL/DEFINE, standard functions, and
the full statement set.

C backend so far: procs (including MAIN and value-returning calls), declarations
across the type set, arithmetic/relational/boolean/bitwise expressions, IF,
WHILE, RETURN, CALL, GOTO and labels, and the $ functions.

## Not yet

CASE, FOR, DO/UNTIL, MOVE, SCAN/RSCAN, CODE, USE/DROP/STORE, struct codegen,
LITERAL/DEFINE expansion, address-of, FIXED scaling. INT's 16-bit wraparound and
the signed/unsigned operator split are simplified for now.

## Sources

Built from the TAL Reference Manual (526371-001) and Programmer's Guide (096254).
Clean-room from the published manuals.

## License

Apache 2.0.

## Contact

zanehambly@gmail.com
