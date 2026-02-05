#include <stdio.h>
#include <stdlib.h>
#include "image_manager.h"

/**
 * Sistema de Gerenciamento de Imagens Binárias
 * 
 * Funcionalidades implementadas:
 * - Compressão RLE de imagens binárias
 * - Armazenamento com arquivo de índices
 * - Remoção lógica e compactação física
 * - Recuperação de imagens em formato PGM
 * - Reconstrução da imagem original (Bônus)
 */

void displayMenu() {
    printf("\n=== SISTEMA DE GERENCIAMENTO DE IMAGENS BINÁRIAS ===\n");
    printf("1. Adicionar imagem ao banco de dados\n");
    printf("2. Listar imagens no banco de dados\n");
    printf("3. Remover imagem do banco de dados\n");
    printf("4. Recuperar imagem do banco de dados\n");
    printf("5. Compactar banco de dados\n");
    printf("6. Reconstruir imagem original (Bônus)\n");
    printf("0. Sair\n");
    printf("Escolha uma opção: ");
}

int main() {
    int choice, threshold;
    char filename[100], output_name[100];
    
    // Inicializa os arquivos do banco de dados
    initializeDatabase();
    
    printf("Sistema de Gerenciamento de Imagens Binárias Inicializado\n");
    
    do {
        displayMenu();
        scanf("%d", &choice);
        
        switch(choice) {
            case 1:
                printf("Nome do arquivo PGM: ");
                scanf("%s", filename);
                printf("Valor de limiar (0-255): ");
                scanf("%d", &threshold);
                if (addImageToDatabase(filename, threshold)) {
                    printf("Imagem adicionada com sucesso!\n");
                } else {
                    printf("Erro ao adicionar imagem.\n");
                }
                break;
                
            case 2:
                if (!listImagesInDatabase()) {
                    printf("Nenhuma imagem encontrada no banco de dados.\n");
                }
                break;
                
            case 3:
                printf("Nome da imagem: ");
                scanf("%s", filename);
                printf("Limiar utilizado: ");
                scanf("%d", &threshold);
                if (removeImageFromDatabase(filename, threshold)) {
                    printf("Imagem removida com sucesso!\n");
                } else {
                    printf("Imagem não encontrada.\n");
                }
                break;
                
            case 4:
                printf("Nome da imagem: ");
                scanf("%s", filename);
                printf("Limiar utilizado: ");
                scanf("%d", &threshold);
                printf("Nome do arquivo de saída: ");
                scanf("%s", output_name);
                if (retrieveImageFromDatabase(filename, threshold, output_name)) {
                    printf("Imagem recuperada: %s\n", output_name);
                } else {
                    printf("Erro ao recuperar imagem.\n");
                }
                break;
                
            case 5:
                if (compactDatabase()) {
                    printf("Banco de dados compactado com sucesso!\n");
                } else {
                    printf("Erro durante a compactação.\n");
                }
                break;
                
            case 6:
                printf("Nome da imagem para reconstrução: ");
                scanf("%s", filename);
                printf("Nome do arquivo de saída: ");
                scanf("%s", output_name);
                if (reconstructOriginalImage(filename, output_name)) {
                    printf("Imagem reconstruída: %s\n", output_name);
                } else {
                    printf("Erro na reconstrução da imagem.\n");
                }
                break;
                
            case 0:
                printf("Encerrando sistema...\n");
                break;
                
            default:
                printf("Opção inválida!\n");
        }
    } while(choice != 0);
    
    return 0;
}