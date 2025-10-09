#include "table.h"
#include <cctype>
#include <cstdio>

static std::string _normalize(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s.size() > 10) s.resize(10);
    return s;
}

static inline int remap_symbol_index(int index, const int* entry_map, size_t old_size) {
    // std::printf("remap_symbol_index called with index: %d\n", index);
    if (index > 0) return index;
    int old_entry = -index;
    if (old_entry < 0 || (size_t)old_entry >= old_size) return index;
    int new_entry = entry_map[old_entry];
    // std::printf("  old_entry: %d, new_entry: %d\n", old_entry, new_entry);
    if (new_entry < 0) return index;
    return -(new_entry);
}

int _hash(const std::string& str, size_t table_size) {
    std::string s = _normalize(str);
    long long h = 0;
    for (char c : s) h += (unsigned char)c;
    long long m = (long long)table_size;
    long long r = h % m;
    return (int)r;
}

NodeArray* _init_NodeArray(size_t size) {
    return new NodeArray(size);
}

HashTable* _init_HashTable(size_t size) {
    HashTable* ht = new HashTable(size);
    const size_t N = sizeof(built_in_functions)/sizeof(built_in_functions[0]);
    for (size_t i = 0; i < N; ++i) {
        int start = _hash(built_in_functions[i], size);
        ht->insert(built_in_functions[i], start);
    }
    return ht;
}

NodeArray::NodeArray(size_t size)
: size_of_table(size), table(nullptr), free_root(0)
{
    table = new node_t[size_of_table];
    for (size_t i = 0; i < size_of_table; ++i) {
        table[i].lchild = 0;
        table[i].rchild = (i == 0) ? 0 : ((i == size_of_table - 1) ? 0 : (int)(i + 1));
    }
    // 0 is for NULL list
    free_root = (size_of_table > 1) ? 1 : 0;
}

NodeArray::~NodeArray() {
    delete[] table;
}

int NodeArray::alloc() {
    if (free_root == 0) {
        doubleSize();
        if (free_root == 0) return 0;
    }
    int idx = free_root;
    free_root = table[idx].rchild;
    table[idx].lchild = 0;
    table[idx].rchild = 0;
    return idx;
}

void NodeArray::freeNode(int idx) {
    if (idx <= 0 || (size_t)idx >= size_of_table) return;
    table[idx].lchild = 0;
    table[idx].rchild = free_root;
    free_root = idx;
}

void NodeArray::doubleSize() {
    size_t old_size = size_of_table;
    size_t new_size = old_size * 2;
    node_t* new_table = new node_t[new_size];

    for (size_t i = 0; i < old_size; ++i) {
        new_table[i] = table[i];
    }

    for (size_t i = old_size; i < new_size; ++i) {
        new_table[i].lchild = 0;
        new_table[i].rchild = (i == new_size - 1) ? 0 : (int)(i + 1);
    }

    if (free_root == 0) {
        free_root = (int)old_size;
    } else {
        // should not reach here
        printf("Unwanted behavior\n");
        int tail = free_root;
        while (new_table[tail].rchild != 0) tail = new_table[tail].rchild;
        new_table[tail].rchild = (int)old_size;
    }

    delete[] table;
    table = new_table;
    size_of_table = new_size;
}

void NodeArray::printTable() const {
    std::printf("Node array =\n");
    for (size_t i = 0; i < size_of_table; ++i) {
        if (table[i].lchild < 0) std::printf("[index=%zu] lchild=%d rchild=%d\n", i, table[i].lchild, table[i].rchild);
        else std::printf("[index=%zu] lchild=%d rchild=%d\n", i, table[i].lchild, table[i].rchild);
    }
}

HashTable::HashTable(size_t size)
: size_of_table(size), table(nullptr)
{
    table = new hash_node_t[size_of_table];
    table[0].lchild = "()"; table[0].rchild = 0;
    for (size_t i = 1; i < size_of_table; ++i) {
        table[i].lchild = "";
        table[i].rchild = 0;
    }
}

HashTable::~HashTable() {
    delete[] table;
}

int HashTable::search(const std::string& key, int index) const {
    std::string norm = _normalize(key);
    const int m = (int)size_of_table;
    for (int i = 0; i < m; ++i) {
        int j = (index + i) % m;
        if (table[j].lchild == "" && j != 0) return -1;
        if (table[j].lchild == norm && j != 0) return j;
    }
    return -1;
}

void HashTable::insert(const std::string& key, int index, NodeArray* na) {
    std::string norm = _normalize(key);

    int s = search(norm, index);
    if (s >= 0) return;

    const int m = (int)size_of_table;
    for (int i = 0; i < m; ++i) {
        int j = (index + i) % m;
        if (table[j].lchild == "" && j != 0) {
            table[j].lchild = norm;
            table[j].rchild = 0;
            return;
        }
    }
    doubleSize(na);

    int start = _hash(norm, size_of_table);
    for (size_t i = 0; i < size_of_table; ++i) {
        size_t j = (start + i) % size_of_table;
        if (table[j].lchild == "" && j != 0) {
            table[j].lchild = norm;
            table[j].rchild = 0;
            return;
        }
        if (table[j].lchild == norm) return;
    }
    std::fprintf(stderr, "[HashTable] insert failed after doubling: %s\n", norm.c_str());
}

int HashTable::intern(const std::string& key, NodeArray* na) {
    std::string norm = _normalize(key);
    // std::printf("intern called with key: '%s', normalized: '%s'\n", key.c_str(), norm.c_str());
    int start = _hash(norm, size_of_table);
    int entry  = search(norm, start);
    if (entry < 0) {
        insert(norm, start, na);
        // std::printf("Inserted symbol: %s\n", norm.c_str());
        entry = search(norm, _hash(norm, size_of_table));
        // std::printf("After insert, search returned entry: %d\n", entry);
        if (entry < 0) {
            std::fprintf(stderr, "[HashTable] overflow on intern: %s\n", norm.c_str());
            return 0;
        }
    }
    return indexFromEntry(entry);
}

const std::string* HashTable::nameByIndex(int index) const {
    if (index > 0) return nullptr;
    int entry = entryFromIndex(index);
    // std::printf("nameByIndex called with index: %d, entry: %d\n", index, entry);
    if ((size_t)entry >= size_of_table) return nullptr;
    if (table[entry].lchild == "" && entry != 0) return nullptr;
    return &table[entry].lchild;
}

void HashTable::doubleSize(NodeArray* na) {
    size_t old_size = size_of_table;
    size_t new_size = old_size * 2;
    hash_node_t* new_table = new hash_node_t[new_size];
    for (size_t i = 0; i < new_size; ++i) { new_table[i].lchild = ""; new_table[i].rchild = 0; }

    int* entry_map = new int[old_size];
    entry_map[0] = 0;
    for (size_t i = 1; i < old_size; ++i) entry_map[i] = -1;

    new_table[0].lchild = "()"; new_table[0].rchild = 0;
    for (size_t i = 1; i < old_size; ++i) {
        if (table[i].lchild != "") {
            const std::string& s = table[i].lchild;
            int start = _hash(s, new_size);
            int insert_j = -1;
            for (size_t k = 0; k < new_size; ++k) {
                size_t j = (start + k) % new_size;
                if (new_table[j].lchild == "") { insert_j = (int)j; break; }
            }
            if (insert_j < 0) {
                std::fprintf(stderr, "[HashTable] double rehash overflow (unexpected)\n");
                delete[] entry_map; delete[] new_table; return;
            }
            new_table[insert_j].lchild = s;
            new_table[insert_j].rchild = table[i].rchild;
            // printf("Rehashed '%s' from index %zu to new index %d\n", s.c_str(), i, insert_j);
            entry_map[i] = insert_j;
        }
    }

    if (na) {
        node_t* nt = na->getTable();
        size_t nsz = na->getSizeOfTable();
        for (size_t i = 1; i < nsz; ++i) {
            if (nt[i].lchild < 0) {
                nt[i].lchild = remap_symbol_index(nt[i].lchild, entry_map, old_size);
                // int temp = remap_symbol_index(nt[i].lchild, entry_map, old_size);

            }
        }
    }

    delete[] table;
    table = new_table;
    size_of_table = new_size;

    delete[] entry_map;
}

void HashTable::printTable() const {
    std::printf("Hash table =\n");
    for (size_t i = 0; i < size_of_table; ++i) {
        // if (i == 0) {
        //     std::printf("[idx=%zu] hash=%d symbol=%s body=%d\n", i, 0, table[i].lchild.c_str(), table[i].rchild);
        // }
        if (table[i].lchild != "") {
            int raw = _hash(table[i].lchild, (int)size_of_table);
            std::printf("[idx=%zu] hash=%d symbol=%s body=%d\n", i, raw, table[i].lchild.c_str(), table[i].rchild);
        }
    }
}