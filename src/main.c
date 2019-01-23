#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dfp/stdlib.h"
#include "dfp/math.h"
#include "dfp/fenv.h"

#define UNOP(op) {#op, op##d64}
#define BINOP(op) {#op, op##d64}
#define TERNOP(op) {#op, op##d64}
#define ACTION(op) {#op, op}

int const stksize = 1 << 20;

void fatal(char * fmt, ...) {
    fprintf(stderr, "ERROR: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    if (fmt[strlen(fmt) - 1] != '\n') {
        fprintf(stderr, "\n");
    }
    exit(1);
}

void constant(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 c, char const * name) {
    *--*bos = c;
}

void unop(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 (*f)(_Decimal64), char const * name) {
    if (*bos == stk) fatal("%s: need at least one arg on stack", name);
    **bos = f(**bos);
}

void binop(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 (*f)(_Decimal64, _Decimal64), char const * name) {
    if (*bos == stk) fatal("%s: need at least two args on stack", name);
    ++*bos;
    **bos = f(**bos, (*bos)[-1]);
}

void ternop(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 (*f)(_Decimal64, _Decimal64, _Decimal64), char const * name) {
    if (*bos == stk) fatal("%s: need at least three args on stack", name);
    *bos += 2;
    **bos = f(**bos, (*bos)[-1], (*bos)[-2]);
}

_Decimal64 add(_Decimal64 a, _Decimal64 b) { return a + b; }
_Decimal64 sub(_Decimal64 a, _Decimal64 b) { return a - b; }
_Decimal64 mul(_Decimal64 a, _Decimal64 b) { return a * b; }
_Decimal64 div_(_Decimal64 a, _Decimal64 b) { return a / b; }

typedef struct Constant_t {
    char * name;
    _Decimal64 c;
} Constant;

typedef struct UnOp_t {
    char * name;
    _Decimal64 (*op)(_Decimal64);
} UnOp;

typedef struct BinOp_t {
    char * name;
    _Decimal64 (*op)(_Decimal64, _Decimal64);
} BinOp;

typedef struct TernOp_t {
    char * name;
    _Decimal64 (*op)(_Decimal64, _Decimal64, _Decimal64);
} TernOp;

typedef struct Action_t {
    char * name;
    void (*action)(_Decimal64 * * bos, _Decimal64 * stk);
} Action;

_Decimal64 makeConstant(char const * s) {
    return strtod64(s, NULL);
}

UnOp unops[] = {
    UNOP(acos),
    UNOP(asin),
    UNOP(atan),
    UNOP(cos),
    UNOP(sin),
    UNOP(tan),
    UNOP(cosh),
    UNOP(sinh),
    UNOP(tanh),
    UNOP(acosh),
    UNOP(asinh),
    UNOP(atanh),
    UNOP(exp),
    UNOP(log),
    UNOP(log10),
    UNOP(expm1),
    UNOP(log1p),
    UNOP(logb),
    UNOP(exp2),
    UNOP(log2),
    UNOP(sqrt),
    UNOP(cbrt),
    UNOP(ceil),
    UNOP(fabs),
    UNOP(floor),
    // UNOP(significand),
    // UNOP(j0),
    // UNOP(j1),
    // UNOP(y0),
    // UNOP(y1),
    UNOP(erf),
    UNOP(erfc),
    UNOP(lgamma),
    UNOP(tgamma),
    // UNOP(gamma),
    UNOP(rint),
    UNOP(nearbyint),
    UNOP(round),
    UNOP(roundeven),
    UNOP(trunc),
    UNOP(quantum),
};

BinOp binops[] = {
    BINOP(atan2),
    BINOP(pow),
    BINOP(hypot),
    BINOP(fmod),
    // BINOP(drem),
    BINOP(copysign),
    BINOP(nextafter),
    BINOP(remainder),
    BINOP(fdim),
    BINOP(fmax),
    BINOP(fmin),
    // BINOP(scalb),
    BINOP(quantize),
};

TernOp ternops[] = {
    TERNOP(fma),
};


void dumpstk(_Decimal64 * * bos, _Decimal64 * stk) {
    printf("[");
    for (_Decimal64 * p = stk; p-- > *bos;) {
        printf(" %Dg" + (p == stk - 1), *p);
    }
    printf("]\n");
}

void dup(_Decimal64 * * bos, _Decimal64 * stk) {
    if (*bos == stk) fatal("can't dup with empty stack");
    --*bos;
    **bos = (*bos)[1];
}

void pop(_Decimal64 * * bos, _Decimal64 * stk) {
    if (*bos == stk) fatal("can't pop from empty stack");
    ++*bos;
}

void print(_Decimal64 * * bos, _Decimal64 * stk) {
    if (*bos == stk) fatal("can't print from empty stack");
    printf("%.16Da\n", *(*bos)++);
    fflush(stdout);
}

Action actions[] = {
    ACTION(dumpstk),
    ACTION(dup),
    ACTION(pop),
};

void printName(char * name, int * col) {
    int w = strlen(name) + 2;
    int first = *col == 4;
    if ((*col += w) >= 64) {
        fprintf(stderr, "\n    ");
        *col = 4 + w;
    } else if (!first) {
        fprintf(stderr, ", ");
    }
    fprintf(stderr, "%s", name);
}

typedef struct ArgIter_t {
    int argc;
    char * * argv;
    int i;
} ArgIter;

char * nextArg(void * state) {
    ArgIter * iter = (ArgIter *)state;
    return iter->i < iter->argc ? strdup(iter->argv[iter->i++]) : NULL;
}

char * nextStdinWord(void * state) {
    static char buf[1024];
    return scanf("%1023s", buf) == 1 ? strdup(buf) : NULL;
}

int main(int argc, char * argv[]) {
    Constant constants[] = {
        {"pi", makeConstant("3.141592653589793")},
        {"π", makeConstant("3.141592653589793")},
        {"e", makeConstant("2.718281828459045")},
    };

    if (argc == 1) {
        fprintf(
            stderr,
            "Usage: dectest op op ...\n"
            "       dectest -\n"
            "\n"
            "Decimal64 RPN calculator\n"
            "\n"
            "-  = read op op ... from stdin\n"
            "op = one of: a decimal64, +, -, *, /, = (print) or...");

        int col = 4;
        fprintf(stderr, "\n  constants:\n    ");
        for (int i = 0; i < sizeof(constants) / sizeof(*constants); ++i) {
            printName(constants[i].name, &col);
        }

        col = 4;
        fprintf(stderr, "\n  actions:\n    ");
        for (int i = 0; i < sizeof(actions) / sizeof(*actions); ++i) {
            printName(actions[i].name, &col);
        }

        col = 4;
        fprintf(stderr, "\n  unary operations:\n    ");
        for (int i = 0; i < sizeof(unops) / sizeof(*unops); ++i) {
            printName(unops[i].name, &col);
        }

        col = 4;
        fprintf(stderr, "\n  binary operations:\n    ");
        for (int i = 0; i < sizeof(binops) / sizeof(*binops); ++i) {
            printName(binops[i].name, &col);
        }

        col = 4;
        fprintf(stderr, "\n  ternary operations:\n    ");
        for (int i = 0; i < sizeof(ternops) / sizeof(*ternops); ++i) {
            printName(ternops[i].name, &col);
        }

        fprintf(stderr, "\n");
        exit(1);
    }
    _Decimal64 * stk = malloc(stksize * sizeof(_Decimal64)) + stksize;
    _Decimal64 * bos = stk;

    char * (*next)(void * state);
    void * state;
    if (strcmp(argv[1], "-") == 0) {
        next = nextStdinWord;
    } else {
        next = nextArg;
        ArgIter * argIter = state = malloc(sizeof(ArgIter));
        argIter->argc = argc - 1;
        argIter->argv = argv + 1;
        argIter->i = 0;
    }

    int i = 0;
    for (char * a; a = next(state); i++) {
        if (strlen(a) == 1) {
            switch (*a) {
            case '+': binop(&bos, stk, add, "+"); continue;
            case '-': binop(&bos, stk, sub, "-"); continue;
            case 'x': binop(&bos, stk, mul, "x"); continue;
            case '/': binop(&bos, stk, div_, "/"); continue;
            case '^': binop(&bos, stk, powd64, "^"); continue;
            case '=': print(&bos, stk); continue;
            }
        }
        int found = 0;

        for (int i = 0; i < sizeof(constants) / sizeof(*constants); ++i) {
            Constant * c = constants + i;
            if (strcmp(a, c->name) == 0) {
                constant(&bos, stk, c->c, c->name);
                found = 1;
                break;
            }
        }
        if (found) continue;

        for (int i = 0; i < sizeof(unops) / sizeof(*unops); ++i) {
            UnOp * op = unops + i;
            if (strcmp(a, op->name) == 0) {
                unop(&bos, stk, op->op, op->name);
                found = 1;
                break;
            }
        }
        if (found) continue;

        for (int i = 0; i < sizeof(binops) / sizeof(*binops); ++i) {
            BinOp * op = binops + i;
            if (strcmp(a, op->name) == 0) {
                binop(&bos, stk, op->op, op->name);
                found = 1;
                break;
            }
        }
        if (found) continue;

        for (int i = 0; i < sizeof(ternops) / sizeof(*ternops); ++i) {
            TernOp * op = ternops + i;
            if (strcmp(a, op->name) == 0) {
                ternop(&bos, stk, op->op, op->name);
                found = 1;
                break;
            }
        }
        if (found) continue;

        for (int i = 0; i < sizeof(actions) / sizeof(*actions); ++i) {
            Action * action = actions + i;
            if (strcmp(a, action->name) == 0) {
                action->action(&bos, stk);
                found = 1;
                break;
            }
        }
        if (found) continue;

        char * endp;
        *--bos = strtod64(a, &endp);
        if (*endp) {
            fprintf(stderr, "%d: Cannot parse %s\n", i, a);
            exit(1);
        }

        free(a);
    }
}

// TODO
// _Decimal64            ldexpd64         (_Decimal64 __x, int __exponent)
// int                   isinfd64         (_Decimal64 __value)
// int                   isfinited64      (_Decimal64 __value)
// int                   issignalingd64   (_Decimal64 __value)
// int                   fpclassifyd64    (_Decimal64 __value)
// int                   finited64        (_Decimal64 __value)
// int                   isnand64         (_Decimal64 __value)
// int                   isunorderedd64   (_Decimal64 x, _Decimal64 y)
// _Decimal64            jnd64            (int, _Decimal64)
// _Decimal64            ynd64            (int, _Decimal64)
// _Decimal64            scalbnd64        (_Decimal64 __x, int __n)
// int                   ilogbd64         (_Decimal64 __x)
// long int              llogbd64         (_Decimal64 __x)
// long long int         llquantexpd64    (_Decimal64 x)
// _Decimal64            scalblnd64       (_Decimal64 __x, long int __n)
// long int              lrintd64         (_Decimal64 __x)
// long long int         llrintd64        (_Decimal64 __x)
// long int              lroundd64        (_Decimal64 __x)
// long long int         llroundd64       (_Decimal64 __x)
// _Bool                 samequantumd64   (_Decimal64 __x, _Decimal64 __y)
// nexttowardd64,
// _Decimal64            modfd64          (_Decimal64 __x, _Decimal64 *__iptr)
// _Decimal64            frexpd64         (_Decimal64 __x, int *__exponent)
// _Decimal64            nand64           (__const char *__tagb)
// _Decimal64            remquod64        (_Decimal64 __x, _Decimal64 __y, int *__quo)
