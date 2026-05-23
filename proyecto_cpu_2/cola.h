#ifndef COLA_H
#define COLA_H

#include "proceso.h"

typedef struct Nodo
{
    Proceso *proceso;
    struct Nodo *siguiente;
} Nodo;

typedef struct
{
    Nodo *frente;
    Nodo *final;
    int tamanio;
} Cola;

void inicializarCola(Cola *c);
void encolar(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int estaVacia(Cola *c);
void mostrarCola(Cola *c);

void liberarCola(Cola *c);

#endif