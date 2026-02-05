#include "image.h"
#include <ctype.h>

// Funções privadas
static unsigned char* image_compress_rle(int** pixels, int width, int height, int* size);
static int** image_decompress_rle(unsigned char* data, int size, int width, int height);
static void database_compact_recursive(long node_offset, FILE* old_data, FILE* new_data);

/**
 * Lê arquivo PGM (formato P2 ASCII)
 */
PGMImage* image_read_pgm(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível abrir %s\n", filename);
        return NULL;
    }
    
    char magic[3];
    if (fscanf(file, "%2s", magic) != 1 || strcmp(magic, "P2") != 0) {
        fprintf(stderr, "Erro: Formato não é PGM P2\n");
        fclose(file);
        return NULL;
    }
    
    // Pular comentários
    int c;
    while ((c = fgetc(file)) == ' ' || c == '\t' || c == '\n');
    if (c == '#') {
        while ((c = fgetc(file)) != '\n' && c != EOF);
    } else {
        ungetc(c, file);
    }
    
    PGMImage* img = malloc(sizeof(PGMImage));
    if (!img) {
        fclose(file);
        return NULL;
    }
    
    if (fscanf(file, "%d %d %d", &img->width, &img->height, &img->max_gray) != 3) {
        fprintf(stderr, "Erro: Cabeçalho PGM inválido\n");
        free(img);
        fclose(file);
        return NULL;
    }
    
    img->pixels = malloc(img->height * sizeof(int*));
    if (!img->pixels) {
        fprintf(stderr, "Erro: Falha na alocação de memória\n");
        free(img);
        fclose(file);
        return NULL;
    }
    
    for (int i = 0; i < img->height; i++) {
        img->pixels[i] = malloc(img->width * sizeof(int));
        if (!img->pixels[i]) {
            fprintf(stderr, "Erro: Falha na alocação de linha %d\n", i);
            for (int j = 0; j < i; j++) free(img->pixels[j]);
            free(img->pixels);
            free(img);
            fclose(file);
            return NULL;
        }
        
        for (int j = 0; j < img->width; j++) {
            if (fscanf(file, "%d", &img->pixels[i][j]) != 1) {
                fprintf(stderr, "Erro: Leitura de pixels falhou em (%d,%d)\n", i, j);
                for (int k = 0; k <= i; k++) free(img->pixels[k]);
                free(img->pixels);
                free(img);
                fclose(file);
                return NULL;
            }
        }
    }
    
    fclose(file);
    return img;
}

/**
 * Escreve arquivo PGM
 */
int image_write_pgm(const char* filename, PGMImage* img) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível criar %s\n", filename);
        return 0;
    }
    
    fprintf(file, "P2\n%d %d\n%d\n", img->width, img->height, img->max_gray);
    
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            fprintf(file, "%d", img->pixels[i][j]);
            if (j < img->width - 1) fputc(' ', file);
        }
        fputc('\n', file);
    }
    
    fclose(file);
    return 1;
}

/**
 * Aplica limiarização para binarizar imagem
 */
void image_binarize(PGMImage* img, int threshold) {
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->pixels[i][j] = (img->pixels[i][j] > threshold) ? 1 : 0;
        }
    }
    img->max_gray = 1;
}

/**
 * Libera memória da imagem
 */
void image_free(PGMImage* img) {
    if (img) {
        if (img->pixels) {
            for (int i = 0; i < img->height; i++) {
                free(img->pixels[i]);
            }
            free(img->pixels);
        }
        free(img);
    }
}

/**
 * Comprime imagem usando RLE
 */
static unsigned char* image_compress_rle(int** pixels, int width, int height, int* size) {
    int total = width * height;
    unsigned char* compressed = malloc(total * 2);
    if (!compressed) return NULL;
    
    int current_val = pixels[0][0];
    int count = 1;
    int index = 0;
    
    compressed[index++] = current_val;
    
    for (int i = 0; i < height; i++) {
        for (int j = (i == 0 ? 1 : 0); j < width; j++) {
            if (pixels[i][j] == current_val && count < 255) {
                count++;
            } else {
                compressed[index++] = count;
                current_val = pixels[i][j];
                count = 1;
            }
        }
    }
    compressed[index++] = count;
    
    *size = index;
    return realloc(compressed, index);
}

/**
 * Descomprime dados RLE
 */
static int** image_decompress_rle(unsigned char* data, int size, int width, int height) {
    int** pixels = malloc(height * sizeof(int*));
    if (!pixels) return NULL;
    
    for (int i = 0; i < height; i++) {
        pixels[i] = malloc(width * sizeof(int));
        if (!pixels[i]) {
            for (int j = 0; j < i; j++) free(pixels[j]);
            free(pixels);
            return NULL;
        }
    }
    
    int current_val = data[0];
    int data_index = 1;
    int count = 0;
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (count == 0) {
                if (data_index >= size) break;
                count = data[data_index++];
            }
            pixels[i][j] = current_val;
            count--;
            if (count == 0) {
                current_val = !current_val;
            }
        }
    }
    
    return pixels;
}

/**
 * Adiciona imagem com único limiar
 */
void database_add_image(const char* filename, int threshold) {
    printf("Processando imagem: %s (limiar=%d)\n", filename, threshold);
    
    PGMImage* img = image_read_pgm(filename);
    if (!img) {
        printf("Erro: Falha ao ler imagem %s\n", filename);
        return;
    }
    
    image_binarize(img, threshold);
    
    int compressed_size;
    unsigned char* compressed = image_compress_rle(img->pixels, img->width, img->height, &compressed_size);
    if (!compressed) {
        printf("Erro: Falha na compressão da imagem\n");
        image_free(img);
        return;
    }
    
    FILE* data_file = fopen("image_data.dat", "ab");
    if (!data_file) {
        printf("Erro: Não foi possível abrir arquivo de dados\n");
        free(compressed);
        image_free(img);
        return;
    }
    
    long offset = ftell(data_file);
    fwrite(compressed, 1, compressed_size, data_file);
    fclose(data_file);
    
    BTreeKey key;
    strncpy(key.name, filename, MAX_NAME_LEN - 1);
    key.name[MAX_NAME_LEN - 1] = '\0';
    key.threshold = threshold;
    key.data_offset = offset;
    key.data_size = compressed_size;
    key.width = img->width;
    key.height = img->height;
    
    btree_insert(key);
    
    free(compressed);
    image_free(img);
    
    printf("Imagem adicionada com sucesso\n");
}

/**
 * Adiciona imagem com múltiplos limiares
 */
void database_add_multiple_thresholds(const char* filename, int thresholds[], int count) {
    printf("\n=== PROCESSANDO %d VERSÕES DE %s ===\n", count, filename);
    
    PGMImage* original = image_read_pgm(filename);
    if (!original) {
        printf("Erro: Falha ao ler imagem original\n");
        return;
    }
    
    for (int i = 0; i < count; i++) {
        printf("  Versão %d/%d: limiar=%d... ", i + 1, count, thresholds[i]);
        
        PGMImage* copy = malloc(sizeof(PGMImage));
        if (!copy) {
            printf("Erro de alocação\n");
            continue;
        }
        
        copy->width = original->width;
        copy->height = original->height;
        copy->max_gray = original->max_gray;
        
        copy->pixels = malloc(copy->height * sizeof(int*));
        if (!copy->pixels) {
            printf("Erro de alocação\n");
            free(copy);
            continue;
        }
        
        for (int row = 0; row < copy->height; row++) {
            copy->pixels[row] = malloc(copy->width * sizeof(int));
            if (!copy->pixels[row]) {
                printf("Erro de alocação\n");
                for (int k = 0; k < row; k++) free(copy->pixels[k]);
                free(copy->pixels);
                free(copy);
                continue;
            }
            memcpy(copy->pixels[row], original->pixels[row], copy->width * sizeof(int));
        }
        
        image_binarize(copy, thresholds[i]);
        
        int compressed_size;
        unsigned char* compressed = image_compress_rle(copy->pixels, copy->width, copy->height, &compressed_size);
        if (!compressed) {
            printf("Erro na compressão\n");
            image_free(copy);
            continue;
        }
        
        FILE* data_file = fopen("image_data.dat", "ab");
        if (!data_file) {
            printf("Erro ao abrir arquivo\n");
            free(compressed);
            image_free(copy);
            continue;
        }
        
        long offset = ftell(data_file);
        fwrite(compressed, 1, compressed_size, data_file);
        fclose(data_file);
        
        BTreeKey key;
        strncpy(key.name, filename, MAX_NAME_LEN - 1);
        key.name[MAX_NAME_LEN - 1] = '\0';
        key.threshold = thresholds[i];
        key.data_offset = offset;
        key.data_size = compressed_size;
        key.width = copy->width;
        key.height = copy->height;
        
        btree_insert(key);
        
        free(compressed);
        image_free(copy);
        
        printf("✅ (offset: %ld, tamanho: %d bytes)\n", offset, compressed_size);
    }
    
    image_free(original);
    printf("=== CONCLUÍDO: %d VERSÕES ADICIONADAS ===\n\n", count);
}

/**
 * Recupera imagem do banco de dados
 */
void database_retrieve_image(const char* name, int threshold, const char* output) {
    BTreeKey key;
    if (!btree_search(name, threshold, &key)) {
        printf("Imagem não encontrada: %s (limiar=%d)\n", name, threshold);
        return;
    }
    
    FILE* data_file = fopen("image_data.dat", "rb");
    if (!data_file) {
        printf("Erro ao abrir arquivo de dados\n");
        return;
    }
    
    unsigned char* compressed = malloc(key.data_size);
    if (!compressed) {
        printf("Erro de alocação de memória\n");
        fclose(data_file);
        return;
    }
    
    fseek(data_file, key.data_offset, SEEK_SET);
    fread(compressed, 1, key.data_size, data_file);
    fclose(data_file);
    
    int** pixels = image_decompress_rle(compressed, key.data_size, key.width, key.height);
    if (!pixels) {
        printf("Erro na descompressão\n");
        free(compressed);
        return;
    }
    
    PGMImage* img = malloc(sizeof(PGMImage));
    if (!img) {
        printf("Erro de alocação\n");
        free(compressed);
        for (int i = 0; i < key.height; i++) free(pixels[i]);
        free(pixels);
        return;
    }
    
    img->width = key.width;
    img->height = key.height;
    img->max_gray = 1;
    img->pixels = pixels;
    
    if (image_write_pgm(output, img)) {
        printf("✅ Imagem recuperada: %s\n", output);
    } else {
        printf("❌ Erro ao salvar: %s\n", output);
    }
    
    image_free(img);
    free(compressed);
}

/**
 * Lista todas as imagens
 */
void database_list_images() {
    btree_print_inorder();
}

/**
 * Função auxiliar recursiva para compactação
 */
static void database_compact_recursive(long node_offset, FILE* old_data, FILE* new_data) {
    BTreeNode* node = btree_read_node(node_offset);
    if (!node) return;
    
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) {
            database_compact_recursive(node->children[i], old_data, new_data);
        }
        
        unsigned char* data = malloc(node->keys[i].data_size);
        if (!data) continue;
        
        fseek(old_data, node->keys[i].data_offset, SEEK_SET);
        fread(data, 1, node->keys[i].data_size, old_data);
        
        long new_offset = ftell(new_data);
        fwrite(data, 1, node->keys[i].data_size, new_data);
        
        node->keys[i].data_offset = new_offset;
        free(data);
    }
    
    if (!node->is_leaf) {
        database_compact_recursive(node->children[node->num_keys], old_data, new_data);
    }
    
    btree_write_node(node_offset, node);
    free(node);
}

/**
 * Compacta arquivo de dados (apenas dados, não índices)
 */
void database_compact() {
    printf("\n=== INICIANDO COMPACTAÇÃO DO ARQUIVO DE DADOS ===\n");
    
    FILE* old_data = fopen("image_data.dat", "rb");
    FILE* new_data = fopen("data_temp.dat", "wb");
    
    if (!old_data || !new_data) {
        printf("Erro ao abrir arquivos para compactação\n");
        if (old_data) fclose(old_data);
        if (new_data) fclose(new_data);
        return;
    }
    
    database_compact_recursive(btree_get_root_offset(), old_data, new_data);
    
    fclose(old_data);
    fclose(new_data);
    
    remove("image_data.dat");
    rename("data_temp.dat", "image_data.dat");
    
    printf("Compactação concluída com sucesso\n");
}