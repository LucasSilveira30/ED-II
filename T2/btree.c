#include "btree.h"

// Variáveis estáticas - encapsulamento completo
static BTreeHeader btree_header;
static BTreeNode* btree_root = NULL;

// Funções privadas
static long btree_create_node(int is_leaf);
static BTreeNode* btree_read_node(long offset);
static void btree_write_node(long offset, BTreeNode* node);
static void btree_update_header();
static int btree_compare_keys(const BTreeKey* a, const BTreeKey* b);
static void btree_split_child(BTreeNode* parent, int index, BTreeNode* child);
static void btree_insert_non_full(BTreeNode* node, BTreeKey key);
static int btree_delete_recursive(long node_offset, BTreeKey key);
static void btree_remove_from_leaf(long node_offset, int idx);
static void btree_remove_from_non_leaf(long node_offset, int idx);
static BTreeKey btree_get_predecessor(long node_offset, int idx);
static BTreeKey btree_get_successor(long node_offset, int idx);
static void btree_fill_child(long node_offset, int idx);
static void btree_borrow_from_prev(long node_offset, int idx);
static void btree_borrow_from_next(long node_offset, int idx);
static void btree_merge(long node_offset, int idx);
static int btree_find_key_index(BTreeNode* node, const BTreeKey* key);

/**
 * Inicializa a Árvore-B (raiz virtualizada em RAM)
 */
void btree_init() {
    FILE* file = fopen("btree.dat", "rb");
    
    if (file) {
        fread(&btree_header, sizeof(BTreeHeader), 1, file);
        btree_root = btree_read_node(btree_header.root_offset);
        fclose(file);
    } else {
        file = fopen("btree.dat", "wb");
        btree_header.root_offset = btree_create_node(1);
        btree_header.free_offset = sizeof(BTreeHeader) + sizeof(BTreeNode);
        btree_header.node_count = 1;
        
        btree_root = btree_read_node(btree_header.root_offset);
        btree_update_header();
        fclose(file);
    }
}

/**
 * Cria novo nó no arquivo
 */
static long btree_create_node(int is_leaf) {
    BTreeNode node;
    node.is_leaf = is_leaf;
    node.num_keys = 0;
    node.self_offset = btree_header.free_offset;
    
    for (int i = 0; i < ORDER; i++) {
        node.children[i] = -1;
    }
    
    long offset = btree_header.free_offset;
    btree_header.free_offset += sizeof(BTreeNode);
    btree_header.node_count++;
    
    FILE* file = fopen("btree.dat", "r+b");
    fseek(file, offset, SEEK_SET);
    fwrite(&node, sizeof(BTreeNode), 1, file);
    fclose(file);
    
    return offset;
}

/**
 * Lê nó do arquivo
 */
static BTreeNode* btree_read_node(long offset) {
    if (offset == -1) return NULL;
    
    FILE* file = fopen("btree.dat", "rb");
    fseek(file, offset, SEEK_SET);
    
    BTreeNode* node = malloc(sizeof(BTreeNode));
    fread(node, sizeof(BTreeNode), 1, file);
    node->self_offset = offset;
    
    fclose(file);
    return node;
}

/**
 * Escreve nó no arquivo
 */
static void btree_write_node(long offset, BTreeNode* node) {
    FILE* file = fopen("btree.dat", "r+b");
    fseek(file, offset, SEEK_SET);
    fwrite(node, sizeof(BTreeNode), 1, file);
    fclose(file);
}

/**
 * Atualiza cabeçalho do arquivo
 */
static void btree_update_header() {
    FILE* file = fopen("btree.dat", "r+b");
    fseek(file, 0, SEEK_SET);
    fwrite(&btree_header, sizeof(BTreeHeader), 1, file);
    fclose(file);
}

/**
 * Compara chaves (nome + limiar)
 */
static int btree_compare_keys(const BTreeKey* a, const BTreeKey* b) {
    int cmp = strcmp(a->name, b->name);
    return (cmp != 0) ? cmp : (a->threshold - b->threshold);
}

/**
 * Insere chave na Árvore-B
 */
void btree_insert(BTreeKey key) {
    if (btree_root->num_keys == MAX_KEYS) {
        long new_root_offset = btree_create_node(0);
        BTreeNode* new_root = btree_read_node(new_root_offset);
        
        new_root->children[0] = btree_header.root_offset;
        btree_header.root_offset = new_root_offset;
        
        btree_split_child(new_root, 0, btree_root);
        
        free(btree_root);
        btree_root = new_root;
        
        btree_insert_non_full(btree_root, key);
    } else {
        btree_insert_non_full(btree_root, key);
    }
    
    btree_update_header();
}

/**
 * Insere em nó não cheio
 */
static void btree_insert_non_full(BTreeNode* node, BTreeKey key) {
    int i = node->num_keys - 1;
    
    if (node->is_leaf) {
        while (i >= 0 && btree_compare_keys(&key, &node->keys[i]) < 0) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        
        node->keys[i + 1] = key;
        node->num_keys++;
        btree_write_node(node->self_offset, node);
    } else {
        while (i >= 0 && btree_compare_keys(&key, &node->keys[i]) < 0) {
            i--;
        }
        i++;
        
        BTreeNode* child = btree_read_node(node->children[i]);
        
        if (child->num_keys == MAX_KEYS) {
            btree_split_child(node, i, child);
            if (btree_compare_keys(&key, &node->keys[i]) > 0) {
                i++;
                free(child);
                child = btree_read_node(node->children[i]);
            }
        }
        
        btree_insert_non_full(child, key);
        free(child);
    }
}

/**
 * Divide filho cheio
 */
static void btree_split_child(BTreeNode* parent, int index, BTreeNode* child) {
    long new_child_offset = btree_create_node(child->is_leaf);
    BTreeNode* new_child = btree_read_node(new_child_offset);
    
    new_child->num_keys = MIN_KEYS;
    
    for (int j = 0; j < MIN_KEYS; j++) {
        new_child->keys[j] = child->keys[j + ORDER / 2];
    }
    
    if (!child->is_leaf) {
        for (int j = 0; j <= MIN_KEYS; j++) {
            new_child->children[j] = child->children[j + ORDER / 2];
        }
    }
    
    child->num_keys = MIN_KEYS;
    
    for (int j = parent->num_keys; j > index; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[index + 1] = new_child_offset;
    
    for (int j = parent->num_keys - 1; j >= index; j--) {
        parent->keys[j + 1] = parent->keys[j];
    }
    
    parent->keys[index] = child->keys[MIN_KEYS];
    parent->num_keys++;
    
    btree_write_node(parent->self_offset, parent);
    btree_write_node(child->self_offset, child);
    btree_write_node(new_child_offset, new_child);
    
    free(new_child);
}

/**
 * Remove chave da Árvore-B
 */
void btree_delete(const char* name, int threshold) {
    BTreeKey key;
    strncpy(key.name, name, MAX_NAME_LEN - 1);
    key.name[MAX_NAME_LEN - 1] = '\0';
    key.threshold = threshold;
    
    if (btree_delete_recursive(btree_header.root_offset, key)) {
        if (btree_root->num_keys == 0 && !btree_root->is_leaf) {
            long old_root = btree_header.root_offset;
            btree_header.root_offset = btree_root->children[0];
            
            free(btree_root);
            btree_root = btree_read_node(btree_header.root_offset);
            btree_update_header();
        }
    }
}

/**
 * Função recursiva de remoção (página por página)
 */
static int btree_delete_recursive(long node_offset, BTreeKey key) {
    BTreeNode* node = btree_read_node(node_offset);
    int idx = btree_find_key_index(node, &key);
    
    if (idx < node->num_keys && btree_compare_keys(&node->keys[idx], &key) == 0) {
        if (node->is_leaf) {
            btree_remove_from_leaf(node_offset, idx);
        } else {
            btree_remove_from_non_leaf(node_offset, idx);
        }
        free(node);
        return 1;
    }
    
    if (node->is_leaf) {
        free(node);
        return 0;
    }
    
    int last_child = (idx == node->num_keys);
    BTreeNode* child = btree_read_node(node->children[idx]);
    
    if (child->num_keys < MIN_KEYS) {
        btree_fill_child(node_offset, idx);
        free(node);
        node = btree_read_node(node_offset);
        idx = btree_find_key_index(node, &key);
        if (last_child && idx > node->num_keys) idx = node->num_keys;
    }
    
    long next_child = (idx > node->num_keys) ? 
                     node->children[node->num_keys] : node->children[idx];
    
    int result = btree_delete_recursive(next_child, key);
    free(node);
    free(child);
    return result;
}

/**
 * Encontra índice da chave no nó
 */
static int btree_find_key_index(BTreeNode* node, const BTreeKey* key) {
    int idx = 0;
    while (idx < node->num_keys && btree_compare_keys(&node->keys[idx], key) < 0) {
        idx++;
    }
    return idx;
}

/**
 * Remove chave de nó folha
 */
static void btree_remove_from_leaf(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    
    for (int i = idx + 1; i < node->num_keys; i++) {
        node->keys[i - 1] = node->keys[i];
    }
    
    node->num_keys--;
    btree_write_node(node_offset, node);
    free(node);
}

/**
 * Remove chave de nó interno
 */
static void btree_remove_from_non_leaf(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    BTreeKey key = node->keys[idx];
    
    BTreeNode* left_child = btree_read_node(node->children[idx]);
    
    if (left_child->num_keys >= MIN_KEYS) {
        BTreeKey pred = btree_get_predecessor(node_offset, idx);
        node->keys[idx] = pred;
        btree_write_node(node_offset, node);
        btree_delete_recursive(node->children[idx], pred);
    } else {
        BTreeNode* right_child = btree_read_node(node->children[idx + 1]);
        if (right_child->num_keys >= MIN_KEYS) {
            BTreeKey succ = btree_get_successor(node_offset, idx);
            node->keys[idx] = succ;
            btree_write_node(node_offset, node);
            btree_delete_recursive(node->children[idx + 1], succ);
        } else {
            btree_merge(node_offset, idx);
            btree_delete_recursive(node->children[idx], key);
        }
        free(right_child);
    }
    
    free(node);
    free(left_child);
}

/**
 * Obtém predecessor da chave
 */
static BTreeKey btree_get_predecessor(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    long current = node->children[idx];
    free(node);
    
    while (1) {
        BTreeNode* curr_node = btree_read_node(current);
        if (curr_node->is_leaf) {
            BTreeKey pred = curr_node->keys[curr_node->num_keys - 1];
            free(curr_node);
            return pred;
        }
        long next = curr_node->children[curr_node->num_keys];
        free(curr_node);
        current = next;
    }
}

/**
 * Obtém successor da chave
 */
static BTreeKey btree_get_successor(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    long current = node->children[idx + 1];
    free(node);
    
    while (1) {
        BTreeNode* curr_node = btree_read_node(current);
        if (curr_node->is_leaf) {
            BTreeKey succ = curr_node->keys[0];
            free(curr_node);
            return succ;
        }
        long next = curr_node->children[0];
        free(curr_node);
        current = next;
    }
}

/**
 * Preenche filho com poucas chaves
 */
static void btree_fill_child(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    
    if (idx != 0) {
        BTreeNode* left_sibling = btree_read_node(node->children[idx - 1]);
        if (left_sibling->num_keys >= MIN_KEYS) {
            btree_borrow_from_prev(node_offset, idx);
            free(left_sibling);
            free(node);
            return;
        }
        free(left_sibling);
    }
    
    if (idx != node->num_keys) {
        BTreeNode* right_sibling = btree_read_node(node->children[idx + 1]);
        if (right_sibling->num_keys >= MIN_KEYS) {
            btree_borrow_from_next(node_offset, idx);
            free(right_sibling);
            free(node);
            return;
        }
        free(right_sibling);
    }
    
    if (idx != node->num_keys) {
        btree_merge(node_offset, idx);
    } else {
        btree_merge(node_offset, idx - 1);
    }
    
    free(node);
}

/**
 * Empréstimo do irmão anterior
 */
static void btree_borrow_from_prev(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    BTreeNode* child = btree_read_node(node->children[idx]);
    BTreeNode* left_sibling = btree_read_node(node->children[idx - 1]);
    
    for (int i = child->num_keys - 1; i >= 0; i--) {
        child->keys[i + 1] = child->keys[i];
    }
    
    if (!child->is_leaf) {
        for (int i = child->num_keys; i >= 0; i--) {
            child->children[i + 1] = child->children[i];
        }
    }
    
    child->keys[0] = node->keys[idx - 1];
    
    if (!child->is_leaf) {
        child->children[0] = left_sibling->children[left_sibling->num_keys];
    }
    
    child->num_keys++;
    node->keys[idx - 1] = left_sibling->keys[left_sibling->num_keys - 1];
    left_sibling->num_keys--;
    
    btree_write_node(node_offset, node);
    btree_write_node(child->self_offset, child);
    btree_write_node(left_sibling->self_offset, left_sibling);
    
    free(node);
    free(child);
    free(left_sibling);
}

/**
 * Empréstimo do irmão seguinte
 */
static void btree_borrow_from_next(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    BTreeNode* child = btree_read_node(node->children[idx]);
    BTreeNode* right_sibling = btree_read_node(node->children[idx + 1]);
    
    child->keys[child->num_keys] = node->keys[idx];
    child->num_keys++;
    
    if (!child->is_leaf) {
        child->children[child->num_keys] = right_sibling->children[0];
    }
    
    node->keys[idx] = right_sibling->keys[0];
    
    for (int i = 1; i < right_sibling->num_keys; i++) {
        right_sibling->keys[i - 1] = right_sibling->keys[i];
    }
    
    if (!right_sibling->is_leaf) {
        for (int i = 1; i <= right_sibling->num_keys; i++) {
            right_sibling->children[i - 1] = right_sibling->children[i];
        }
    }
    
    right_sibling->num_keys--;
    
    btree_write_node(node_offset, node);
    btree_write_node(child->self_offset, child);
    btree_write_node(right_sibling->self_offset, right_sibling);
    
    free(node);
    free(child);
    free(right_sibling);
}

/**
 * Funde dois filhos
 */
static void btree_merge(long node_offset, int idx) {
    BTreeNode* node = btree_read_node(node_offset);
    BTreeNode* child = btree_read_node(node->children[idx]);
    BTreeNode* right_sibling = btree_read_node(node->children[idx + 1]);
    
    child->keys[MIN_KEYS - 1] = node->keys[idx];
    
    for (int i = 0; i < right_sibling->num_keys; i++) {
        child->keys[i + MIN_KEYS] = right_sibling->keys[i];
    }
    
    if (!child->is_leaf) {
        for (int i = 0; i <= right_sibling->num_keys; i++) {
            child->children[i + MIN_KEYS] = right_sibling->children[i];
        }
    }
    
    for (int i = idx + 1; i < node->num_keys; i++) {
        node->keys[i - 1] = node->keys[i];
    }
    
    for (int i = idx + 2; i <= node->num_keys; i++) {
        node->children[i - 1] = node->children[i];
    }
    
    child->num_keys += right_sibling->num_keys + 1;
    node->num_keys--;
    
    btree_write_node(node_offset, node);
    btree_write_node(child->self_offset, child);
    
    free(node);
    free(child);
    free(right_sibling);
}

/**
 * Busca chave na Árvore-B
 */
int btree_search(const char* name, int threshold, BTreeKey* result) {
    BTreeNode* current = btree_root;
    BTreeKey key;
    strncpy(key.name, name, MAX_NAME_LEN - 1);
    key.name[MAX_NAME_LEN - 1] = '\0';
    key.threshold = threshold;
    
    while (current) {
        int i = 0;
        while (i < current->num_keys && btree_compare_keys(&key, &current->keys[i]) > 0) {
            i++;
        }
        
        if (i < current->num_keys && btree_compare_keys(&key, &current->keys[i]) == 0) {
            *result = current->keys[i];
            if (current != btree_root) free(current);
            return 1;
        }
        
        if (current->is_leaf) break;
        
        long next = current->children[i];
        if (current != btree_root) free(current);
        current = btree_read_node(next);
    }
    
    if (current && current != btree_root) free(current);
    return 0;
}

/**
 * Percurso in-order para impressão ordenada
 */
void btree_print_inorder() {
    printf("\n=== CHAVES EM ORDEM CRESCENTE ===\n");
    btree_print_inorder_recursive(btree_header.root_offset);
    printf("==================================\n");
}

static void btree_print_inorder_recursive(long node_offset) {
    BTreeNode* node = btree_read_node(node_offset);
    
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) {
            btree_print_inorder_recursive(node->children[i]);
        }
        printf("Nome: %-20s | Limiar: %3d | Dimensões: %4dx%4d\n",
               node->keys[i].name, node->keys[i].threshold,
               node->keys[i].width, node->keys[i].height);
    }
    
    if (!node->is_leaf) {
        btree_print_inorder_recursive(node->children[node->num_keys]);
    }
    
    free(node);
}

/**
 * Imprime conteúdo de todas as páginas
 */
void btree_print_pages() {
    printf("\n=== CONTEÚDO DAS PÁGINAS DA ÁRVORE-B ===\n");
    printf("Ordem: %d | Total de páginas: %d | Offset da raiz: %ld\n\n", 
           ORDER, btree_header.node_count, btree_header.root_offset);
    
    long offset = sizeof(BTreeHeader);
    for (int i = 1; i <= btree_header.node_count; i++) {
        BTreeNode* node = btree_read_node(offset);
        
        printf("Página %d (Offset: %ld): ", i, offset);
        printf("[%s] ", node->is_leaf ? "FOLHA" : "INTERNO");
        printf("Chaves: %d | ", node->num_keys);
        
        if (node->num_keys > 0) {
            printf("Conteúdo: ");
            for (int j = 0; j < node->num_keys; j++) {
                printf("\"%s\",limiar=%d", node->keys[j].name, node->keys[j].threshold);
                if (j < node->num_keys - 1) printf(" | ");
            }
        } else {
            printf("VAZIA");
        }
        
        if (!node->is_leaf && node->num_keys > 0) {
            printf(" | Filhos: ");
            for (int j = 0; j <= node->num_keys; j++) {
                if (node->children[j] != -1) {
                    printf("%ld", node->children[j]);
                    if (j < node->num_keys) printf(", ");
                }
            }
        }
        printf("\n");
        
        free(node);
        offset += sizeof(BTreeNode);
    }
    printf("==========================================\n");
}

/**
 * Retorna offset da raiz (para uso em image.c)
 */
long btree_get_root_offset() {
    return btree_header.root_offset;
}