#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "image_manager.h"

/**
 * Lê um arquivo PGM (formato P2 - ASCII)
 * Retorna uma estrutura PGMImage ou NULL em caso de erro
 */
PGMImage* readPGM(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível abrir o arquivo %s\n", filename);
        return NULL;
    }
    
    char magic[3];
    if (fscanf(file, "%2s", magic) != 1 || strcmp(magic, "P2") != 0) {
        fprintf(stderr, "Erro: Formato PGM inválido. Esperado P2.\n");
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
    
    PGMImage* img = (PGMImage*)malloc(sizeof(PGMImage));
    if (!img) {
        fprintf(stderr, "Erro: Falha na alocação de memória.\n");
        fclose(file);
        return NULL;
    }
    
    if (fscanf(file, "%d %d %d", &img->width, &img->height, &img->max_gray) != 3) {
        fprintf(stderr, "Erro: Cabeçalho PGM inválido.\n");
        free(img);
        fclose(file);
        return NULL;
    }
    
    // Alocar memória para os pixels
    img->pixels = (int**)malloc(img->height * sizeof(int*));
    if (!img->pixels) {
        fprintf(stderr, "Erro: Falha na alocação de memória para pixels.\n");
        free(img);
        fclose(file);
        return NULL;
    }
    
    for (int i = 0; i < img->height; i++) {
        img->pixels[i] = (int*)malloc(img->width * sizeof(int));
        if (!img->pixels[i]) {
            fprintf(stderr, "Erro: Falha na alocação de memória para linha %d.\n", i);
            for (int j = 0; j < i; j++) free(img->pixels[j]);
            free(img->pixels);
            free(img);
            fclose(file);
            return NULL;
        }
        
        for (int j = 0; j < img->width; j++) {
            if (fscanf(file, "%d", &img->pixels[i][j]) != 1) {
                fprintf(stderr, "Erro: Leitura de pixel falhou na posição (%d, %d).\n", i, j);
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
 * Escreve uma imagem em formato PGM ASCII (P2)
 */
int writePGM(const char* filename, PGMImage* img) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível criar o arquivo %s\n", filename);
        return 0;
    }
    
    fprintf(file, "P2\n%d %d\n%d\n", img->width, img->height, img->max_gray);
    
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            fprintf(file, "%d", img->pixels[i][j]);
            if (j < img->width - 1) fprintf(file, " ");
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return 1;
}

/**
 * Aplica limiarização para binarizar a imagem
 * Pixels > threshold viram 1, outros viram 0
 */
void binarizeImage(PGMImage* img, int threshold) {
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->pixels[i][j] = (img->pixels[i][j] > threshold) ? 1 : 0;
        }
    }
    img->max_gray = 1;
}

/**
 * Aplica negativação na imagem
 */
void negativeImage(PGMImage* img) {
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->pixels[i][j] = img->max_gray - img->pixels[i][j];
        }
    }
}

/**
 * Libera a memória alocada para uma imagem PGM
 */
void freePGM(PGMImage* img) {
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
 * Comprime uma imagem binária usando Run-Length Encoding (RLE)
 * Formato: [primeiro_pixel, count1, count2, ...]
 * Retorna array comprimido e atualiza compressed_size
 */
unsigned char* compressRLE(int** pixels, int width, int height, int* compressed_size) {
    int total_pixels = width * height;
    unsigned char* compressed = (unsigned char*)malloc(total_pixels * 2); // Pior caso
    
    if (!compressed) return NULL;
    
    int current_val = pixels[0][0];
    int count = 1;
    int comp_index = 0;
    
    compressed[comp_index++] = current_val; // Primeiro pixel
    
    for (int i = 0; i < height; i++) {
        for (int j = (i == 0 ? 1 : 0); j < width; j++) {
            if (pixels[i][j] == current_val && count < 255) {
                count++;
            } else {
                compressed[comp_index++] = count;
                current_val = pixels[i][j];
                count = 1;
            }
        }
    }
    compressed[comp_index++] = count; // Última sequência
    
    *compressed_size = comp_index;
    return (unsigned char*)realloc(compressed, comp_index);
}

/**
 * Descomprime dados RLE para reconstruir matriz de pixels
 */
int** decompressRLE(unsigned char* compressed_data, int compressed_size, int width, int height) {
    int** pixels = (int**)malloc(height * sizeof(int*));
    if (!pixels) return NULL;
    
    for (int i = 0; i < height; i++) {
        pixels[i] = (int*)malloc(width * sizeof(int));
        if (!pixels[i]) {
            for (int j = 0; j < i; j++) free(pixels[j]);
            free(pixels);
            return NULL;
        }
    }
    
    int current_val = compressed_data[0];
    int data_index = 1;
    int pixel_count = 0;
    int total_processed = 0;
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (pixel_count == 0) {
                if (data_index >= compressed_size) break;
                pixel_count = compressed_data[data_index++];
            }
            pixels[i][j] = current_val;
            pixel_count--;
            total_processed++;
            
            if (pixel_count == 0) {
                current_val = !current_val; // Alterna entre 0 e 1
            }
        }
    }
    
    return pixels;
}