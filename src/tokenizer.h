#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include "table.h"

enum TokKind { TK_LPAREN, TK_RPAREN, TK_SYMBOL };

typedef struct Token {
    TokKind      kind;
    std::string  lexeme;
} Token;

typedef struct TokenStream {
    Token toks[512];
    int   count = 0;
    int   pos   = 0;

    bool hasMore() const { return pos < count; }
    const Token& peek() const { return toks[pos]; }
    const Token& next() { return toks[pos++]; }
    void clear() { count = 0; pos = 0; }
    void reset() { pos = 0; }
} TokenStream;

void tokenize_line(const std::string& line, TokenStream& ts, HashTable& ht);

int parse_and_build(TokenStream& ts, HashTable& ht, NodeArray& na);

void PRINT(int root, const HashTable& ht, const NodeArray& na);

void free_parse_tree(int root, NodeArray& na);

#endif
