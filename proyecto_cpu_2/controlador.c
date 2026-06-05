#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "controlador.h"
#include "vista.h"

/* ─── Detección de tecla sin bloqueo ───────────────────────────────────────── */
static int hayTecla(void)
{
    struct termios oldt, newt;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    int c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (c != EOF) { ungetc(c, stdin); return 1; }
    return 0;
}

static char leerTecla(void)
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}

/* ─── Manejo de entrada ─────────────────────────────────────────────────────── */
void manejarEntrada(ContextoHilos *ctx)
{
    if (!hayTecla()) return;
    char tecla = leerTecla();

    /* ── Q: salir ──────────────────────────────────────────────────────────── */
    if (tecla == 'q' || tecla == 'Q') {
        printf("\n[Q] Terminando simulacion por solicitud del usuario...\n");
        *ctx->terminado = 1;
        return;
    }

    /* ── X: cambiar algoritmo ──────────────────────────────────────────────── */
    if (tecla == 'x' || tecla == 'X') {
        if (*ctx->algoritmo == ALG_FCFS) {
            printf("\n[X] Cambiar a Round Robin\nIngrese Quantum: ");
            fflush(stdout);
            int q = 0;
            scanf("%d", &q);
            if (q <= 0) q = 20;
            *ctx->algoritmo              = ALG_RR;
            *ctx->quantum                = q;
            tablaSistema.algoritmoActual = ALG_RR;
            tablaSistema.quantumActual   = q;
            printf("[X] Algoritmo -> RR (Q=%d)\n", q);
        } else {
            *ctx->algoritmo              = ALG_FCFS;
            tablaSistema.algoritmoActual = ALG_FCFS;
            printf("\n[X] Algoritmo -> FCFS\n");
        }
        logEvento("Cambio manual de algoritmo");
    }

    /* ── A: apropiatividad ─────────────────────────────────────────────────── */
    if (tecla == 'a' || tecla == 'A') {
        vistaMostrarMasRezagados(ctx->colaListos);
        printf("Ingrese ID del proceso a privilegiar (ej: A-0): ");
        fflush(stdout);
        char idBuf[32];
        scanf("%31s", idBuf);

        int encontrado = 0;
        for (int i = 0; i < TOTAL_PROCESOS; i++) {
            Proceso *p = &tablaSistema.tablaBCPs[i];
            if (strcmp(p->id, idBuf) == 0 && p->estado != ESTADO_TERMINADO) {
                if (*ctx->procesoPrivilId >= 0)
                    tablaSistema.tablaBCPs[*ctx->procesoPrivilId].esApropiativo = 0;
                p->esApropiativo      = 1;
                *ctx->procesoPrivilId = i;

                moverAlFrenteCola(ctx->colaListos, p);
                moverAlFrenteCola(&ctx->es->disco,     p);
                moverAlFrenteCola(&ctx->es->pantalla,  p);
                moverAlFrenteCola(&ctx->es->teclado,   p);
                moverAlFrenteCola(&ctx->es->impresora, p);

                printf("[A] Proceso %s marcado como apropiativo\n", p->id);
                encontrado = 1;
                logEvento("Proceso marcado como apropiativo");
                break;
            }
        }
        if (!encontrado)
            printf("[A] Proceso '%s' no encontrado o ya termino\n", idBuf);
    }
}

/* ─── Helper: proceso apropiativo siempre al frente ───────────────────────── */
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
   FCFS  —  CORREGIDO
   La terminación se verifica ANTES de ejecutar y también DESPUÉS.
   ═══════════════════════════════════════════════════════════════════════════ */
void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    ponerApropiatvioAlFrente(colaListos);

    Proceso *p = desencolar(colaListos);
    if (!p) return;

    /* ─ Si ya terminó (ciclosRestantes == 0 al llegar) terminar de inmediato ─ */
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
        /* Terminó en esta ráfaga */
        if (p->esApropiativo) {
            printf("[FCFS] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
        procesarTerminacion(p, reloj);
        printf("[FCFS] %s TERMINO (ciclos=%d)\n", p->id, p->ciclosTotales);
    } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
        asignarES(p, es);
    } else {
        p->estado = ESTADO_LISTO;
        if (p->esApropiativo)
            encolarAlFrente(colaListos, p);
        else
            encolar(colaListos, p);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   ROUND ROBIN  —  CORREGIDO
   ═══════════════════════════════════════════════════════════════════════════ */
void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
                int *quantum, int *iteracionesRR,
                int histDesp[], int histCiclo[], int *histIdx, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    ponerApropiatvioAlFrente(colaListos);

    Proceso *p = desencolar(colaListos);
    if (!p) return;

    /* ─ Si ya terminó al llegar ─────────────────────────────────────────── */
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
        /* Terminó en esta ráfaga */
        if (p->esApropiativo) {
            printf("[RR] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
        procesarTerminacion(p, reloj);
        printf("[RR] %s TERMINO (ciclos=%d)\n", p->id, p->ciclosTotales);
    } else if (p->rafagaActual >= q) {
        /* Ráfaga superó o igualó quantum: se interrumpe, vuelve a cola */
        p->estado = ESTADO_LISTO;
        p->restanteQuantum = p->rafagaActual - ejecuta;
        if (p->esApropiativo)
            encolarAlFrente(colaListos, p);
        else
            encolar(colaListos, p);
    } else {
        /* Usó menos que el quantum: va a E/S */
        asignarES(p, es);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   HILO E/S  —  CORREGIDO
   El tick de E/S ahora descuenta un valor proporcional al tiempo real
   transcurrido, en lugar de decrementar de 1 en 1 por semáforo.
   Cada vez que el semáforo se dispara (cada ~50ms del hilo reloj) se
   descuentan TICKS_POR_CICLO_ES ciclos de E/S, lo que hace que los procesos
   salgan de E/S en tiempo razonable.
   ═══════════════════════════════════════════════════════════════════════════ */
#define TICKS_ES 10  /* cuántos ciclos de E/S se consumen por cada tick del reloj */

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
            if (p->esApropiativo)
                encolarAlFrente(colaListos, p);
            else
                encolar(colaListos, p);
        } else {
            encolar(colaES, p);
        }
    }
}

void *hiloDispositivoES(void *arg)
{
    ArgHiloES *a = (ArgHiloES *)arg;
    while (1) {
        sem_wait(a->sem);
        if (*a->terminado) break;
        pthread_mutex_lock(a->mutex);
        procesarColaES(a->colaES, a->colaListos);
        pthread_mutex_unlock(a->mutex);
    }
    return NULL;
}

void *hiloReloj(void *arg)
{
    ContextoHilos *ctx = (ContextoHilos *)arg;
    while (!(*ctx->terminado)) {
        usleep(50000);   /* 50 ms */
        pthread_mutex_lock(&ctx->mutexPrincipal);
        sem_post(&ctx->semDisco);
        sem_post(&ctx->semPantalla);
        sem_post(&ctx->semTeclado);
        sem_post(&ctx->semImpresora);
        pthread_mutex_unlock(&ctx->mutexPrincipal);
    }
    return NULL;
}

/* ─── Ajuste automático de quantum (cada 20 iteraciones RR) ─────────────── */
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
    int q = tablaSistema.quantumActual;
    if (propListos > 0.75f) {
        q = (q > 5) ? q - 5 : q;
        printf("[RR] Desbalance listos>75%% -> Q reducido a %d\n", q);
    } else if (propES > 0.75f) {
        q = (q < 100) ? q + 5 : q;
        printf("[RR] Desbalance ES>75%% -> Q aumentado a %d\n", q);
    } else if (propListos > 0.4f && propListos < 0.6f) {
        printf("[RR] Colas balanceadas (listos=%.0f%%, ES=%.0f%%)\n",
               propListos * 100, propES * 100);
    }
    tablaSistema.quantumActual = q;
}

/* ─── Envejecimiento: top 5 con más veces en CPU ────────────────────────── */
void mostrarEnvejecimiento(Cola *colaListos)
{
    printf("\n--- TOP 5 ENVEJECIMIENTO ---\n");
    Proceso *top[5] = {NULL};
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        for (int i = 0; i < 5; i++) {
            if (!top[i] || p->vecesEnCPU > top[i]->vecesEnCPU) {
                for (int j = 4; j > i; j--) top[j] = top[j - 1];
                top[i] = p; break;
            }
        }
    }
    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | vecesEnCPU:%3d | ciclosRest:%6d | espera:%d\n",
               i + 1, top[i]->id, top[i]->vecesEnCPU,
               top[i]->ciclosRestantes, top[i]->tiempoEspera);
    if (!top[0]) printf("  (cola vacia)\n");
}

/* ─── Desperdiciadores: top 5 con mayor (quantum − ráfaga) ─────────────── */
void mostrarDesperdiciadores(Cola *colaListos)
{
    printf("\n--- TOP 5 DESPERDICIADORES ---\n");
    Proceso *top[5] = {NULL};
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        int desp = tablaSistema.quantumActual - p->rafagaActual;
        if (desp < 0) desp = 0;
        for (int i = 0; i < 5; i++) {
            if (!top[i]) { top[i] = p; break; }
            int despTop = tablaSistema.quantumActual - top[i]->rafagaActual;
            if (despTop < 0) despTop = 0;
            if (desp > despTop) {
                for (int j = 4; j > i; j--) top[j] = top[j - 1];
                top[i] = p; break;
            }
        }
    }
    for (int i = 0; i < 5 && top[i]; i++) {
        int desp = tablaSistema.quantumActual - top[i]->rafagaActual;
        if (desp < 0) desp = 0;
        printf("  %d. %-8s | rafaga:%3d | Q:%3d | desperdicio:%3d\n",
               i + 1, top[i]->id, top[i]->rafagaActual,
               tablaSistema.quantumActual, desp);
    }
    if (!top[0]) printf("  (cola vacia)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   PERSISTENCIA
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
        fprintf(f, " FallosPag    : %d\n",   p->fallosPagina);
        fprintf(f, " NumMarcos    : %d\n",   p->numMarcos);
        fprintf(f, " NumPaginas   : %d\n",   p->numPaginas);
        fprintf(f, " MemKB        : %d\n\n", p->bloqueMemoriaKB);
    }
    fclose(f);
}

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

void logEvento(const char *msg)
{
    FILE *f = fopen("eventos.log", "a");
    if (!f) return;
    time_t ahora = time(NULL);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&ahora));
    fprintf(f, "[%s] %s\n", buf, msg);
    fclose(f);
}