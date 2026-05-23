#ifndef MEMORIA_H
#define MEMORIA_H

#define TAM_MEM 20

extern char memoria[TAM_MEM][20];
extern int recursoOcupado[TAM_MEM];

void inicializarMemoria();
int usarRecurso(int index);
void liberarRecurso(int index);

#endif