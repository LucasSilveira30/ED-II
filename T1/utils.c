#include <stdio.h>
#include <stdlib.h>
#include "image_manager.h"

/**
 * Obtém o tamanho de um arquivo
 */
long getFileSize(FILE* file) {
    if (!file) return -1;
    
    long current = ftell(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, current, SEEK_SET);
    
    return size;
}

/**
 * Exibe informações sobre uma imagem
 */
void printImageInfo(PGMImage* img) {
    if (!img) {
        printf("Imagem: NULL\n");
        return;
    }
    
    printf("Dimensões: %dx%d | Max Gray: %d\n", 
           img->width, img->height, img->max_gray);
}

/**
 * Verifica se uma entrada do índice está marcada como removida
 */
int isRemoved(ImageIndex* entry) {
    return (entry == NULL) ? 1 : entry->removed;
}