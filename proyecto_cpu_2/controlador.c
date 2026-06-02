#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "controlador.h"
#include "vista.h"

static struct termios termOriginal;
static int termConfigurada = 0;

static void configurarTerminalRaw(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &termOriginal);
    raw = termOriginal;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    termConfigurada = 1;
}

static void restaurarTerminal(void)
{
    if (termConfigurada)
        tcsetattr(STDIN_FILENO, TCSANOW, &termOriginal);
}

static void leerLineaConsola(char *buf, int max)
{
    // Restaurar terminal para que scanf/fgets funcione normal
    restaurarTerminal();
    fflush(stdout);
    if (fgets(buf, max, stdin) != NULL) {
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    }
    // Volver a raw
    configurarTerminalRaw();
}

// ─── FCFS ────────────────────────────────────────────────────────────────────
// El proceso toma su rafaga completa sin limite de tiempo
void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId)
{
    if (estaVaciaCola(colaListos)) return;

    Proceso *p = desencolar(colaListos);
    procesarEntradaCPU(p);
    tablaSistema.totalCambiosContexto++;

    if (p->ciclosRestantes <= 0) {
        procesarTerminacion(p);
        if (*procesoPrivilId == (int)(p - tablaSistema.tablaBCPs)) {
            printf("[FCFS] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
    } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
        // Va a E/S: si es apropiativo igual queda en frente cuando vuelva
        asignarES(p, es);
    } else {
        p->estado = ESTADO_LISTO;
        // Apropiativo siempre al frente, en cualquier cola
        if (p->esApropiativo)
            encolarAlFrente(colaListos, p);
        else
            encolar(colaListos, p);
    }
}

// ─── ROUND ROBIN ─────────────────────────────────────────────────────────────
// Si rafagaActual > quantum: proceso interrumpido, vuelve al final (o frente si es apropiativo)
// Si rafagaActual <= quantum: termino su turno, va a E/S o vuelve a cola
void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
                int *quantum, int *iteracionesRR,
                int histDesp[], int histCiclo[], int *histIdx, int reloj)
{
    if (estaVaciaCola(colaListos)) return;

    Proceso *p = desencolar(colaListos);
    procesarEntradaCPU(p);
    tablaSistema.totalCambiosContexto++;
    (*iteracionesRR)++;

    int q     = tablaSistema.quantumActual;
    int usado = (p->rafagaActual < q) ? p->rafagaActual : q;
    int desp  = q - usado;

    p->aprovechamiento = (usado * 100) / (q ? q : 1);
    p->desperdicio     = desp;

    histDesp[*histIdx]  = desp * 100 / (q ? q : 1);
    histCiclo[*histIdx] = reloj;
    *histIdx = (*histIdx + 1) % 100;

    ajustarQuantumAutomatico(colaListos, es, *iteracionesRR);
    *quantum = tablaSistema.quantumActual;

    if (p->ciclosRestantes <= 0) {
        procesarTerminacion(p);
        if (*procesoPrivilId == (int)(p - tablaSistema.tablaBCPs)) {
            printf("[RR] Proceso %s termino (era apropiativo)\n", p->id);
            *procesoPrivilId = -1;
        }
    } else if (p->rafagaActual > q) {
        // Quantum insuficiente: proceso interrumpido, regresa a cola
        p->estado = ESTADO_LISTO;
        if (p->esApropiativo)
            encolarAlFrente(colaListos, p);
        else
            encolar(colaListos, p);
    } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
        // Uso menos que el quantum, es ES-bound: va a E/S
        asignarES(p, es);
    } else {
        p->estado = ESTADO_LISTO;
        if (p->esApropiativo)
            encolarAlFrente(colaListos, p);
        else
            encolar(colaListos, p);
    }
}

// ─── HILO E/S ────────────────────────────────────────────────────────────────
// Cuando un proceso termina su tiempo de E/S vuelve a colaListos
// Si es apropiativo siempre va al frente
void procesarColaES(Cola *colaES, Cola *colaListos)
{
    int n = colaES->tamanio;
    while (n--) {
        Proceso *p = desencolar(colaES);
        if (!p) continue;
        p->tiempoES--;
        if (p->tiempoES <= 0) {
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

// ─── HILO RELOJ ──────────────────────────────────────────────────────────────
void *hiloReloj(void *arg)
{
    ContextoHilos *ctx = (ContextoHilos *)arg;
    while (!(*ctx->terminado)) {
        usleep(50000);
        pthread_mutex_lock(&ctx->mutexPrincipal);
        sem_post(&ctx->semDisco);
        sem_post(&ctx->semPantalla);
        sem_post(&ctx->semTeclado);
        sem_post(&ctx->semImpresora);
        pthread_mutex_unlock(&ctx->mutexPrincipal);
    }
    return NULL;
}

// ─── HILO ENTRADA ────────────────────────────────────────────────────────────
// Terminal en modo raw: detecta tecla sin necesitar Enter
// X: toggle FCFS <-> RR inmediato
// A: pedir ID y marcar proceso como apropiativo (solo en RR)
void *hiloEntrada(void *arg)
{
    ContextoHilos *ctx = (ContextoHilos *)arg;
    configurarTerminalRaw();

    while (!(*ctx->terminado)) {
        int c = getchar();
        if (c == EOF || c == -1) { usleep(20000); continue; }

        switch (c) {

        case 'x': case 'X':
            {
                pthread_mutex_lock(&ctx->mutexPrincipal);
                int actual = *ctx->algoritmo;
                if (actual == ALG_FCFS) {
                    // Cambiar a RR: pedir quantum
                    pthread_mutex_unlock(&ctx->mutexPrincipal);
                    restaurarTerminal();
                    printf("\n[X] Cambiando a Round Robin\n");
                    printf("Ingrese Quantum: ");
                    fflush(stdout);
                    char buf[32] = {0};
                    leerLineaConsola(buf, sizeof(buf));
                    int q = atoi(buf);
                    if (q <= 0) q = 20;
                    pthread_mutex_lock(&ctx->mutexPrincipal);
                    *ctx->algoritmo              = ALG_RR;
                    *ctx->quantum                = q;
                    tablaSistema.algoritmoActual = ALG_RR;
                    tablaSistema.quantumActual   = q;
                    printf("[X] Algoritmo cambiado a RR (Q=%d)\n", q);
                    pthread_mutex_unlock(&ctx->mutexPrincipal);
                } else {
                    // Cambiar a FCFS directamente sin preguntar
                    *ctx->algoritmo              = ALG_FCFS;
                    tablaSistema.algoritmoActual = ALG_FCFS;
                    printf("\n[X] Algoritmo cambiado a FCFS\n");
                    pthread_mutex_unlock(&ctx->mutexPrincipal);
                }
            }
            break;

        case 'a': case 'A':
            {
                pthread_mutex_lock(&ctx->mutexPrincipal);
                if (*ctx->algoritmo != ALG_RR) {
                    printf("\n[A] Apropiatividad solo disponible en Round Robin\n");
                    pthread_mutex_unlock(&ctx->mutexPrincipal);
                    break;
                }
                // Mostrar los 5 mas rezagados para que el usuario elija
                vistaMostrarMasRezagados(ctx->colaListos);
                pthread_mutex_unlock(&ctx->mutexPrincipal);

                restaurarTerminal();
                printf("Ingrese ID del proceso a privilegiar (ej: A-0): ");
                fflush(stdout);
                char idBuf[32] = {0};
                leerLineaConsola(idBuf, sizeof(idBuf));
                // leerLineaConsola ya devolvio a raw

                pthread_mutex_lock(&ctx->mutexPrincipal);
                int encontrado = 0;
                for (int i = 0; i < TOTAL_PROCESOS; i++) {
                    Proceso *p = &tablaSistema.tablaBCPs[i];
                    if (strcmp(p->id, idBuf) == 0 && p->estado != ESTADO_TERMINADO) {
                        // Quitar apropiatividad al anterior
                        if (*ctx->procesoPrivilId >= 0)
                            tablaSistema.tablaBCPs[*ctx->procesoPrivilId].esApropiativo = 0;
                        p->esApropiativo     = 1;
                        *ctx->procesoPrivilId = i;
                        printf("[A] Proceso %s marcado como apropiativo\n", p->id);
                        encontrado = 1;
                        break;
                    }
                }
                if (!encontrado)
                    printf("[A] Proceso '%s' no encontrado o ya termino\n", idBuf);
                pthread_mutex_unlock(&ctx->mutexPrincipal);
            }
            break;

        case 'q': case 'Q':
            pthread_mutex_lock(&ctx->mutexPrincipal);
            *ctx->terminado = 1;
            pthread_mutex_unlock(&ctx->mutexPrincipal);
            break;

        default:
            break;
        }
    }

    restaurarTerminal();
    return NULL;
}

// ─── AJUSTE AUTOMATICO DE QUANTUM ────────────────────────────────────────────
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

// ─── ENVEJECIMIENTO Y DESPERDICIADORES ───────────────────────────────────────
void mostrarEnvejecimiento(Cola *colaListos)
{
    printf("\n--- TOP 5 ENVEJECIMIENTO ---\n");
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
        printf("  %d. %-8s | vecesEnCPU:%3d | ciclosRest:%6d | espera:%d\n",
               i+1, top[i]->id, top[i]->vecesEnCPU,
               top[i]->ciclosRestantes, top[i]->tiempoEspera);
    if (!top[0]) printf("  (cola vacia)\n");
}

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
                for (int j = 4; j > i; j--) top[j] = top[j-1];
                top[i] = p; break;
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

// ─── PERSISTENCIA ────────────────────────────────────────────────────────────
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