#include "tokenizer.h"
#include "table.h"
#include "node.h"
#include "eval.h"
#include <cstdio>
#include <iostream>
#include <string>

static inline bool is_list(int n)   { return n > 0; }
static inline bool is_symbol(int n) { return n < 0; }
static inline int  CAR(const NodeArray& na, int p){ return na.getTable()[p].lchild; }

static const std::string* sym_name(const HashTable& ht, int sym_idx){
    return ht.nameByIndex(sym_idx);
}
static inline bool is_sym_name(const HashTable& ht, int sym, const char* name){
    if (!is_symbol(sym)) return false;
    const std::string* s = sym_name(ht, sym);
    return (s && *s == name);
}

int main() {
    HashTable* ht = _init_HashTable(101);
    NodeArray* na = _init_NodeArray(101);

    std::string line;
    while (true) {
        std::printf("> ");
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "exit") break;

        TokenStream ts;
        int root = 0;
        try {
            tokenize_line(line, ts);
            preprocess(ts);
            root = parse_and_build(ts, *ht, *na);

            bool suppress_output = false;
            if (is_list(root)) {
                int op = CAR(*na, root);
                if (is_sym_name(*ht, op, "define")) suppress_output = true;
            }

            int val = EVAL(root, *ht, *na);

            if (!suppress_output) {
                PRINT(val, *ht, *na);
                std::printf("\n");
            }
            else {
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "error: %s\n", e.what());
        }

        if (root > 0) free_parse_tree(root, *na);
    }
    delete ht; delete na;
    return 0;
}
