#ifndef IMAGE_MANAGER_H
#define IMAGE_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 50
#define MAX_THRESHOLDS 10

// Estrutura para entrada no arquivo de índices
typedef struct {
    char name[MAX_NAME_LEN];
    int threshold;
    long offset;
    int compressed_size;
    int width;
    int height;
    int max_gray;
    int removed;
} ImageIndex;

// Estrutura para imagem PGM
typedef struct {
    int width;
    int height;
    int max_gray;
    int **pixels;
} PGMImage;

// Estrutura para múltiplas versões de uma imagem (reconstrução)
typedef struct {
    int threshold;
    PGMImage *image;
} ImageVersion;

// Processamento de imagens
PGMImage* readPGM(const char* filename);
int writePGM(const char* filename, PGMImage* img);
void binarizeImage(PGMImage* img, int threshold);
void negativeImage(PGMImage* img);
void freePGM(PGMImage* img);

// Compressão e descompressão
unsigned char* compressRLE(int** pixels, int width, int height, int* compressed_size);
int** decompressRLE(unsigned char* compressed_data, int compressed_size, int width, int height);

// Gerenciamento do banco de dados
void initializeDatabase();
int addImageToDatabase(const char* filename, int threshold);
int listImagesInDatabase();
int removeImageFromDatabase(const char* name, int threshold);
int compactDatabase();
int retrieveImageFromDatabase(const char* name, int threshold, const char* output_filename);

// Reconstrução (Bônus)
int reconstructOriginalImage(const char* name, const char* output_filename);

// Utilitários
long getFileSize(FILE* file);
void printImageInfo(PGMImage* img);
int isRemoved(ImageIndex* entry);

#endif