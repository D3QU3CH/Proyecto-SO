#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H

#include "cola.h"
#include "es.h"

extern Cola colaTerminados;
extern int totalTerminados;

void evaluarColas(int *quantum, Cola *cola, SistemaES *io);

//  ALGORITMOS
void ejecutarFCFS(Cola *colaListos, SistemaES *io, int *algoritmo, int reloj);
void ejecutarRR(Cola *colaEnCiclo, Cola *nuevasSolicitudes, SistemaES *es, int *algoritmo, int *quantum, int reloj);

void mostrarBalanceColas(Cola *cola, SistemaES *io);

// ESPERA
void actualizarEspera(Cola *cola);

// HISTORIAL CPU
void pushHistorial(int uso);
void mostrarHistorialCPU();

#endif