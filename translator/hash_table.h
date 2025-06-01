#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#define HASH_TABLE_SIZE 1024

typedef struct HashNode {
    char *key;
    void *value;
    struct HashNode *next;
} HashNode;

typedef struct {
    HashNode *buckets[HASH_TABLE_SIZE];
} HashTable;

HashTable *create_hash_table(void);
void hash_table_insert(HashTable *table, const char *key, void *value);
void *hash_table_find(HashTable *table, const char *key);
void hash_table_free(HashTable *table);
unsigned int hash(const char *str);

#endif
