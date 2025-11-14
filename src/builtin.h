#ifndef BUILTIN_H
#define BUILTIN_H

#include <string>

const std::string built_in_functions[] = {
    "+", "-", "*", "/", "modulo",
    "cons", "car", "cdr", "list", "null?",
    "not", "=", "<", ">", "<=", ">=",
    "display", "reverse", "length", "max", "min",
    "floor", "ceiling", "define", "cond",
    "and", "or", "if", "else",
    "(", ")", "quote", "lambda"
};
#endif