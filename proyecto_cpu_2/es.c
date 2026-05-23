#include <stdio.h>
#include <stdlib.h>
#include "es.h"
#include "control.h"

void inicializarES(SistemaES* es) {
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

void asignarTiempoES(Proceso* p, int tipo) {

    int tiempo = rand()%100 + 1;

    if(tipo == 0) tiempo *= 2;
    if(tipo == 1) tiempo *= 4;
    if(tipo == 2) tiempo *= 8;
    if(tipo == 3) tiempo *= 12;

    p->tiempoES = tiempo;
}

void procesarColaES(Cola* colaES, Cola* colaEnCiclo) {

    int size = colaES->tamanio;

    while(size--) {

        Proceso* p = desencolar(colaES);
        if (!p) continue;

        p->tiempoES--;

        if (p->tiempoES <= 0) {

            p->bloqueado = 0;
            p->dispositivoES = -1;

            printf(">>> %s SALE DE ES -> LISTOS\n", p->id);

            p->estado = 0;

            if (p->esApropiativo)
                moverAlFrente(colaEnCiclo, p);
            else
                encolar(colaEnCiclo, p);

        } 
        else {
            encolar(colaES, p);
        }
    }
}

void procesarES(SistemaES* es, Cola* colaListos) {

    procesarColaES(&es->disco, colaListos);
    procesarColaES(&es->pantalla, colaListos);
    procesarColaES(&es->teclado, colaListos);
    procesarColaES(&es->impresora, colaListos);
}