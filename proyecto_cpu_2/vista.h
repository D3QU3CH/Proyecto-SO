#ifndef VISTA_H
#define VISTA_H

#include "modelo.h"

// ─────────────────────────────────────────────────────────────────────────────
// CODIGOS DE COLOR ANSI
// ─────────────────────────────────────────────────────────────────────────────

#define RESET    "\033[0m"
#define ROJO     "\033[31m"
#define VERDE    "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL     "\033[34m"
#define MAGENTA  "\033[35m"
#define CIAN     "\033[36m"
#define BLANCO   "\033[37m"
#define NEGRITA  "\033[1m"

// ─────────────────────────────────────────────────────────────────────────────
// HISTORIAL CPU
// ─────────────────────────────────────────────────────────────────────────────

void vistaPushHistorial(int uso);
void vistaMostrarHistorialCPU(void);

// ─────────────────────────────────────────────────────────────────────────────
// BALANCE DE COLAS
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarBalanceColas(Cola *colaEnCiclo, SistemaES *es);

// ─────────────────────────────────────────────────────────────────────────────
// ENVEJECIMIENTO Y DESPERDICIO
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarEnvejecimiento(Cola *cola);
void vistaMostrarTopDesperdicio(Cola *cola);

// ─────────────────────────────────────────────────────────────────────────────
// ESTADO DEL SISTEMA
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarEstadoSistema(Cola *colaListos,
                               int algoritmo,
                               int quantum,
                               int terminados);

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA (frutas)
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarMemoria(void);

// ─────────────────────────────────────────────────────────────────────────────
// NRU – estado de marcos de pagina
// ─────────────────────────────────────────────────────────────────────────────

// Muestra los marcos NRU de todos los procesos activos en la cola
void vistaMostrarNRU(Cola *cola);

// ─────────────────────────────────────────────────────────────────────────────
// INICIO Y CIERRE
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarBienvenida(void);
void vistaMostrarCierre(int ciclos, int terminados);

// ─────────────────────────────────────────────────────────────────────────────
// MENSAJES DE EVENTOS
// ─────────────────────────────────────────────────────────────────────────────

void vistaMensajeAlgoritmo(int algoritmo);
void vistaMensajeProcesoCritico(Proceso *p);
void vistaMensajeCambioAutomatico(int de, int a);

#endif