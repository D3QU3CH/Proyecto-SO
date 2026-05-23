#ifndef PROCESO_H
#define PROCESO_H


typedef struct {

    char id[10]; // identificador del proceso

    int tiempoLlegada; // ciclo en que llega al sistema
    int ciclosTotales; // total de ciclos que necesita
    int ciclosRestantes; // ciclos pendientes por ejecutar

    // 0 = listo
    // 1 = ejecutando
    // 2 = espera E/S
    // 3 = terminado
    // 4 = bloqueado por recurso (seccion critica)
    int estado; // estado actual del proceso

    int rafagaActual; // ciclos ejecutados en la iteracion actual
    int tiempoEspera; // tiempo acumulado esperando CPU
    int tiempoEjecucion; // tiempo total ejecutado en CPU

    int tipoProceso; // tipo o categoría del proceso
    int desperdicio; // ciclos desperdiciados de CPU

    int vecesEnCPU; // cuantas veces ha entrado a CPU
    int esApropiativo; // indica si tiene prioridad (1 = sí)

    int dispositivoES; // dispositivo de entrada/salida asignado
    int tiempoES; // tiempo que dura en E/S

    int bloqueado; // indica si esta bloqueado

    int tiempoRespuesta; // tiempo desde llegada hasta primera ejecucion
    int tiempoRetorno; // tiempo total desde llegada hasta finalizacion

    int iteraciones; // veces que ha sido procesado en el ciclo

    int aprovechamiento; // porcentaje de uso de CPU
    int usoMemoria; // memoria que utiliza

    int variable1; // recurso 1 asignado
    int variable2; // recurso 2 asignado

    int enSeccionCritica; // indica si esta usando recursos criticos

    int restanteQuantum; // quantum restante en RR

    int cambiosContexto; // cantidad de cambios de contexto

} Proceso;

void inicializarProceso(Proceso* p, int index);

#endif