#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "controlador.h"
#include "vista.h"

/* Ciclos acumulados antes de solicitar E/S */
#define CICLOS_PARA_ES 100
/* Ticks que avanza cada proceso en E/S por ciclo de simulacion */
#define TICKS_ES       10

/* ── Poner proceso apropiativo al frente ─── */
static void ponerApropiativoAlFrente(Cola *colaListos)
{
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        if (n->proceso->esApropiativo) {
            moverAlFrenteCola(colaListos, n->proceso);
            break;
        }
    }
}

/* ── Lógica post-ejecución compartida por FCFS y RR ─────────────────────
   Decide si el proceso: termina, va a E/S, o vuelve a listos.
   En RR, ciclosEjecutados puede ser < rafagaActual (preempción por quantum).  */
static void postEjecucion(Proceso *p, Cola *colaListos, SistemaES *es,
                          int *procesoPrivilId, int reloj, int ciclosEjecutados)
{
    if (p->ciclosRestantes <= 0) {
        /* TERMINÓ */
        if (p->esApropiativo) *procesoPrivilId = -1;
        procesarTerminacion(p, reloj);
    } else if (p->ciclosEnEjecucion >= CICLOS_PARA_ES) {
        /* Acumuló 200 ciclos → E/S */
        asignarES(p, es, reloj);
    } else {
        /* Vuelve a cola de listos */
        p->estado = ESTADO_LISTO;
        if (p->esApropiativo) encolarAlFrente(colaListos, p);
        else                  encolar(colaListos, p);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   FCFS — no hay preempción, el proceso ejecuta su ráfaga completa
   ═══════════════════════════════════════════════════════════════════════════ */
void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    ponerApropiativoAlFrente(colaListos);
    Proceso *p = desencolar(colaListos);
    if (!p) return;

    /* Si llegó con ciclos=0 (caso borde) terminar directo */
    if (p->ciclosRestantes <= 0) {
        if (p->esApropiativo) *procesoPrivilId = -1;
        procesarTerminacion(p, reloj);
        tablaSistema.totalCambiosContexto++;
        return;
    }

    procesarEntradaCPU(p, reloj);          /* descuenta ráfaga completa */
    tablaSistema.totalCambiosContexto++;

    postEjecucion(p, colaListos, es, procesoPrivilId, reloj, p->rafagaActual);
}

/* ═══════════════════════════════════════════════════════════════════════════
   ROUND ROBIN
   - Si rafagaActual > quantum → preempción: solo ejecuta `quantum` ciclos,
     se devuelven los sobrantes, desperdicio = 0 (usó todo el quantum).
   - Si rafagaActual <= quantum → terminó su ráfaga antes del quantum,
     desperdicio = quantum - rafagaActual.
   ═══════════════════════════════════════════════════════════════════════════ */
void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
                int *quantum, int *iteracionesRR,
                int histDesp[], int histCiclo[], int *histIdx, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    ponerApropiativoAlFrente(colaListos);
    Proceso *p = desencolar(colaListos);
    if (!p) return;

    if (p->ciclosRestantes <= 0) {
        if (p->esApropiativo) *procesoPrivilId = -1;
        procesarTerminacion(p, reloj);
        tablaSistema.totalCambiosContexto++;
        return;
    }

    procesarEntradaCPU(p, reloj);   /* descuenta rafagaActual completa */
    tablaSistema.totalCambiosContexto++;
    (*iteracionesRR)++;

    int q = tablaSistema.quantumActual;
    int ejecuta, desp;

    if (p->rafagaActual > q) {
        /* PREEMPCIÓN: el proceso quería más que el quantum.
           procesarEntradaCPU ya descontó rafagaActual; devolvemos el sobrante. */
        int sobrante = p->rafagaActual - q;
        p->ciclosRestantes   += sobrante;
        p->tiempoEjecucion   -= sobrante;
        p->ciclosEnEjecucion -= sobrante;
        ejecuta = q;
        desp    = 0;            /* usó todo el quantum: no hay desperdicio */
    } else {
        /* Terminó ráfaga antes del quantum → hay desperdicio de CPU */
        ejecuta = p->rafagaActual;
        desp    = q - ejecuta;
    }

    p->aprovechamiento = (q > 0) ? (ejecuta * 100) / q : 0;
    p->desperdicio     = desp;

    /* Guardar en historial (porcentaje de desperdicio) */
    histDesp[*histIdx]  = (q > 0) ? (desp * 100) / q : 0;
    histCiclo[*histIdx] = reloj;
    *histIdx = (*histIdx + 1) % 100;

    /* Ajuste automático de quantum cada 20 iteraciones */
    ajustarQuantumAutomatico(colaListos, es, *iteracionesRR);
    *quantum = tablaSistema.quantumActual;

    postEjecucion(p, colaListos, es, procesoPrivilId, reloj, ejecuta);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PROCESAMIENTO DE COLA E/S
   Cada ciclo se descuentan TICKS_ES por proceso.
   Al terminar su tiempo de E/S, el proceso vuelve a la cola de listos.
   ═══════════════════════════════════════════════════════════════════════════ */
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
   AJUSTE AUTOMÁTICO DE QUANTUM (cada 20 iteraciones de RR)
   Si listos > 75% → reducir Q (menos espera por proceso)
   Si E/S   > 75% → aumentar Q (menos preempciones)
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
        q = (q > 20) ? q - 5 : q; 
        printf("[RR] Desbalance listos>75%% -> Q reducido a %d\n", q);
    } else if (propES > 0.75f) {
        q = (q < 100) ? q + 5 : q;
        printf("[RR] Desbalance ES>75%% -> Q aumentado a %d\n", q);
    } else {
        printf("[RR] Colas balanceadas (listos=%.0f%% ES=%.0f%%) Q=%d\n",
               propListos * 100, propES * 100, q);
    }
    tablaSistema.quantumActual = q;
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOP 5 ENVEJECIMIENTO — los que más veces han pasado por CPU
   (los que el quantum les queda pequeño y necesitan más iteraciones)
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
                top[i] = p; break;
            }
        }
    }
    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | vecesEnCPU:%3d | ciclosRest:%5d | espera:%d\n",
               i+1, top[i]->id, top[i]->vecesEnCPU,
               top[i]->ciclosRestantes, top[i]->tiempoEspera);
    if (!top[0]) printf("  (cola vacia)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOP 5 DESPERDICIADORES — los que más quantum dejan sin usar
   (rafagaActual < quantum → desp = quantum - rafagaActual)
   ═══════════════════════════════════════════════════════════════════════════ */
void mostrarDesperdiciadores(Cola *colaListos)
{
    printf("\n--- TOP 5 DESPERDICIADORES DE CPU ---\n");
    Proceso *top[5] = {NULL};
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        /* El desperdicio real de este proceso en su última ejecución */
        int desp = p->desperdicio;
        for (int i = 0; i < 5; i++) {
            if (!top[i]) { top[i] = p; break; }
            if (desp > top[i]->desperdicio) {
                for (int j = 4; j > i; j--) top[j] = top[j-1];
                top[i] = p; break;
            }
        }
    }
    int q = tablaSistema.quantumActual;
    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | rafaga:%3d | Q:%3d | desp:%3d | aprov:%3d%%\n",
               i+1, top[i]->id, top[i]->rafagaActual,
               q, top[i]->desperdicio, top[i]->aprovechamiento);
    if (!top[0]) printf("  (cola vacia)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   PERSISTENCIA — BCP y variables globales
   ═══════════════════════════════════════════════════════════════════════════ */
void guardarBCPs(Lista *enEjecucion, const char *ruta)
{
    FILE *f = fopen(ruta, "w");
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
        fprintf(f, " Aprovech%%    : %d\n",   p->aprovechamiento);
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

void guardarVariablesGlobales(const char *ruta)
{
    FILE *f = fopen(ruta, "w");
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
    fprintf(f, " 9.  Algoritmo        : %s\n", t->algoritmoActual == ALG_FCFS ? "FCFS" : "RR");
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