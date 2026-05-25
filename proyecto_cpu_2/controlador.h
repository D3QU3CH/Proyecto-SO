#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "modelo.h"

// ─────────────────────────────────────────────────────────────────────────────
// GLOBALES DEL CONTROLADOR
// ─────────────────────────────────────────────────────────────────────────────

extern Cola colaTerminados;
extern int  totalTerminados;

// ─────────────────────────────────────────────────────────────────────────────
// PLANIFICACION
//
// Ambos algoritmos reciben `colaListos` que es la UNICA cola dinamica.
// Los procesos entran y salen de colaListos pero su BCP vive en
// listaProcesosEnEjecucion[] o listaNuevasSolicitudes[].
// ─────────────────────────────────────────────────────────────────────────────

// Un ciclo FCFS: toma el primero de colaListos, ejecuta rafaga,
// luego manda a E/S o marca terminado (estado=3, no se encola mas).
void ejecutarFCFS(Cola *colaListos, SistemaES *es,
                  int *algoritmo, int reloj,
                  pthread_mutex_t *mtxSocios);

// Un ciclo RR: atiende el frente de colaListos con quantum variable.
// Si no termina lo regresa a colaListos (estado=0).
// Si va a E/S va a la cola del dispositivo (estado=2).
// Si termina queda estado=3, nunca vuelve a encolarse.
void ejecutarRR(Cola *colaListos, SistemaES *es,
                int *algoritmo, int *quantum, int reloj,
                pthread_mutex_t *mtxSocios);

// ─────────────────────────────────────────────────────────────────────────────
// E/S
// ─────────────────────────────────────────────────────────────────────────────

// Asigna tiempo de E/S segun dispositivo (tipo 0=disco 1=pantalla 2=teclado 3=impresora)
void asignarTiempoES(Proceso *p, int tipo);

// Procesa todas las colas E/S un tick (version sincrona, usada en FCFS)
void procesarES(SistemaES *es, Cola *colaListos);

// ─────────────────────────────────────────────────────────────────────────────
// HILOS DE E/S
// ─────────────────────────────────────────────────────────────────────────────

// Argumento para cada hilo de dispositivo E/S
typedef struct {
    Cola            *colaES;        // cola del dispositivo especifico
    Cola            *colaListos;    // a donde vuelven cuando terminan E/S
    pthread_mutex_t *mutex;
    sem_t           *sem;
    int             *terminado;
    const char      *nombre;
} ArgHiloES;

// Hilo que atiende un dispositivo E/S: espera en semaforo, procesa su cola
void *hiloDispositivoES(void *arg);

// Hilo del reloj: cada 50ms limpia bits R y despierta hilos E/S
void *hiloReloj(void *arg);

// Hilo de teclado: escucha entrada sin bloquear el ciclo principal
void *hiloTeclado(void *arg);

// ─────────────────────────────────────────────────────────────────────────────
// METRICAS Y CONTROL DE COLAS
// ─────────────────────────────────────────────────────────────────────────────

// Incrementa tiempoEspera de todos los procesos en estado LISTO dentro de colaListos
void actualizarEspera(Cola *colaListos);

// Ajusta quantum segun proporcion Listos vs E/S (se llama cada 20 iteraciones)
void evaluarColas(int *quantum, Cola *colaListos, SistemaES *es);

// ─────────────────────────────────────────────────────────────────────────────
// CAMBIO AUTOMATICO DE ALGORITMO
// ─────────────────────────────────────────────────────────────────────────────

// Analiza variables de la tabla y del BCP para decidir si cambiar algoritmo.
// Retorna el algoritmo recomendado (1=FCFS, 2=RR).
int decidirCambio(Cola *colaListos, int algoritmoActual);

// ─────────────────────────────────────────────────────────────────────────────
// APROPIATIVIDAD
// ─────────────────────────────────────────────────────────────────────────────

// Muestra top-5 procesos mas rezagados (mas ciclosRestantes) y pide elegir uno
Proceso *seleccionarProcesoCritico(Cola *colaListos);

// Mueve el proceso privilegiado al frente de colaListos
void moverAlFrente(Cola *cola, Proceso *p);

// ─────────────────────────────────────────────────────────────────────────────
// TECLADO
// ─────────────────────────────────────────────────────────────────────────────

// Lee teclas sin bloquear. Acciones:
//   X = cambiar algoritmo
//   A = apropiatividad (solo en RR)
//   M = mostrar memoria (frutas)
//   S = estado del sistema
//   N = estado NRU
void manejarEntrada(Cola *colaListos, int *algoritmo,
                    int quantum, int terminados);

// ─────────────────────────────────────────────────────────────────────────────
// LOGS / PERSISTENCIA
// ─────────────────────────────────────────────────────────────────────────────

// Guarda BCP completo (25 variables) de todos los procesos en ambas listas
void guardarTablaProcesos(void);

// Guarda 20 variables globales del sistema
void guardarVariablesGlobales(Cola *colaListos,
                              int   algoritmo,
                              int   quantum,
                              int   iteracionCPU);

// Agrega un evento con timestamp al log cronologico
void logEvento(const char *msg);

#endif