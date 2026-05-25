#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "controlador.h"
#include "vista.h"

// ─────────────────────────────────────────────────────────────────────────────
// HILO E/S — procesa un tick de cada cola de dispositivo
// ─────────────────────────────────────────────────────────────────────────────

void procesarColaES(Cola *colaES, Cola *colaListos)
{
    int n = colaES->tamanio;
    while (n--) {
        Proceso *p = desencolar(colaES);
        if (!p) continue;
        p->tiempoES--;
        if (p->tiempoES <= 0) {
            p->bloqueado     = 0;
            p->dispositivoES = -1;
            p->estado        = 0;
            encolar(colaListos, p);
            printf("  [ES] %s termino E/S -> colaListos\n", p->id);
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

// ─────────────────────────────────────────────────────────────────────────────
// HILO RELOJ — tick cada 50ms, despierta los hilos E/S
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// HILO ENTRADA — escucha teclado sin bloquear el loop principal
// ─────────────────────────────────────────────────────────────────────────────

void *hiloEntrada(void *arg)
{
    ContextoHilos *ctx = (ContextoHilos *)arg;
    while (!(*ctx->terminado)) {
        int c = getchar();
        if (c == EOF) { usleep(30000); continue; }
        pthread_mutex_lock(&ctx->mutexPrincipal);
        switch (c) {
            case 's': case 'S': vistaMostrarTablaGlobal();                    break;
            case 'b': case 'B': vistaMostrarBuddy();                          break;
            case 'e': case 'E': vistaEstadoES(ctx->es);                       break;
            case 'l': case 'L': vistaMostrarLista(ctx->procesosEnEjecucion,
                                                  "procesosEnEjecucion");     break;
            case 'q': case 'Q': *ctx->terminado = 1;                          break;
            default: break;
        }
        pthread_mutex_unlock(&ctx->mutexPrincipal);
    }
    return NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// PERSISTENCIA
// ─────────────────────────────────────────────────────────────────────────────

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
        fprintf(f, " TiempoEspera : %d\n",   p->tiempoEspera);
        fprintf(f, " Estado       : %d\n",   p->estado);
        fprintf(f, " Mem KB       : %d\n\n", p->bloqueMemoriaKB);
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
    fprintf(f, " 9.  Algoritmo        : %d\n", t->algoritmoActual);
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