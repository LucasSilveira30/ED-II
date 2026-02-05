#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image_manager.h"

/**
 * Inicializa os arquivos do banco de dados
 * Cria os arquivos se não existirem
 */
void initializeDatabase() {
    FILE* index_file = fopen("image_index.dat", "ab");
    FILE* data_file = fopen("image_data.dat", "ab");
    if (index_file) fclose(index_file);
    if (data_file) fclose(data_file);
}

/**
 * Adiciona uma imagem ao banco de dados
 * Processo: Ler PGM → Binarizar → Comprimir → Salvar dados → Atualizar índice
 */
int addImageToDatabase(const char* filename, int threshold) {
    // Ler e processar imagem
    PGMImage* img = readPGM(filename);
    if (!img) return 0;
    
    binarizeImage(img, threshold);
    
    // Comprimir imagem
    int compressed_size;
    unsigned char* compressed_data = compressRLE(img->pixels, img->width, img->height, &compressed_size);
    if (!compressed_data) {
        freePGM(img);
        return 0;
    }
    
    // Salvar dados comprimidos
    FILE* data_file = fopen("image_data.dat", "ab");
    if (!data_file) {
        free(compressed_data);
        freePGM(img);
        return 0;
    }
    
    long offset = ftell(data_file);
    fwrite(compressed_data, 1, compressed_size, data_file);
    fclose(data_file);
    
    // Atualizar arquivo de índices
    FILE* index_file = fopen("image_index.dat", "ab");
    if (!index_file) {
        free(compressed_data);
        freePGM(img);
        return 0;
    }
    
    ImageIndex entry;
    strncpy(entry.name, filename, MAX_NAME_LEN - 1);
    entry.name[MAX_NAME_LEN - 1] = '\0';
    entry.threshold = threshold;
    entry.offset = offset;
    entry.compressed_size = compressed_size;
    entry.width = img->width;
    entry.height = img->height;
    entry.max_gray = img->max_gray;
    entry.removed = 0;
    
    fwrite(&entry, sizeof(ImageIndex), 1, index_file);
    fclose(index_file);
    
    free(compressed_data);
    freePGM(img);
    return 1;
}

/**
 * Lista todas as imagens não removidas do banco de dados
 */
int listImagesInDatabase() {
    FILE* index_file = fopen("image_index.dat", "rb");
    if (!index_file) return 0;
    
    ImageIndex entry;
    int count = 0;
    
    printf("\n=== IMAGENS NO BANCO DE DADOS ===\n");
    while (fread(&entry, sizeof(ImageIndex), 1, index_file)) {
        if (!entry.removed) {
            printf("%d. Nome: %s | Limiar: %d | Dimensões: %dx%d | Tamanho: %d bytes\n",
                   ++count, entry.name, entry.threshold, entry.width, entry.height, entry.compressed_size);
        }
    }
    
    fclose(index_file);
    return count;
}

/**
 * Remove uma imagem logicamente (marca como removida no índice)
 */
int removeImageFromDatabase(const char* name, int threshold) {
    FILE* index_file = fopen("image_index.dat", "r+b");
    if (!index_file) return 0;
    
    ImageIndex entry;
    long position = 0;
    int found = 0;
    
    while (fread(&entry, sizeof(ImageIndex), 1, index_file)) {
        if (strcmp(entry.name, name) == 0 && entry.threshold == threshold && !entry.removed) {
            // Marcar como removida
            entry.removed = 1;
            fseek(index_file, position, SEEK_SET);
            fwrite(&entry, sizeof(ImageIndex), 1, index_file);
            found = 1;
            break;
        }
        position = ftell(index_file);
    }
    
    fclose(index_file);
    return found;
}

/**
 * Compacta o banco de dados removendo entradas marcadas como excluídas
 * Complexidade: O(n) tempo, O(1) espaço (além dos buffers)
 */
int compactDatabase() {
    FILE* old_index = fopen("image_index.dat", "rb");
    FILE* old_data = fopen("image_data.dat", "rb");
    FILE* new_index = fopen("index_temp.dat", "wb");
    FILE* new_data = fopen("data_temp.dat", "wb");
    
    if (!old_index || !old_data || !new_index || !new_data) {
        if (old_index) fclose(old_index);
        if (old_data) fclose(old_data);
        if (new_index) fclose(new_index);
        if (new_data) fclose(new_data);
        return 0;
    }
    
    ImageIndex entry;
    long new_offset = 0;
    int kept_count = 0;
    
    while (fread(&entry, sizeof(ImageIndex), 1, old_index)) {
        if (!entry.removed) {
            // Copiar dados comprimidos
            unsigned char* data = (unsigned char*)malloc(entry.compressed_size);
            if (!data) continue;
            
            fseek(old_data, entry.offset, SEEK_SET);
            fread(data, 1, entry.compressed_size, old_data);
            
            // Atualizar offset e salvar
            entry.offset = new_offset;
            fwrite(data, 1, entry.compressed_size, new_data);
            fwrite(&entry, sizeof(ImageIndex), 1, new_index);
            
            new_offset += entry.compressed_size;
            kept_count++;
            free(data);
        }
    }
    
    fclose(old_index);
    fclose(old_data);
    fclose(new_index);
    fclose(new_data);
    
    // Substituir arquivos antigos
    remove("image_index.dat");
    remove("image_data.dat");
    rename("index_temp.dat", "image_index.dat");
    rename("data_temp.dat", "image_data.dat");
    
    return kept_count;
}

/**
 * Recupera uma imagem do banco de dados e salva em formato PGM
 */
int retrieveImageFromDatabase(const char* name, int threshold, const char* output_filename) {
    FILE* index_file = fopen("image_index.dat", "rb");
    if (!index_file) return 0;
    
    ImageIndex entry;
    int found = 0;
    
    // Buscar entrada no índice
    while (fread(&entry, sizeof(ImageIndex), 1, index_file) && !found) {
        if (strcmp(entry.name, name) == 0 && entry.threshold == threshold && !entry.removed) {
            found = 1;
        }
    }
    fclose(index_file);
    
    if (!found) return 0;
    
    // Ler dados comprimidos
    FILE* data_file = fopen("image_data.dat", "rb");
    if (!data_file) return 0;
    
    unsigned char* compressed_data = (unsigned char*)malloc(entry.compressed_size);
    if (!compressed_data) {
        fclose(data_file);
        return 0;
    }
    
    fseek(data_file, entry.offset, SEEK_SET);
    fread(compressed_data, 1, entry.compressed_size, data_file);
    fclose(data_file);
    
    // Descomprimir e reconstruir imagem
    int** pixels = decompressRLE(compressed_data, entry.compressed_size, entry.width, entry.height);
    free(compressed_data);
    
    if (!pixels) return 0;
    
    PGMImage* img = (PGMImage*)malloc(sizeof(PGMImage));
    if (!img) {
        for (int i = 0; i < entry.height; i++) free(pixels[i]);
        free(pixels);
        return 0;
    }
    
    img->width = entry.width;
    img->height = entry.height;
    img->max_gray = entry.max_gray;
    img->pixels = pixels;
    
    int success = writePGM(output_filename, img);
    freePGM(img);
    
    return success;
}