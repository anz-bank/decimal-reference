#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dfp/stdlib.h"
#include "dfp/math.h"
#include "dfp/fenv.h"

#define UNFUN(fun) {#fun, fun##d64}
#define BINFUN(fun) {#fun, fun##d64}
#define TERNFUN(fun) {#fun, fun##d64}
#define ACTION(action) {#action, action}

#define BINOP(op, fun) {#op, fun}

#define NAMED_ACTION(name, action) {#name, action}

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

void unfun(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 (*f)(_Decimal64), char const * name) {
    if (*bos == stk) fatal("%s: need at least one arg on stack", name);
    **bos = f(**bos);
}

void binfun(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 (*f)(_Decimal64, _Decimal64), char const * name) {
    if (*bos > stk - 2) fatal("%s: need at least two args on stack", name);
    ++*bos;
    **bos = f(**bos, (*bos)[-1]);
}

void ternfun(_Decimal64 * * bos, _Decimal64 * stk, _Decimal64 (*f)(_Decimal64, _Decimal64, _Decimal64), char const * name) {
    if (*bos > stk - 3) fatal("%s: need at least three args on stack", name);
    *bos += 2;
    **bos = f(**bos, (*bos)[-1], (*bos)[-2]);
}

_Decimal64 add(_Decimal64 a, _Decimal64 b) { return a + b; }
_Decimal64 sub(_Decimal64 a, _Decimal64 b) { return a - b; }
_Decimal64 mul(_Decimal64 a, _Decimal64 b) { return a * b; }
_Decimal64 div_(_Decimal64 a, _Decimal64 b) { return a / b; }
_Decimal64 not(_Decimal64 a) { return !a; }

_Decimal64 eq(_Decimal64 a, _Decimal64 b) { return a == b; }
_Decimal64 ne(_Decimal64 a, _Decimal64 b) { return a != b; }
_Decimal64 lt(_Decimal64 a, _Decimal64 b) { return a < b; }
_Decimal64 le(_Decimal64 a, _Decimal64 b) { return a <= b; }
_Decimal64 gt(_Decimal64 a, _Decimal64 b) { return a > b; }
_Decimal64 ge(_Decimal64 a, _Decimal64 b) { return a >= b; }

typedef struct Constant_t {
    char * name;
    _Decimal64 c;
} Constant;

typedef struct UnFun_t {
    char * name;
    _Decimal64 (*fun)(_Decimal64);
} UnFun;

typedef struct BinFun_t {
    char * name;
    _Decimal64 (*fun)(_Decimal64, _Decimal64);
} BinFun;

typedef struct TernFun_t {
    char * name;
    _Decimal64 (*fun)(_Decimal64, _Decimal64, _Decimal64);
} TernFun;

typedef struct Action_t {
    char * name;
    void (*action)(_Decimal64 * * bos, _Decimal64 * stk);
} Action;

_Decimal64 makeConstant(char const * s) {
    return strtod64(s, NULL);
}

UnFun unfuns[] = {
    UNFUN(acos),
    UNFUN(asin),
    UNFUN(atan),
    UNFUN(cos),
    UNFUN(sin),
    UNFUN(tan),
    UNFUN(cosh),
    UNFUN(sinh),
    UNFUN(tanh),
    UNFUN(acosh),
    UNFUN(asinh),
    UNFUN(atanh),
    UNFUN(exp),
    UNFUN(log),
    UNFUN(log10),
    UNFUN(expm1),
    UNFUN(log1p),
    UNFUN(logb),
    UNFUN(exp2),
    UNFUN(log2),
    UNFUN(sqrt),
    UNFUN(cbrt),
    UNFUN(ceil),
    UNFUN(fabs),
    UNFUN(floor),
    // UNFUN(significand),
    // UNFUN(j0),
    // UNFUN(j1),
    // UNFUN(y0),
    // UNFUN(y1),
    UNFUN(erf),
    UNFUN(erfc),
    UNFUN(lgamma),
    UNFUN(tgamma),
    // UNFUN(gamma),
    UNFUN(rint),
    UNFUN(nearbyint),
    UNFUN(round),
    UNFUN(roundeven),
    UNFUN(trunc),
    UNFUN(quantum),
};

BinFun binfuns[] = {
    BINOP(==,eq),
    BINOP(!=,ne),
    BINOP(<=,le),
    BINOP(>=,ge),

    // For easier command-line
    BINOP({=,le),
    BINOP(}=,ge),

    BINFUN(atan2),
    BINFUN(pow),
    BINFUN(hypot),
    BINFUN(fmod),
    // BINFUN(drem),
    BINFUN(copysign),
    BINFUN(nextafter),
    BINFUN(remainder),
    BINFUN(fdim),
    BINFUN(fmax),
    BINFUN(fmin),
    // BINFUN(scalb),
    BINFUN(quantize),
};

TernFun ternfuns[] = {
    TERNFUN(fma),
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

int isLittleEndian() {
    union {
        unsigned char c[sizeof(unsigned long)];
        unsigned long i;
    } u = {{'\1'}};

    return u.i == 0x01;
}

void hexImpl(_Decimal64 * * bos, _Decimal64 * stk, int colons) {
    if (*bos == stk) fatal("can't print from empty stack");

    union {
        _Decimal64 d;
        unsigned char data[sizeof(_Decimal64)];
    } u = {**bos};
    if (isLittleEndian()) {
        for (int i = 0; i < sizeof(_Decimal64)/2; i++) {
            // Swap sides
            u.data[i] ^= u.data[sizeof(_Decimal64) - 1 - i];
            u.data[sizeof(_Decimal64) - 1 - i] ^= u.data[i];
        }
    }
    for (int i = 0; i < sizeof(_Decimal64); i++) {
        if (colons && i && i % 2 == 0) {
            printf(":");
        }
        printf("%02x", u.data[i]);
    }
    printf("\n");
    fflush(stdout);
}

void hex(_Decimal64 * * bos, _Decimal64 * stk) {
    hexImpl(bos, stk, 0);
}

void hexWithColons(_Decimal64 * * bos, _Decimal64 * stk) {
    hexImpl(bos, stk, 1);
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
    ACTION(hex),
    NAMED_ACTION(hex:,hexWithColons),
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
        {"inf", 1.0dd / 0.0dd},
        {"-inf", -1.0dd / 0.0dd},
        {"nan", 0.0dd / 0.0dd},
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
            "op = one of:\n"
            "  [±]d…d.d…d[E[±]d…d] (a decimal number)\n"
            "  [±]inf, nan\n"
            "  +, -, *, /, ^, !\n"
            "  =, == (same as =), !=, <, <=, >, >=\n"
            "  {, {=, }, }= (same as <, <=, >, >=, but no need to escape on cmdline)\n"
            "  ? (print)\n"
            "  hex (print encoded big-endian decimal64 bits\n"
            "  hex: (print encoded big-endian decimal64 bits with colon separators");

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
        for (int i = 0; i < sizeof(unfuns) / sizeof(*unfuns); ++i) {
            printName(unfuns[i].name, &col);
        }

        col = 4;
        fprintf(stderr, "\n  binary operations:\n    ");
        for (int i = 0; i < sizeof(binfuns) / sizeof(*binfuns); ++i) {
            printName(binfuns[i].name, &col);
        }

        col = 4;
        fprintf(stderr, "\n  ternary operations:\n    ");
        for (int i = 0; i < sizeof(ternfuns) / sizeof(*ternfuns); ++i) {
            printName(ternfuns[i].name, &col);
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
            case '+': binfun(&bos, stk, add, a); continue;
            case '-': binfun(&bos, stk, sub, a); continue;
            case 'x': binfun(&bos, stk, mul, a); continue;
            case '/': binfun(&bos, stk, div_, a); continue;
            case '^': binfun(&bos, stk, powd64, a); continue;
            case '!': unfun(&bos, stk, not, a); continue;
            case '=': binfun(&bos, stk, eq, a); continue;
            case '<': binfun(&bos, stk, lt, a); continue;
            case '{': binfun(&bos, stk, lt, a); continue;
            case '>': binfun(&bos, stk, lt, a); continue;
            case '}': binfun(&bos, stk, lt, a); continue;
            case '?': print(&bos, stk); continue;
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

        for (int i = 0; i < sizeof(unfuns) / sizeof(*unfuns); ++i) {
            UnFun * fun = unfuns + i;
            if (strcmp(a, fun->name) == 0) {
                unfun(&bos, stk, fun->fun, fun->name);
                found = 1;
                break;
            }
        }
        if (found) continue;

        for (int i = 0; i < sizeof(binfuns) / sizeof(*binfuns); ++i) {
            BinFun * fun = binfuns + i;
            if (strcmp(a, fun->name) == 0) {
                binfun(&bos, stk, fun->fun, fun->name);
                found = 1;
                break;
            }
        }
        if (found) continue;

        for (int i = 0; i < sizeof(ternfuns) / sizeof(*ternfuns); ++i) {
            TernFun * fun = ternfuns + i;
            if (strcmp(a, fun->name) == 0) {
             ternfun(&bos, stk, fun->fun, fun->name);
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
