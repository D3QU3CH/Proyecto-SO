#ifndef MASTER_H
#define MASTER_H

#include "modelo.h"

// Mensajes PVM
#define MSG_DATOS_PROCESOS   100
#define MSG_RESULTADO_STATS  101
#define MSG_DATOS_RR         102
#define MSG_RESULTADO_RR     103
#define MSG_FIN              199

// Resultado tarea 1: estadísticas parciales
typedef struct {
    int   finalizados;
    int   enEspera;
    int   enES;
    float promCiclosPendientes;
    // Top 3 desperdiciadores (id + desperdicio)
    char  topId[3][12];
    int   topDesp[3];
} ResultadoStats;

// Resultado tarea 2: análisis RR
typedef struct {
    // Top 3 perjudicados (quantum pequeño)
    char  perjId[3][12];
    int   perjDelta[3];   // rafagaActual - quantum (>0 = se cortó)
    // Top 3 desperdiciadores
    char  despId[3][12];
    int   despVal[3];
    float promAprovechamiento;
    int   totalRetornos;  // suma vecesEnCPU del subconjunto
} ResultadoRR;

// Serialización simple: campos planos de Proceso que necesitamos
typedef struct {
    char id[12];
    int  estado;
    int  ciclosRestantes;
    int  tiempoEspera;
    int  dispositivoES;
    int  desperdicio;
    int  aprovechamiento;
    int  rafagaActual;
    int  vecesEnCPU;
} ProcesoSerial;

void ejecutarMasterPVM(Lista *enEjecucion, int quantum);

#endif