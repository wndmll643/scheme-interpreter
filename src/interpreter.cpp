#include "tokenizer.h"
#include "table.h"
#include <cstdio>
#include <iostream>

int main() {
    HashTable* ht = _init_HashTable(31);
    NodeArray* na = _init_NodeArray(31);

    std::string line;
    while (true) {
        std::printf("> ");
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "exit") break;

        TokenStream ts;
        tokenize_line(line, ts);
        int root = parse_and_build(ts, *ht, *na);
        std::printf("] ");
        PRINT(root, *ht, *na);
        std::printf("\n");
        std::printf("Free list's root = %d\n", na->getFreeRoot());
        std::printf("Parse tree's root = %d\n\n", root);
        
        na->printTable();
        std::printf("\n");
        ht->printTable();
        std::printf("\n");

        if (root > 0) free_parse_tree(root, *na);
    }
    delete ht; delete na;
    return 0;
}

