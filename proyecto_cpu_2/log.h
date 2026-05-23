#ifndef LOG_H
#define LOG_H

#include "cola.h"

//Guarda todos los procesos (BCP 25 variables) 
void guardarTablaProcesos(Cola *ejecucion, Cola *nuevas);

//Guarda 20 variables generales 
void guardarVariablesGlobales(Cola *procesosCiclo, Cola *nuevasSolicitudes, int algoritmo, int quantum, int iteracionCPU, int procesosNuevos);

//eventos 
void logEvento(const char *msg);

//TOPS 
void mostrarEnvejecimiento(Cola *cola);
void mostrarTopDesperdicio(Cola *cola);

#endif