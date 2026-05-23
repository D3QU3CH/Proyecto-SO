#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sistema.h"

Proceso tablaProcesos[TOTAL_PROCESOS];

int tiemposUsados[801] = {0};

int indiceSiguiente = EN_SISTEMA;

int generarTiempoUnico() {
    int t;
    do {
        t = rand() % 800;
    } while (tiemposUsados[t] == 1);

    tiemposUsados[t] = 1;
    return t;
}

void inicializarSistema() {
    for (int i = 0; i < TOTAL_PROCESOS; i++) {
        inicializarProceso(&tablaProcesos[i], i);
        tablaProcesos[i].tiempoLlegada = generarTiempoUnico();
    }
}

void ordenarPorLlegada() {
    for (int i = 0; i < TOTAL_PROCESOS - 1; i++) {
        for (int j = i + 1; j < TOTAL_PROCESOS; j++) {
            if (tablaProcesos[i].tiempoLlegada > tablaProcesos[j].tiempoLlegada) {
                Proceso temp = tablaProcesos[i];
                tablaProcesos[i] = tablaProcesos[j];
                tablaProcesos[j] = temp;
            }
        }
    }
}

void cargarProcesosEnCola(Cola* procesosEnCiclo, Cola* nuevasSolicitudes) {

    ordenarPorLlegada(); 

    for (int i = 0; i < EN_SISTEMA; i++) {
        encolar(procesosEnCiclo, &tablaProcesos[i]);
    }

    for (int i = EN_SISTEMA; i < TOTAL_PROCESOS; i++) {
        encolar(nuevasSolicitudes, &tablaProcesos[i]);
    }
}

void insertarOrdenadoPorLlegada(Cola* cola, Proceso* p){

    Nodo* nuevo = (Nodo*)malloc(sizeof(Nodo));
    nuevo->proceso = p;
    nuevo->siguiente = NULL;

    if(cola->frente == NULL || p->tiempoLlegada < cola->frente->proceso->tiempoLlegada){
        nuevo->siguiente = cola->frente;
        cola->frente = nuevo;
        return;
    }

    Nodo* actual = cola->frente;

    while(actual->siguiente != NULL && actual->siguiente->proceso->tiempoLlegada <= p->tiempoLlegada){
        actual = actual->siguiente;
    }

    nuevo->siguiente = actual->siguiente;
    actual->siguiente = nuevo;
}

void ingresarProcesosNuevos(Cola* procesosEnCiclo, int reloj) {

    while (indiceSiguiente < TOTAL_PROCESOS) {

        Proceso* p = &tablaProcesos[indiceSiguiente];

        if (p->tiempoLlegada <= reloj) {
            insertarOrdenadoPorLlegada(procesosEnCiclo, p);
            indiceSiguiente++;
        } else {
            break;
        }
    }
}


