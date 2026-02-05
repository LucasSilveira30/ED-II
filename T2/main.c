#include <stdio.h>
#include <stdlib.h>
#include "btree.h"
#include "image.h"

#define MAX_THRESHOLDS 10

/**
 * Sistema de Gerenciamento de Imagens com Árvore-B
 * 
 * Funcionalidades implementadas:
 * Árvore-B de ordem 3 páginada
 * Operações de inserção e remoção clássicas
 * Inserção com múltiplos limiares
 * Virtualização da raiz
 * Compactação apenas do arquivo de dados
 * Impressão do conteúdo das páginas
 */

void display_menu() {
    printf("\n=== SISTEMA DE IMAGENS COM ÁRVORE-B ===\n");
    printf("1. Adicionar imagem com múltiplos limiares\n");
    printf("2. Adicionar imagem com limiar único\n");
    printf("3. Recuperar imagem\n");
    printf("4. Listar todas as imagens (ordenado)\n");
    printf("5. Remover imagem\n");
    printf("6. Compactar arquivo de dados\n");
    printf("7. Imprimir conteúdo das páginas\n");
    printf("8. Percurso ordenado\n");
    printf("0. Sair\n");
    printf("========================================\n");
    printf("Escolha: ");
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main() {
    btree_init();
    
    printf("===============================================\n");
    printf("  SISTEMA DE GERENCIAMENTO DE IMAGENS BINÁRIAS\n");
    printf("         ÁRVORE-B DE ORDEM %d (PÁGINADA)\n", ORDER);
    printf("===============================================\n");
    
    int choice;
    char filename[100], output[100];
    int threshold, count;
    int thresholds[MAX_THRESHOLDS];
    
    do {
        display_menu();
        
        if (scanf("%d", &choice) != 1) {
            printf("Entrada inválida!\n");
            clear_input_buffer();
            continue;
        }
        
        switch (choice) {
            case 1:
                printf("Nome do arquivo PGM: ");
                scanf("%99s", filename);
                printf("Quantidade de limiares (1-%d): ", MAX_THRESHOLDS);
                if (scanf("%d", &count) != 1 || count < 1 || count > MAX_THRESHOLDS) {
                    printf("Quantidade inválida!\n");
                    clear_input_buffer();
                    break;
                }
                printf("Digite os %d limiares (separados por espaço): ", count);
                for (int i = 0; i < count; i++) {
                    if (scanf("%d", &thresholds[i]) != 1) {
                        printf("Limiar inválido!\n");
                        clear_input_buffer();
                        break;
                    }
                }
                database_add_multiple_thresholds(filename, thresholds, count);
                break;
                
            case 2:
                printf("Nome do arquivo PGM: ");
                scanf("%99s", filename);
                printf("Limiar (0-255): ");
                if (scanf("%d", &threshold) != 1) {
                    printf("Limiar inválido!\n");
                    clear_input_buffer();
                    break;
                }
                database_add_image(filename, threshold);
                break;
                
            case 3:
                printf("Nome da imagem: ");
                scanf("%99s", filename);
                printf("Limiar utilizado: ");
                if (scanf("%d", &threshold) != 1) {
                    printf("Limiar inválido!\n");
                    clear_input_buffer();
                    break;
                }
                printf("Nome do arquivo de saída: ");
                scanf("%99s", output);
                database_retrieve_image(filename, threshold, output);
                break;
                
            case 4:
                database_list_images();
                break;
                
            case 5:
                printf("Nome da imagem: ");
                scanf("%99s", filename);
                printf("Limiar: ");
                if (scanf("%d", &threshold) != 1) {
                    printf("Limiar inválido!\n");
                    clear_input_buffer();
                    break;
                }
                btree_delete(filename, threshold);
                printf("Operação de remoção concluída\n");
                break;
                
            case 6:
                database_compact();
                break;
                
            case 7:
                btree_print_pages();
                break;
                
            case 8:
                btree_print_inorder();
                break;
                
            case 0:
                printf("Encerrando o sistema...\n");
                break;
                
            default:
                printf("Opção inválida! Tente novamente.\n");
        }
        
        clear_input_buffer();
        
    } while (choice != 0);
    
    return 0;
}