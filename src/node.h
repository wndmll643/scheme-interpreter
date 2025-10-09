#ifndef NODE_H
#define NODE_H

#include <string>

typedef struct node_t {
    int lchild;
    int rchild;
} node_t;

typedef struct hash_node_t {
    std::string lchild;
    int rchild;
} hash_node_t;

#endif