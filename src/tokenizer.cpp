#include "tokenizer.h"
#include <cctype>
#include <cstdio>

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

void tokenize_line(const std::string& line, TokenStream& ts, HashTable& ht, NodeArray& na) {
    ts.clear();
    std::string cur;

    const int N = (int)line.size();
    for (int i = 0; i < N; ++i) {
        const char c = line[i];
        int left_paren = ht.intern("(", &na);
        int right_paren = ht.intern(")", &na);
        if (ht.intern(std::string(1, c), &na) == left_paren) {
            flush_symbol(cur, ts);
            if (ts.count < 512) {
                ts.toks[ts.count].kind = TK_LPAREN;
                ts.toks[ts.count].lexeme = "(";
                ++ts.count;
            }
        } else if (ht.intern(std::string(1, c), &na) == right_paren) {
            // std::printf("cur %s\n", cur.c_str());
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
        if (startList) std::printf("\n");
        return;
    }
    if (root < 0) {
        print_atom(root, ht);
        if (startList) std::printf("\n");
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
