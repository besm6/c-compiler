#include "hash_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Hash function
unsigned int hash(const char *str)
{
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % HASH_TABLE_SIZE;
}

// Hash table operations
HashTable *create_hash_table(void)
{
    HashTable *table = malloc(sizeof(HashTable));
    memset(table->buckets, 0, sizeof(table->buckets));
    return table;
}

void hash_table_insert(HashTable *table, const char *key, void *value)
{
    unsigned int index    = hash(key);
    HashNode *node        = malloc(sizeof(HashNode));
    node->key             = strdup(key);
    node->value           = value;
    node->next            = table->buckets[index];
    table->buckets[index] = node;
}

void *hash_table_find(HashTable *table, const char *key)
{
    unsigned int index = hash(key);
    HashNode *node     = table->buckets[index];
    while (node) {
        if (strcmp(node->key, key) == 0)
            return node->value;
        node = node->next;
    }
    return NULL;
}

void hash_table_free(HashTable *table)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = table->buckets[i];
        while (node) {
            HashNode *next = node->next;
            free(node->key);
            free(node->value); // Assumes value is dynamically allocated
            free(node);
            node = next;
        }
    }
    free(table);
}
