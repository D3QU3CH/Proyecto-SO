#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "modelo.h"

void manejarEntrada(ContextoHilos *ctx);

void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId, int reloj);
void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
                int *quantum, int *iteracionesRR,
                int histDesp[], int histCiclo[], int *histIdx, int reloj);

void  procesarColaES(Cola *colaES, Cola *colaListos);
void *hiloDispositivoES(void *arg);
void *hiloReloj(void *arg);

void ajustarQuantumAutomatico(Cola *colaListos, SistemaES *es, int iteracionesRR);
void mostrarEnvejecimiento(Cola *colaListos);
void mostrarDesperdiciadores(Cola *colaListos);

void guardarBCPs(Lista *enEjecucion, const char *ruta);
void guardarVariablesGlobales(const char *ruta);
void logEvento(const char *msg);

#endif