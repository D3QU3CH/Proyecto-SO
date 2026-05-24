#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "modelo.h"

// GLOBALES DEL CONTROLADOR

extern Cola colaTerminados;
extern int  totalTerminados;

// PLANIFICACION

// Ejecuta un ciclo FCFS: desencola el primero listo y lo ejecuta
void ejecutarFCFS(Cola *colaListos, SistemaES *es,
                  int *algoritmo, int reloj);

// Ejecuta un ciclo Round Robin con quantum variable
void ejecutarRR(Cola *colaEnCiclo, Cola *nuevasSolicitudes,
                SistemaES *es, int *algoritmo,
                int *quantum, int reloj);

// E/S

// Asigna tiempo de espera segun dispositivo (tipo 0-3)
void asignarTiempoES(Proceso *p, int tipo);

// Procesa un ciclo en todas las colas de E/S; devuelve los listos a colaListos
void procesarES(SistemaES *es, Cola *colaListos);

// CONTROL DE COLAS Y METRICAS

// Incrementa tiempoEspera de todos los procesos listos
void actualizarEspera(Cola *colaEnCiclo);

// Ajusta quantum segun proporcion Listos vs E/S
void evaluarColas(int *quantum, Cola *colaEnCiclo, SistemaES *es);

// CONTROL AUTOMATICO DE ALGORITMO

// Analiza metricas y decide si conviene cambiar de algoritmo. Retorna el algoritmo recomendado (1=FCFS, 2=RR).
int decidirCambio(Cola *colaListos, int algoritmoActual);

// APROPIATIVIDAD

// Muestra los 5 procesos con mas ciclos pendientes y pide al usuario elegir uno; lo marca como apropiativo.
Proceso *seleccionarProcesoCritico(Cola *procesosEnCiclo);

// Mueve un proceso privilegiado al frente de la cola
void moverAlFrente(Cola *cola, Proceso *p);

// TECLADO

// Detecta teclas sin bloquear y ejecuta la accion correspondiente: X=cambiar algoritmo  A=apropiatividad(RR)  M=memoria  S=estado
void manejarEntrada(Cola *colaListos, int *algoritmo,
                    int quantum, int terminados);

// LOGS / PERSISTENCIA

// Guarda BCP completo (25 variables) de cada proceso activo
void guardarTablaProcesos(Cola *colaEnCiclo, Cola *nuevasSolicitudes);

// Guarda 20 variables globales del sistema
void guardarVariablesGlobales(Cola *colaEnCiclo,
                              Cola *nuevasSolicitudes,
                              int   algoritmo,
                              int   quantum,
                              int   iteracionCPU,
                              int   procesosNuevos);

// Agrega un evento con timestamp al log cronologico
void logEvento(const char *msg);

#endif