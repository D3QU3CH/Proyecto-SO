#ifndef VISTA_H
#define VISTA_H

#include "modelo.h"

// Codigos de color ANSI

#define RESET    "\033[0m"
#define ROJO     "\033[31m"
#define VERDE    "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL     "\033[34m"
#define MAGENTA  "\033[35m"
#define CIAN     "\033[36m"
#define BLANCO   "\033[37m"
#define NEGRITA  "\033[1m"


// Historial CPU

void vistaPushHistorial(int uso);
void vistaMostrarHistorialCPU(void);


// Balance de colas

void vistaMostrarBalanceColas(Cola *colaEnCiclo, SistemaES *es);


// Envejecimiento y desperdicio

void vistaMostrarEnvejecimiento(Cola *cola);
void vistaMostrarTopDesperdicio(Cola *cola);


// Estado del sistema

void vistaMostrarEstadoSistema(Cola *colaListos,
                               int algoritmo,
                               int quantum,
                               int terminados);


// Memoria (frutas)

void vistaMostrarMemoria(void);


// Inicio y cierre

void vistaMostrarBienvenida(void);
void vistaMostrarCierre(int ciclos, int terminados);

// Mensajes de eventos

void vistaMensajeAlgoritmo(int algoritmo);
void vistaMensajeProcesoCritico(Proceso *p);
void vistaMensajeCambioAutomatico(int de, int a);

#endif