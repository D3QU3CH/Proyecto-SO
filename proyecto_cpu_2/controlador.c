#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "controlador.h"
#include "vista.h"

/* ═══════════════════════════════════════════════════════════════════════════
   HELPER INTERNO: Proceso apropiativo siempre al frente
   ═══════════════════════════════════════════════════════════════════════════ */
static void ponerApropiatvioAlFrente(Cola *colaListos)
{
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        if (n->proceso->esApropiativo) {
            moverAlFrenteCola(colaListos, n->proceso);
            break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   FCFS
   ═══════════════════════════════════════════════════════════════════════════ */
void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    ponerApropiatvioAlFrente(colaListos);

    Proceso *p = desencolar(colaListos);
    if (!p) return;

    /* Proceso ya sin ciclos pendientes (puede ocurrir si llegó marcado) */
    if (p->ciclosRestantes <= 0) {
        if (p->esApropiativo) {
            printf("[FCFS] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
        procesarTerminacion(p, reloj);
        printf("[FCFS] %s TERMINO\n", p->id);
        tablaSistema.totalCambiosContexto++;
        return;
    }

    procesarEntradaCPU(p, reloj);
    tablaSistema.totalCambiosContexto++;

    if (p->ciclosRestantes <= 0) {
        if (p->esApropiativo) {
            printf("[FCFS] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
        procesarTerminacion(p, reloj);
        printf("[FCFS] %s TERMINO (ciclos=%d)\n", p->id, p->ciclosTotales);
    } else if (p->tipoProceso == 1 && rand() % 2 == 0) {
        /* ES-bound: 50% de probabilidad de ir a E/S → activa fallos de página */
        asignarES(p, es);
    } else if (p->tipoProceso == 0 && rand() % 4 == 0) {
        /* CPU-bound: 25% de probabilidad de ir a E/S ocasionalmente */
        asignarES(p, es);
    } else {
        p->estado = ESTADO_LISTO;
        if (p->esApropiativo) encolarAlFrente(colaListos, p);
        else                  encolar(colaListos, p);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   ROUND ROBIN
   ═══════════════════════════════════════════════════════════════════════════ */
void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
                int *quantum, int *iteracionesRR,
                int histDesp[], int histCiclo[], int *histIdx, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    ponerApropiatvioAlFrente(colaListos);

    Proceso *p = desencolar(colaListos);
    if (!p) return;

    if (p->ciclosRestantes <= 0) {
        if (p->esApropiativo) {
            printf("[RR] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
        procesarTerminacion(p, reloj);
        printf("[RR] %s TERMINO\n", p->id);
        tablaSistema.totalCambiosContexto++;
        return;
    }

    procesarEntradaCPU(p, reloj);
    tablaSistema.totalCambiosContexto++;
    (*iteracionesRR)++;

    int q       = tablaSistema.quantumActual;
    int ejecuta = (p->rafagaActual < q) ? p->rafagaActual : q;
    int desp    = q - ejecuta;

    p->aprovechamiento = (q == 0) ? 0 : (ejecuta * 100) / q;
    p->desperdicio     = desp;

    histDesp[*histIdx]  = (q == 0) ? 0 : (desp * 100) / q;
    histCiclo[*histIdx] = reloj;
    *histIdx = (*histIdx + 1) % 100;

    ajustarQuantumAutomatico(colaListos, es, *iteracionesRR);
    *quantum = tablaSistema.quantumActual;

    if (p->ciclosRestantes <= 0) {
        if (p->esApropiativo) {
            printf("[RR] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
        procesarTerminacion(p, reloj);
        printf("[RR] %s TERMINO (ciclos=%d)\n", p->id, p->ciclosTotales);
    } else if (p->rafagaActual >= q) {
        /*
         * No completó el quantum: vuelve a cola.
         * Procesos ES-bound (tipoProceso==1) van a E/S cada 3 vueltas
         * para garantizar que procesarFraseES se ejecute y haya fallos NRU.
         */
        if (p->tipoProceso == 1 && p->vecesEnCPU % 3 == 0) {
            asignarES(p, es);
        } else {
            p->estado          = ESTADO_LISTO;
            p->restanteQuantum = p->rafagaActual - ejecuta;
            if (p->esApropiativo) encolarAlFrente(colaListos, p);
            else                  encolar(colaListos, p);
        }
    } else {
        /*
         * Completó su ráfaga antes del quantum.
         * Procesos CPU-bound van ocasionalmente a E/S (1 de cada 4).
         * Procesos ES-bound siempre van a E/S.
         */
        if (p->tipoProceso == 1 || rand() % 4 == 0) {
            asignarES(p, es);
        } else {
            p->estado = ESTADO_LISTO;
            if (p->esApropiativo) encolarAlFrente(colaListos, p);
            else                  encolar(colaListos, p);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   PROCESAMIENTO DE COLA E/S
   ═══════════════════════════════════════════════════════════════════════════ */
#define TICKS_ES 10

void procesarColaES(Cola *colaES, Cola *colaListos)
{
    int n = colaES->tamanio;
    while (n--) {
        Proceso *p = desencolar(colaES);
        if (!p) continue;
        p->tiempoES -= TICKS_ES;
        if (p->tiempoES <= 0) {
            p->tiempoES      = 0;
            p->dispositivoES = -1;
            p->estado        = ESTADO_LISTO;
            if (p->esApropiativo) encolarAlFrente(colaListos, p);
            else                  encolar(colaListos, p);
        } else {
            encolar(colaES, p);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   AJUSTE AUTOMÁTICO DE QUANTUM
   Cada 20 iteraciones RR evalúa proporciones de colas.
   Si desbalance > 75-25 ajusta Q en ±5.
   ═══════════════════════════════════════════════════════════════════════════ */
void ajustarQuantumAutomatico(Cola *colaListos, SistemaES *es, int iteracionesRR)
{
    if (iteracionesRR % 20 != 0) return;

    int enListos = colaListos->tamanio;
    int enES     = es->disco.tamanio + es->pantalla.tamanio +
                   es->teclado.tamanio + es->impresora.tamanio;
    int total    = enListos + enES;
    if (total == 0) return;

    float propListos = (float)enListos / total;
    float propES     = (float)enES     / total;
    int   q          = tablaSistema.quantumActual;

    if (propListos > 0.75f) {
        /* Cola de listos muy llena: reducir Q para dar más turnos */
        q = (q > 5) ? q - 5 : q;
        printf("[RR] Desbalance listos>75%% -> Q reducido a %d\n", q);
    } else if (propES > 0.75f) {
        /* Cola E/S muy llena: aumentar Q para que procesos terminen ráfaga */
        q = (q < 100) ? q + 5 : q;
        printf("[RR] Desbalance ES>75%% -> Q aumentado a %d\n", q);
    } else if (propListos > 0.4f && propListos < 0.6f) {
        printf("[RR] Colas balanceadas (listos=%.0f%% ES=%.0f%%)\n",
               propListos * 100, propES * 100);
    }
    tablaSistema.quantumActual = q;
}

/* ═══════════════════════════════════════════════════════════════════════════
   ENVEJECIMIENTO — 5 procesos más perjudicados (mayor vecesEnCPU)
   ═══════════════════════════════════════════════════════════════════════════ */
void mostrarEnvejecimiento(Cola *colaListos)
{
    printf("\n--- TOP 5 ENVEJECIMIENTO (mas iteraciones en CPU) ---\n");
    Proceso *top[5] = {NULL};
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        for (int i = 0; i < 5; i++) {
            if (!top[i] || p->vecesEnCPU > top[i]->vecesEnCPU) {
                for (int j = 4; j > i; j--) top[j] = top[j-1];
                top[i] = p;
                break;
            }
        }
    }
    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | vecesEnCPU:%3d | ciclosRest:%6d | espera:%d\n",
               i+1, top[i]->id, top[i]->vecesEnCPU,
               top[i]->ciclosRestantes, top[i]->tiempoEspera);
    if (!top[0]) printf("  (cola vacia)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   DESPERDICIADORES — 5 procesos con mayor desperdicio de CPU
   ═══════════════════════════════════════════════════════════════════════════ */
void mostrarDesperdiciadores(Cola *colaListos)
{
    printf("\n--- TOP 5 DESPERDICIADORES DE CPU ---\n");
    Proceso *top[5] = {NULL};
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        Proceso *p    = n->proceso;
        int      desp = tablaSistema.quantumActual - p->rafagaActual;
        if (desp < 0) desp = 0;
        for (int i = 0; i < 5; i++) {
            if (!top[i]) { top[i] = p; break; }
            int despTop = tablaSistema.quantumActual - top[i]->rafagaActual;
            if (despTop < 0) despTop = 0;
            if (desp > despTop) {
                for (int j = 4; j > i; j--) top[j] = top[j-1];
                top[i] = p;
                break;
            }
        }
    }
    for (int i = 0; i < 5 && top[i]; i++) {
        int desp = tablaSistema.quantumActual - top[i]->rafagaActual;
        if (desp < 0) desp = 0;
        printf("  %d. %-8s | rafaga:%3d | Q:%3d | desperdicio:%3d\n",
               i+1, top[i]->id, top[i]->rafagaActual,
               tablaSistema.quantumActual, desp);
    }
    if (!top[0]) printf("  (cola vacia)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   PERSISTENCIA — BCP en archivo
   ═══════════════════════════════════════════════════════════════════════════ */
void guardarBCPs(Lista *enEjecucion, const char *ruta)
{
    FILE *f = fopen(ruta, "a");
    if (!f) return;
    time_t ahora = time(NULL);
    fprintf(f, "\n=== CHECKPOINT %s", ctime(&ahora));
    int num = 1;
    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente, num++) {
        Proceso *p = n->proceso;
        fprintf(f, "--- BCP #%d ---\n",      num);
        fprintf(f, " ID           : %s\n",   p->id);
        fprintf(f, " Nombre       : %s\n",   p->nombre);
        fprintf(f, " Llegada      : %d\n",   p->tiempoLlegada);
        fprintf(f, " CiclosTot    : %d\n",   p->ciclosTotales);
        fprintf(f, " CiclosRest   : %d\n",   p->ciclosRestantes);
        fprintf(f, " RafagaActual : %d\n",   p->rafagaActual);
        fprintf(f, " TiempoEjec   : %d\n",   p->tiempoEjecucion);
        fprintf(f, " TiempoEspera : %d\n",   p->tiempoEspera);
        fprintf(f, " TiempoResp   : %d\n",   p->tiempoRespuesta);
        fprintf(f, " TiempoRetorno: %d\n",   p->tiempoRetorno);
        fprintf(f, " Estado       : %d\n",   p->estado);
        fprintf(f, " VecesEnCPU   : %d\n",   p->vecesEnCPU);
        fprintf(f, " Iteraciones  : %d\n",   p->iteraciones);
        fprintf(f, " RestQuantum  : %d\n",   p->restanteQuantum);
        fprintf(f, " CambiosCtx   : %d\n",   p->cambiosContexto);
        fprintf(f, " Apropiativo  : %d\n",   p->esApropiativo);
        fprintf(f, " TipoProceso  : %d\n",   p->tipoProceso);
        fprintf(f, " Aprovech     : %d\n",   p->aprovechamiento);
        fprintf(f, " Desperdicio  : %d\n",   p->desperdicio);
        fprintf(f, " DispositivoES: %d\n",   p->dispositivoES);
        fprintf(f, " TiempoES     : %d\n",   p->tiempoES);
        fprintf(f, " Bloqueado    : %d\n",   p->bloqueado);
        fprintf(f, " Variable1    : %d\n",   p->variable1);
        fprintf(f, " Variable2    : %d\n",   p->variable2);
        fprintf(f, " FallosPag    : %d\n\n", p->fallosPagina);
    }
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PERSISTENCIA — Variables globales en archivo (20 variables)
   ═══════════════════════════════════════════════════════════════════════════ */
void guardarVariablesGlobales(const char *ruta)
{
    FILE *f = fopen(ruta, "a");
    if (!f) return;
    TablaProcesos *t = &tablaSistema;
    time_t ahora = time(NULL);
    fprintf(f, "\n=== VARIABLES GLOBALES %s", ctime(&ahora));
    fprintf(f, " 1.  Total            : %d\n", t->totalProcesos);
    fprintf(f, " 2.  En ciclo         : %d\n", t->procesosEnCiclo);
    fprintf(f, " 3.  En solicitudes   : %d\n", t->procesosEnSolicitud);
    fprintf(f, " 4.  Cola listos      : %d\n", t->procesosEnColaListos);
    fprintf(f, " 5.  Ejecutando       : %d\n", t->procesosEjecutando);
    fprintf(f, " 6.  En E/S           : %d\n", t->procesosEnES);
    fprintf(f, " 7.  Terminados       : %d\n", t->procesosTerminados);
    fprintf(f, " 8.  Bloqueados       : %d\n", t->procesosBloqueados);
    fprintf(f, " 9.  Algoritmo        : %s\n",
            t->algoritmoActual == ALG_FCFS ? "FCFS" : "RR");
    fprintf(f, "10.  Quantum          : %d\n", t->quantumActual);
    fprintf(f, "11.  Ciclo actual     : %d\n", t->cicloActual);
    fprintf(f, "12.  Cambios ctx      : %d\n", t->totalCambiosContexto);
    fprintf(f, "13.  Fallos pagina    : %d\n", t->totalFallosPagina);
    fprintf(f, "14.  Suma espera      : %d\n", t->sumaEspera);
    fprintf(f, "15.  Suma ciclos      : %d\n", t->sumaCiclosRestantes);
    fprintf(f, "16.  Prom. espera     : %d\n", t->promedioEspera);
    fprintf(f, "17.  Prom. ciclos     : %d\n", t->promedioCiclos);
    fprintf(f, "18.  Ingr. dinamico   : %d\n", t->procesosIngresadosDinam);
    fprintf(f, "19.  Mem. libre (KB)  : %d\n", t->memoriaLibreKB);
    fprintf(f, "20.  Desperdicio (KB) : %d\n", t->desperdicioTotal);
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   LOG DE EVENTOS
   ═══════════════════════════════════════════════════════════════════════════ */
void logEvento(const char *msg)
{
    FILE *f = fopen("eventos.log", "a");
    if (!f) return;
    time_t ahora = time(NULL);
    char   buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&ahora));
    fprintf(f, "[%s] %s\n", buf, msg);
    fclose(f);
}