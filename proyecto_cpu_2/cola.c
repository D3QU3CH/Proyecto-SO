#include <stdio.h>
#include <stdlib.h>
#include "cola.h"

void inicializarCola(Cola *c)
{
    c->frente = NULL;
    c->final = NULL;
    c->tamanio = 0;
}

void encolar(Cola *c, Proceso *p)
{

    Nodo *nuevo = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso = p;
    nuevo->siguiente = NULL;

    if (c->final == NULL)
    {
        c->frente = c->final = nuevo;
    }
    else
    {
        c->final->siguiente = nuevo;
        c->final = nuevo;
    }

    c->tamanio++;
}

Proceso *desencolar(Cola *c)
{

    if (estaVacia(c))
        return NULL;

    Nodo *temp = c->frente;
    Proceso *p = temp->proceso;

    c->frente = c->frente->siguiente;

    if (c->frente == NULL)
        c->final = NULL;

    free(temp);
    c->tamanio--;

    return p;
}

int estaVacia(Cola *c)
{
    return c->frente == NULL;
}

void mostrarCola(Cola *c)
{

    Nodo *actual = c->frente;

    while (actual != NULL)
    {
        printf("%s -> ", actual->proceso->id);
        actual = actual->siguiente;
    }

    printf("NULL\n");
}

void liberarCola(Cola *c)
{
    while (!estaVacia(c))
    {
        desencolar(c);
    }
}