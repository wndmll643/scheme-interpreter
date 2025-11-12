#include "tokenizer.h"
#include "table.h"
#include "eval.h"
#include <cstdio>
#include <iostream>

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
        tokenize_line(line, ts);
        preprocess(ts);
        int root = parse_and_build(ts, *ht, *na);

        // TODO: now we have to evaluate the parse tree to do some work

        std::printf("] ");
        PRINT(root, *ht, *na);
        std::printf("\n");
        // std::printf("Free list's root = %d\n", na->getFreeRoot());
        // std::printf("Parse tree's root = %d\n\n", root);

        // na->printTable();
        // std::printf("\n");
        // ht->printTable();
        // std::printf("\n");

        int val = EVAL(root, *ht, *na);
        // there should be a way to divide if output is
        // 1) from definition of a function or variable
        // 2) from evaluation of an expression
        // std::printf("val = %d\n", val);
        PRINT(val, *ht, *na);
        std::printf("\n");

        if (root > 0) free_parse_tree(root, *na);
    }
    delete ht; delete na;
    return 0;
}
