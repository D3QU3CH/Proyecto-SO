#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "modelo.h"

typedef struct {
    Cola            *colaES;
    Cola            *colaListos;
    pthread_mutex_t *mutex;
    sem_t           *sem;
    int             *terminado;
    const char      *nombre;
} ArgHiloES;

void *hiloDispositivoES(void *arg);
void *hiloReloj(void *arg);
void *hiloEntrada(void *arg);

void procesarColaES(Cola *colaES, Cola *colaListos);
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj);
void actualizarEspera(Cola *colaListos);

void guardarBCPs(Lista *enEjecucion, const char *ruta);
void guardarVariablesGlobales(const char *ruta);
void logEvento(const char *msg);

#endif