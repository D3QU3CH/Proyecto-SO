#include <stdio.h>
#include "memoria.h"

char memoria[TAM_MEM][20] = {
    "manzana","pera","uva","melon","sandia",
    "kiwi","mango","fresa","piña","papaya",
    "limon","naranja","coco","guayaba","ciruela",
    "banana","durazno","granada","higo","tamarindo"
};

// 0 = libre
// 1 = ocupado
int recursoOcupado[TAM_MEM];

void inicializarMemoria(){

    // Marca todos los recursos como libres
    for(int i = 0; i < TAM_MEM; i++){
        recursoOcupado[i] = 0;
    }
}

// Intenta usar un recurso
// retorna 1 si lo obtiene
// retorna 0 si esta ocupado
int usarRecurso(int index){

    // si ya esta ocupado no lo puede usar
    if(recursoOcupado[index] == 1){
        return 0;
    }

    // lo marca como ocupado
    recursoOcupado[index] = 1;

    return 1;
}

// Libera el recurso
void liberarRecurso(int index){

    recursoOcupado[index] = 0;
}