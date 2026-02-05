#ifndef IMAGE_H
#define IMAGE_H

#include "btree.h"

typedef struct {
    int width;
    int height;
    int max_gray;
    int** pixels;
} PGMImage;

// Interface pública do módulo de imagem
PGMImage* image_read_pgm(const char* filename);
int image_write_pgm(const char* filename, PGMImage* img);
void image_binarize(PGMImage* img, int threshold);
void image_free(PGMImage* img);

// Interface pública do banco de dados
void database_add_image(const char* filename, int threshold);
void database_add_multiple_thresholds(const char* filename, int thresholds[], int count);
void database_retrieve_image(const char* name, int threshold, const char* output);
void database_list_images();
void database_compact();

#endif