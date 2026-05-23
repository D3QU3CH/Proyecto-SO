#ifndef CONTROL_H
#define CONTROL_H

#include "cola.h"

// Decide si se debe cambiar de algoritmo automaticamente
int decidirCambio(Cola* colaListos, int algoritmoActual);

//Muestra los 5 procesos mas rezagados (mas ciclos restantes)
Proceso* seleccionarProcesoCritico(Cola* colaListos);

//Mueve el proceso privilegiado al frente de la cola de listos 
void moverAlFrente(Cola* cola, Proceso* p);

#endif