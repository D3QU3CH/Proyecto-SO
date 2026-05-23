#ifndef SISTEMA_H
#define SISTEMA_H

#include "proceso.h"
#include "cola.h"

#define TOTAL_PROCESOS 250
#define EN_SISTEMA 150

extern Proceso tablaProcesos[TOTAL_PROCESOS];

void inicializarSistema();
void ordenarPorLlegada();
void cargarProcesosEnCola(Cola* procesosEnCiclo, Cola* nuevasSolicitudes);
void ingresarProcesosNuevos(Cola* procesosEnCiclo, int reloj);

#endif