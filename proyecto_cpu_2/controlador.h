#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "modelo.h"
#include <signal.h>
#include <pthread.h>

// Terminal
void termGuardar(void);
void termRestaurar(void);
void termRaw(void);
void termBloqueo(void);
char leerTecla(void);
int  leerLinea(char *buf, int maxlen);

void manejadorSenal(int sig);

//MANEJO DE TECLAS
void manejarTeclaX(int *algoritmo, int *quantum, int reloj,
        int *cicloUltimoCambio, pthread_mutex_t *mutex);
void manejarTeclaA(Cola *colaListos, SistemaES *es,
        int *procesoPrivilId, pthread_mutex_t *mutex);

// Scheduling
void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId, int reloj);
void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
        int *quantum, int *iteracionesRR,
        int histDesp[], int histCiclo[], int *histIdx, int reloj);

// E/S
void procesarColaES(Cola *colaES, Cola *colaListos);

// AJUSTE DE QUANTUM Y REPORTES
void ajustarQuantumAutomatico(Cola *colaListos, SistemaES *es, int iteracionesRR);
void mostrarEnvejecimiento(Cola *colaListos);
void mostrarDesperdiciadores(Cola *colaListos);

// PERSISTECIA
void guardarBCPs(Lista *enEjecucion, const char *ruta);
void guardarVariablesGlobales(const char *ruta);

#endif