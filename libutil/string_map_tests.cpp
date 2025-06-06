//
// Copyright (c) 2025 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <gtest/gtest.h>
extern "C" {
#include "string_map.h"
}

// Test fixture for string-to-int map tests
class StringMapTest : public ::testing::Test {
protected:
    void SetUp() override { map_init(&map); }

    void TearDown() override { map_destroy(&map); }

    StringMap map;

    // Helper function to count nodes
    static int count_nodes(StringNode *node)
    {
        if (!node)
            return 0;
        return 1 + count_nodes(node->left) + count_nodes(node->right);
    }

    // Helper to check if tree is balanced
    static bool is_balanced(StringNode *node)
    {
        if (!node)
            return true;
        int bf = abs(map_node_height(node->left) - map_node_height(node->right));
        if (bf > 1)
            return false;
        return is_balanced(node->left) && is_balanced(node->right);
    }

    // Helper function to check BST property
    static bool is_bst(StringNode *node, const char *min_key, const char *max_key)
    {
        if (!node)
            return true;
        if (min_key && strcmp(node->key, min_key) <= 0)
            return false;
        if (max_key && strcmp(node->key, max_key) >= 0)
            return false;
        return is_bst(node->left, min_key, node->key) && is_bst(node->right, node->key, max_key);
    }
};

// Test map creation
TEST_F(StringMapTest, CreateStringMap)
{
    EXPECT_EQ(map.root, nullptr);
}

// Test inserting a new key-value pair
TEST_F(StringMapTest, InsertNewKey)
{
    map_insert(&map, "key1", 42, 0);
    intptr_t value;
    EXPECT_TRUE(map_get(&map, "key1", &value));
    EXPECT_EQ(value, 42);
}

// Test updating an existing key
TEST_F(StringMapTest, UpdateExistingKey)
{
    map_insert(&map, "key1", 42, 0);
    map_insert(&map, "key1", 100, 0);
    intptr_t value;
    EXPECT_TRUE(map_get(&map, "key1", &value));
    EXPECT_EQ(value, 100);
}

// Test inserting multiple keys
TEST_F(StringMapTest, InsertMultipleKeys)
{
    map_insert(&map, "apple", 5, 0);
    map_insert(&map, "banana", 10, 0);
    map_insert(&map, "orange", 15, 0);

    intptr_t value;
    EXPECT_TRUE(map_get(&map, "apple", &value));
    EXPECT_EQ(value, 5);
    EXPECT_TRUE(map_get(&map, "banana", &value));
    EXPECT_EQ(value, 10);
    EXPECT_TRUE(map_get(&map, "orange", &value));
    EXPECT_EQ(value, 15);
}

// Test getting non-existent key
TEST_F(StringMapTest, GetNonExistentKey)
{
    intptr_t value;
    EXPECT_FALSE(map_get(&map, "nonexistent", &value));
}

// Test removing a key
TEST_F(StringMapTest, RemoveKey)
{
    map_insert(&map, "key1", 42, 0);
    map_remove_key(&map, "key1");
    intptr_t value;
    EXPECT_FALSE(map_get(&map, "key1", &value));
}

// Test removing non-existent key
TEST_F(StringMapTest, RemoveNonExistentKey)
{
    map_remove_key(&map, "nonexistent");
}

// Test null inputs
TEST_F(StringMapTest, NullInputs)
{
    map_insert(nullptr, "key1", 42, 0);
    map_insert(&map, nullptr, 42, 0);
    intptr_t value;
    EXPECT_FALSE(map_get(nullptr, "key1", &value));
    EXPECT_FALSE(map_get(&map, nullptr, &value));
    EXPECT_FALSE(map_get(&map, "key1", nullptr));
    map_remove_key(nullptr, "key1");
    map_remove_key(&map, nullptr);
}

// Test balance after multiple insertions
TEST_F(StringMapTest, BalanceAfterInsertions)
{
    // Insert keys in a way that may cause imbalance
    const char *keys[] = { "a", "b", "c", "d", "e" };
    for (int i = 0; i < 5; i++) {
        map_insert(&map, keys[i], i, 0);
    }

    EXPECT_TRUE(is_balanced(map.root));
}

// Test balance after deletions
TEST_F(StringMapTest, BalanceAfterDeletions)
{
    // Insert multiple keys
    map_insert(&map, "apple", 5, 0);
    map_insert(&map, "banana", 10, 0);
    map_insert(&map, "orange", 15, 0);
    map_insert(&map, "grape", 20, 0);

    // Remove some keys
    map_remove_key(&map, "banana");
    intptr_t value;
    EXPECT_FALSE(map_get(&map, "banana", &value));
    map_remove_key(&map, "orange");
    EXPECT_FALSE(map_get(&map, "orange", &value));

    EXPECT_TRUE(is_balanced(map.root));
}

// Test empty map
TEST_F(StringMapTest, CondEmptyMap)
{
    map_remove_level(&map, 0);
    EXPECT_EQ(map.root, nullptr);
    EXPECT_TRUE(is_balanced(map.root));
    EXPECT_TRUE(is_bst(map.root, NULL, NULL));
}

// Test null inputs
TEST_F(StringMapTest, CondNullInputs)
{
    map_remove_level(NULL, 0);
    EXPECT_EQ(map.root, nullptr);
}

// Test no removals (cond always false)
TEST_F(StringMapTest, CondNoRemovals)
{
    map_insert(&map, "apple", 1, 0);
    map_insert(&map, "banana", 2, 0);
    map_insert(&map, "cherry", 3, 0);
    map_remove_level(&map, 0);
    EXPECT_EQ(count_nodes(map.root), 3);
    EXPECT_TRUE(is_balanced(map.root));
    EXPECT_TRUE(is_bst(map.root, NULL, NULL));
}

// Test remove all nodes (cond always true)
TEST_F(StringMapTest, CondRemoveAll)
{
    map_insert(&map, "apple", 1, 1);
    map_insert(&map, "banana", 2, 2);
    map_insert(&map, "cherry", 3, 3);
    map_remove_level(&map, 0);
    EXPECT_EQ(map.root, nullptr);
    EXPECT_TRUE(is_balanced(map.root));
    EXPECT_TRUE(is_bst(map.root, NULL, NULL));
}

// Test remove short keys
TEST_F(StringMapTest, CondRemoveShortKeys)
{
    map_insert(&map, "a", 1, 1);
    map_insert(&map, "bb", 2, 1);
    map_insert(&map, "ccc", 3, 1);
    map_insert(&map, "dddd", 4, 0);
    map_insert(&map, "eeeee", 5, 0);
    map_remove_level(&map, 0);
    EXPECT_EQ(count_nodes(map.root), 2); // Should keep "dddd" and "eeeee"
    EXPECT_TRUE(is_balanced(map.root));
    EXPECT_TRUE(is_bst(map.root, NULL, NULL));

    // Verify remaining keys
    StringNode *node = map.root;
    while (node) {
        EXPECT_GE(strlen(node->key), 4);
        node = node->left ? node->left : node->right;
    }
}

// Test remove keys with prefix
TEST_F(StringMapTest, CondRemovePrefix)
{
    map_insert(&map, "cat", 1, 3);
    map_insert(&map, "car", 2, 2);
    map_insert(&map, "dog", 3, 1);
    map_insert(&map, "bird", 4, 0);
    map_remove_level(&map, 1);
    EXPECT_EQ(count_nodes(map.root), 2); // Should keep "dog" and "bird"
    EXPECT_TRUE(is_balanced(map.root));
    EXPECT_TRUE(is_bst(map.root, NULL, NULL));
    StringNode *node = map.root;
    while (node) {
        EXPECT_NE(strncmp(node->key, "ca", 2), 0);
        node = node->left ? node->left : node->right;
    }
}

// Test large tree with mixed removals
TEST_F(StringMapTest, CondLargeTreeMixedRemovals)
{
    const char *keys[] = { "alpha", "beta", "gamma", "delta", "epsilon",
                           "zeta",  "eta",  "theta", "iota",  "kappa" };
    int n              = sizeof(keys) / sizeof(keys[0]);
    for (int i = 0; i < n; i++) {
        map_insert(&map, keys[i], i + 1, strlen(keys[i]));
    }
    map_remove_level(&map, 4); // Remove keys with length > 4

    // Expected remaining keys: "beta", "zeta", "eta", "iota"
    EXPECT_EQ(count_nodes(map.root), 4);
    EXPECT_TRUE(is_balanced(map.root));
    EXPECT_TRUE(is_bst(map.root, NULL, NULL));
}
