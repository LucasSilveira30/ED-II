#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image_manager.h"

/**
 * Reconstrução da imagem original (BÔNUS)
 * Encontra todas as versões da mesma imagem com diferentes limiares
 * Calcula a imagem média para tentar reconstruir a original
 */
int reconstructOriginalImage(const char* name, const char* output_filename) {
    FILE* index_file = fopen("image_index.dat", "rb");
    if (!index_file) return 0;
    
    // Coletar todas as versões da imagem
    ImageIndex entry;
    ImageVersion* versions = NULL;
    int version_count = 0;
    int max_versions = 10;
    
    versions = (ImageVersion*)malloc(max_versions * sizeof(ImageVersion));
    if (!versions) {
        fclose(index_file);
        return 0;
    }
    
    // Buscar todas as versões não removidas
    while (fread(&entry, sizeof(ImageIndex), 1, index_file)) {
        if (strcmp(entry.name, name) == 0 && !entry.removed) {
            if (version_count >= max_versions) {
                max_versions *= 2;
                ImageVersion* temp = (ImageVersion*)realloc(versions, max_versions * sizeof(ImageVersion));
                if (!temp) {
                    fclose(index_file);
                    for (int i = 0; i < version_count; i++) freePGM(versions[i].image);
                    free(versions);
                    return 0;
                }
                versions = temp;
            }
            
            // Recuperar imagem
            FILE* data_file = fopen("image_data.dat", "rb");
            if (!data_file) continue;
            
            unsigned char* compressed_data = (unsigned char*)malloc(entry.compressed_size);
            if (!compressed_data) {
                fclose(data_file);
                continue;
            }
            
            fseek(data_file, entry.offset, SEEK_SET);
            fread(compressed_data, 1, entry.compressed_size, data_file);
            fclose(data_file);
            
            int** pixels = decompressRLE(compressed_data, entry.compressed_size, entry.width, entry.height);
            free(compressed_data);
            
            if (pixels) {
                versions[version_count].threshold = entry.threshold;
                versions[version_count].image = (PGMImage*)malloc(sizeof(PGMImage));
                if (versions[version_count].image) {
                    versions[version_count].image->width = entry.width;
                    versions[version_count].image->height = entry.height;
                    versions[version_count].image->max_gray = 255; // Para reconstrução
                    versions[version_count].image->pixels = pixels;
                }
                version_count++;
            }
        }
    }
    fclose(index_file);
    
    if (version_count == 0) {
        free(versions);
        return 0;
    }
    
    printf("Encontradas %d versões para reconstrução\n", version_count);
    
    // Verificar se todas as versões têm as mesmas dimensões
    int width = versions[0].image->width;
    int height = versions[0].image->height;
    
    for (int i = 1; i < version_count; i++) {
        if (versions[i].image->width != width || versions[i].image->height != height) {
            printf("Erro: Dimensões inconsistentes entre versões\n");
            for (int j = 0; j < version_count; j++) freePGM(versions[j].image);
            free(versions);
            return 0;
        }
    }
    
    // Criar imagem de reconstrução (média)
    PGMImage* reconstructed = (PGMImage*)malloc(sizeof(PGMImage));
    if (!reconstructed) {
        for (int i = 0; i < version_count; i++) freePGM(versions[i].image);
        free(versions);
        return 0;
    }
    
    reconstructed->width = width;
    reconstructed->height = height;
    reconstructed->max_gray = 255;
    reconstructed->pixels = (int**)malloc(height * sizeof(int*));
    
    if (!reconstructed->pixels) {
        free(reconstructed);
        for (int i = 0; i < version_count; i++) freePGM(versions[i].image);
        free(versions);
        return 0;
    }
    
    // Alocar e inicializar matriz de soma
    for (int i = 0; i < height; i++) {
        reconstructed->pixels[i] = (int*)malloc(width * sizeof(int));
        if (!reconstructed->pixels[i]) {
            for (int j = 0; j < i; j++) free(reconstructed->pixels[j]);
            free(reconstructed->pixels);
            free(reconstructed);
            for (int k = 0; k < version_count; k++) freePGM(versions[k].image);
            free(versions);
            return 0;
        }
        memset(reconstructed->pixels[i], 0, width * sizeof(int));
    }
    
    // Somar todas as versões
    for (int v = 0; v < version_count; v++) {
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                // Converter binário (0/1) para escala aproximada
                int value = versions[v].image->pixels[i][j] * 255;
                reconstructed->pixels[i][j] += value;
            }
        }
    }
    
    // Calcular média e normalizar
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            reconstructed->pixels[i][j] /= version_count;
            // Garantir que está no range 0-255
            if (reconstructed->pixels[i][j] < 0) reconstructed->pixels[i][j] = 0;
            if (reconstructed->pixels[i][j] > 255) reconstructed->pixels[i][j] = 255;
        }
    }
    
    // Salvar imagem reconstruída
    int success = writePGM(output_filename, reconstructed);
    
    // Liberar memória
    freePGM(reconstructed);
    for (int i = 0; i < version_count; i++) freePGM(versions[i].image);
    free(versions);
    
    return success;
}