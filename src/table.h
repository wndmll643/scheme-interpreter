#ifndef TABLE_H
#define TABLE_H

#include "node.h"
#include "builtin.h"
#include <string>

class NodeArray {
private:
    // std::string table_name;
    size_t      size_of_table;
    node_t*     table;
    int         free_root;
public:
    NodeArray(size_t size);
    ~NodeArray();

    int  alloc();
    void freeNode(int idx);

    void doubleSize();

    void printTable() const;

    // std::string getTableName() const { return table_name; }
    std::string getTableType() const { return "NodeArray"; }
    size_t      getSizeOfTable() const { return size_of_table; }
    node_t*     getTable() const { return table; }
    int         getFreeRoot() const { return free_root; }
    void        setFreeRoot(int idx) { free_root = idx; }
};

class HashTable {
private:
    // std::string  table_name;
    size_t       size_of_table;
    hash_node_t* table;

    static inline int indexFromEntry(int entry) { return -entry; }
    static inline int entryFromIndex(int index) { return -index; }
public:
    HashTable(size_t size);
    ~HashTable();

    void insert(const std::string& key, int index, NodeArray* na = nullptr);
    int  search(const std::string& key, int index) const;

    int  intern(const std::string& key, NodeArray* na = nullptr);

    const std::string* nameByIndex(int index) const;

    void doubleSize(NodeArray* na);

    void printTable() const;

    // std::string getTableName() const { return table_name; }
    std::string getTableType() const { return "HashTable"; }
    size_t      getSizeOfTable() const { return size_of_table; }
    hash_node_t* getTable() const { return table; }
};

int _hash(const std::string& str, size_t table_size);

NodeArray* _init_NodeArray(size_t size);
HashTable* _init_HashTable(size_t size);

#endif