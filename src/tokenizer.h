#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <cstdio>
#include <stdexcept>
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

typedef struct Out {
    Token buf[2048]; // 임시로 넉넉히 확보
    int   count = 0;

    void emit(TokKind k, const std::string& s) {
        if (count >= 2048) return; // 안전하게 트렁케이트
        buf[count].kind = k;
        buf[count].lexeme = s;
        ++count;
    }
    void emit_l() { emit(TK_LPAREN, "("); }
    void emit_r() { emit(TK_RPAREN, ")"); }
    void emit_sym(const std::string& s){ emit(TK_SYMBOL, s); }
} Out;

// TODO: preprocessor must be implemented here
// 1) lambda translation
// 2) quote handling
void preprocess(TokenStream& ts);

void tokenize_line(const std::string& line, TokenStream& ts);

int parse_and_build(TokenStream& ts, HashTable& ht, NodeArray& na);

void PRINT(int root, const HashTable& ht, const NodeArray& na);

void free_parse_tree(int root, NodeArray& na);

void dump_ts(const char* tag, const TokenStream& ts);

#endif
