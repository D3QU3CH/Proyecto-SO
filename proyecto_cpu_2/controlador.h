#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "modelo.h"

void *hiloDispositivoES(void *arg);
void *hiloReloj(void *arg);
void *hiloEntrada(void *arg);
void  procesarColaES(Cola *colaES, Cola *colaListos);
void  guardarBCPs(Lista *enEjecucion, const char *ruta);
void  guardarVariablesGlobales(const char *ruta);
void  logEvento(const char *msg);

#endif