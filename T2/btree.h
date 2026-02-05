#ifndef BTREE_H
#define BTREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 50
#define ORDER 3
#define MAX_KEYS (ORDER - 1)
#define MIN_KEYS (ORDER / 2)

typedef struct {
    char name[MAX_NAME_LEN];
    int threshold;
    long data_offset;
    int data_size;
    int width;
    int height;
} BTreeKey;

typedef struct {
    int is_leaf;
    int num_keys;
    long children[ORDER];
    BTreeKey keys[MAX_KEYS];
    long self_offset;
} BTreeNode;

typedef struct {
    long root_offset;
    long free_offset;
    int node_count;
} BTreeHeader;

// Interface pública da Árvore-B
void btree_init();
void btree_insert(BTreeKey key);
void btree_delete(const char* name, int threshold);
int btree_search(const char* name, int threshold, BTreeKey* result);
void btree_print_inorder();
void btree_print_pages();
long btree_get_root_offset();

#endif