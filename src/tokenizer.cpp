#include "tokenizer.h"
#include <cctype>
#include <cstdio>
#ifndef DEBUG_PREPROCESS
#define DEBUG_PREPROCESS 0
#endif

static inline std::string _normalize(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if ((int)s.size() > 10) s.resize(10);
    return s;
}

static inline void flush_symbol(std::string& cur, TokenStream& ts) {
    if (!cur.empty() && ts.count < 512) {
        ts.toks[ts.count].kind   = TK_SYMBOL;
        ts.toks[ts.count].lexeme = _normalize(cur);
        ++ts.count;
        cur.clear();
    }
}

static void expand_form(TokenStream& in, Out& out);
static bool expand_single_quote_token_if_any(TokenStream& in, Out& out);
static bool expand_leading_quotes_if_any(TokenStream& in, Out& out);
static void expand_define_sugar(TokenStream& in, Out& out);

static void expand_form(TokenStream& in, Out& out) {
    if (!in.hasMore()) return;

    if (expand_single_quote_token_if_any(in, out)) return;

    if (expand_leading_quotes_if_any(in, out)) return;

    const Token& t = in.peek();
    if (t.kind == TK_LPAREN) {
        (void)in.next();

        if (in.hasMore() && in.peek().kind == TK_SYMBOL && in.peek().lexeme == "define") {
            (void)in.next();
            expand_define_sugar(in, out);
            return;
        }

        out.emit_l();
        while (in.hasMore() && in.peek().kind != TK_RPAREN) {
            expand_form(in, out);
        }
        if (!in.hasMore() || in.peek().kind != TK_RPAREN)
            throw std::runtime_error("syntax: expected ')'");
        (void)in.next();
        out.emit_r();
        return;
    }

    if (t.kind == TK_RPAREN) {
        return;
    }

    out.emit_sym(in.next().lexeme);
}

static bool expand_single_quote_token_if_any(TokenStream& in, Out& out) {
    if (!in.hasMore()) return false;
    const Token& t = in.peek();
    if (t.kind == TK_SYMBOL && t.lexeme == "'") {
        (void)in.next();
        out.emit_l(); out.emit_sym("quote");
        expand_form(in, out);
        out.emit_r();
        return true;
    }
    return false;
}

static bool expand_leading_quotes_if_any(TokenStream& in, Out& out) {
    if (!in.hasMore()) return false;
    const Token& t = in.peek();
    if (t.kind != TK_SYMBOL) return false;

    const std::string& s = t.lexeme;
    if (s.empty() || s[0] != '\'') return false;

    int q = 0;
    while (q < (int)s.size() && s[q] == '\'') ++q;
    std::string rest = s.substr(q);
    (void)in.next(); // 현재 토큰 소비

    for (int i = 0; i < q; ++i) { out.emit_l(); out.emit_sym("quote"); }

    if (!rest.empty()) {
        out.emit_sym(rest);
    } else {
        expand_form(in, out);
    }

    for (int i = 0; i < q; ++i) out.emit_r();
    return true;
}

static void expand_define_sugar(TokenStream& in, Out& out) {
    out.emit_l();
    out.emit_sym("define");

    if (in.hasMore() && in.peek().kind == TK_LPAREN) {
        (void)in.next();
        if (!in.hasMore() || in.peek().kind != TK_SYMBOL)
            throw std::runtime_error("syntax: expected function name in define");
        std::string fname = in.next().lexeme;

        std::string params[64];
        int pcount = 0;

        while (in.hasMore() && in.peek().kind != TK_RPAREN) {
            if (in.peek().kind != TK_SYMBOL)
                throw std::runtime_error("syntax: parameter must be symbol");
            if (pcount < 64) params[pcount++] = in.next().lexeme;
            else (void)in.next();
        }
        if (!in.hasMore() || in.peek().kind != TK_RPAREN)
            throw std::runtime_error("syntax: expected ')' after parameter list");
        (void)in.next();

        out.emit_sym(fname);
        out.emit_l(); out.emit_sym("lambda");
        out.emit_l();
        for (int i = 0; i < pcount; ++i) out.emit_sym(params[i]);
        out.emit_r();

        while (in.hasMore() && in.peek().kind != TK_RPAREN) {
            expand_form(in, out);
        }
        if (!in.hasMore() || in.peek().kind != TK_RPAREN)
            throw std::runtime_error("syntax: expected ')' to close define");
        (void)in.next();

        out.emit_r();
        out.emit_r();
    } else {
        if (!in.hasMore() || in.peek().kind != TK_SYMBOL)
            throw std::runtime_error("syntax: expected name after define");
        out.emit_sym(in.next().lexeme);

        if (!in.hasMore())
            throw std::runtime_error("syntax: expected value after define name");
        expand_form(in, out);

        if (!in.hasMore() || in.peek().kind != TK_RPAREN)
            throw std::runtime_error("syntax: expected ')' to close define");
        (void)in.next();
        out.emit_r();
    }
}

void preprocess(TokenStream& ts) {
#if DEBUG_PREPROCESS
    Token before[512];
    int   before_count = ts.count;
    int   before_pos   = ts.pos;
    for (int i = 0; i < before_count; ++i) before[i] = ts.toks[i];
    {
        TokenStream tmp;
        for (int i = 0; i < before_count; ++i) tmp.toks[i] = before[i];
        tmp.count = before_count; tmp.pos = before_pos;
        dump_ts("BEFORE", tmp);
    }
#endif
    Out out;
    while (ts.hasMore()) {
        expand_form(ts, out);
    }

    ts.clear();
    int limit = out.count;
    if (limit > 512) limit = 512;
    for (int i = 0; i < limit; ++i) ts.toks[i] = out.buf[i];
    ts.count = limit;
    ts.reset();

#if DEBUG_PREPROCESS
    dump_ts("AFTER", ts);
#endif
}


void tokenize_line(const std::string& line, TokenStream& ts) {
    ts.clear();
    std::string cur;

    const int N = (int)line.size();
    for (int i = 0; i < N; ++i) {
        const char c = line[i];
        if (c == '(') {
            flush_symbol(cur, ts);
            if (ts.count < 512) {
                ts.toks[ts.count].kind = TK_LPAREN;
                ts.toks[ts.count].lexeme = "(";
                ++ts.count;
            }
        } else if (c == ')') {
            flush_symbol(cur, ts);
            if (ts.count < 512) {
                ts.toks[ts.count].kind = TK_RPAREN;
                ts.toks[ts.count].lexeme = ")";
                ++ts.count;
            }
        } else if (std::isspace((unsigned char)c)) {
            flush_symbol(cur, ts);
        } else {
            cur.push_back(c);
        }
    }
    flush_symbol(cur, ts);
    ts.reset();
}

static int parse_expr(TokenStream& ts, HashTable& ht, NodeArray& na);
static int parse_list(TokenStream& ts, HashTable& ht, NodeArray& na);

static int parse_expr(TokenStream& ts, HashTable& ht, NodeArray& na) {
    if (!ts.hasMore()) return 0;
    const Token& t = ts.peek();
    if (t.kind == TK_LPAREN) {
        (void)ts.next();
        return parse_list(ts, ht, na);
    } else if (t.kind == TK_RPAREN) {
        (void)ts.next();
        return 0;
    } else {
        int index = ht.intern(t.lexeme, &na);
        (void)ts.next();
        return index;
    }
}

static int parse_list(TokenStream& ts, HashTable& ht, NodeArray& na) {
    int first = 1;
    int root = 0;
    int tail = 0;

    while (ts.hasMore() && ts.peek().kind != TK_RPAREN) {
        int cons = na.alloc();
        if (cons == 0) {
            std::fprintf(stderr, "[NodeArray] out of space while building list\n");
            return root;
        }

        if (first) { root = tail = cons; first = 0; }
        else {
            na.getTable()[tail].rchild = cons;
            tail = cons;
        }

        int car = parse_expr(ts, ht, na);
        na.getTable()[cons].lchild = car;
    }

    if (ts.hasMore() && ts.peek().kind == TK_RPAREN) (void)ts.next();
    return root;
}

int parse_and_build(TokenStream& ts, HashTable& ht, NodeArray& na) {
    return parse_expr(ts, ht, na);
}

static void print_atom(int index, const HashTable& ht) {
    // std::printf("print_atom called with index: %d\n", index);
    if (index == 0) { std::printf("()"); return; }
    if (index < 0) {
        const std::string* name = ht.nameByIndex(index);
        if (name) std::printf("%s", name->c_str());
        else      std::printf("#sym(%d)", index);
        return;
    }
    std::printf("?");
}

static void print_expr(int root, bool startList, const HashTable& ht, const NodeArray& na) {
    if (root == 0) {
        std::printf("()");
        // if (startList) std::printf("\n");
        return;
    }
    if (root < 0) {
        print_atom(root, ht);
        // if (startList) std::printf("\n");
        return;
    }
    const node_t* tbl = na.getTable();
    if (startList) std::printf("(");

    int car = tbl[root].lchild;
    if (car > 0) print_expr(car, true, ht, na);
    else         print_atom(car, ht);

    int cdr = tbl[root].rchild;
    if (cdr == 0) {
        std::printf(")");
    } else {
        std::printf(" ");
        print_expr(cdr, false, ht, na);
    }
}

void PRINT(int root, const HashTable& ht, const NodeArray& na) {
    print_expr(root, true, ht, na);
}

static void free_tree_rec(int root, NodeArray& na) {
    if (root <= 0) return;
    node_t* tbl = na.getTable();
    if (tbl[root].lchild > 0) free_tree_rec(tbl[root].lchild, na);
    int next = tbl[root].rchild;
    na.freeNode(root);
    if (next > 0) free_tree_rec(next, na);
}

void free_parse_tree(int root, NodeArray& na) {
    free_tree_rec(root, na);
}

void dump_ts(const char* tag, const TokenStream& ts) {
    std::printf("[TokenStream %s] count=%d pos=%d\n", tag, ts.count, ts.pos);
    for (int i = 0; i < ts.count; ++i) {
        const Token& t = ts.toks[i];
        const char* k =
            (t.kind == TK_LPAREN) ? "LPAREN" :
            (t.kind == TK_RPAREN) ? "RPAREN" : "SYMBOL";
        std::printf("  %3d: %-6s \"%s\"\n", i, k, t.lexeme.c_str());
    }
}