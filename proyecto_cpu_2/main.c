#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"
#include "master.h"

static int histDesp[100];
static int histCiclo[100];
static int histIdx       = 0;
static int iteracionesRR = 0;

typedef struct {
    Lista           *solicitudes;
    pthread_mutex_t *mutex;
    int             *terminado;
} ArgHiloCreacion;

void *hiloCreacionProcesos(void *arg)
{
    ArgHiloCreacion *a = (ArgHiloCreacion *)arg;
    for (int i = 0; i < TOTAL_PROCESOS && !(*a->terminado); i++) {
        Proceso *p = &tablaSistema.tablaBCPs[i];
        if (p->yaIngresado) continue;
        usleep((rand() % 50 + 1) * 1000);
        pthread_mutex_lock(a->mutex);
        if (asignarMemoriaBuddy(p, p->memoriaUsadaKB) >= 0) {
            asignarSlotMemoria(p);
            asignarPaginasProceso(p);
        }
        pthread_mutex_unlock(a->mutex);
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MANEJO DE TERMINAL
   ═══════════════════════════════════════════════════════════════════════════ */
static struct termios g_termOrig;

static void termSave(void)
{
    tcgetattr(STDIN_FILENO, &g_termOrig);
}

static void termRestore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termOrig);
    /* Restaurar fd a bloqueante */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

/*
 * termRaw: sin eco, sin buffering, NO bloqueante.
 * O_NONBLOCK en el fd es CLAVE para que getchar() no bloquee el loop.
 */
static void termRaw(void)
{
    struct termios t = g_termOrig;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN]  = 0;   /* no esperar ningún carácter */
    t.c_cc[VTIME] = 0;   /* sin timeout */
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    /* Poner fd en non-blocking */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

/*
 * termBlocking: restaura modo canónico bloqueante para leer input del usuario.
 * Se usa SOLO cuando necesitamos leer texto (quantum, ID de proceso).
 */
static void termBlocking(void)
{
    /* Primero quitar O_NONBLOCK del fd */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    /* Restaurar modo canónico con eco */
    struct termios t = g_termOrig;
    t.c_lflag |= (ICANON | ECHO);
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

/*
 * leerLineaSegura: cambia a modo bloqueante, lee la línea, limpia el buffer
 * y vuelve a raw+nonblocking.
 */
static int leerLineaSegura(char *buf, int maxlen)
{
    termBlocking();
    fflush(stdout);

    /* Limpiar cualquier basura pendiente en stdin */
    int c;
    while ((c = getchar()) != '\n' && c != EOF && c != '\0')
        ;   /* descartar basura del raw mode */

    /* Leer la línea real */
    if (fgets(buf, maxlen, stdin) == NULL) {
        buf[0] = '\0';
        termRaw();
        return 0;
    }

    /* Quitar newline */
    int len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';

    termRaw();
    return len;
}

int main(void)
{
    srand((unsigned)time(NULL));
    termSave();

    inicializarBuddy();
    inicializarTablaSistema();

    Lista enEjecucion, solicitudes;
    inicializarLista(&enEjecucion);
    inicializarLista(&solicitudes);

    cargarPalabras("libro1.txt");
    cargarFrases("frases.txt");
    inicializarMemoriaPrincipal();
    inicializarPaginacion();

    poblarListas(&enEjecucion, &solicitudes);

    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (asignarMemoriaBuddy(p, p->memoriaUsadaKB) < 0) continue;
        asignarSlotMemoria(p);
        asignarPaginasProceso(p);
        p->yaIngresado = 1;
    }

    Cola colaListos;
    inicializarCola(&colaListos);

    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (p->tiempoLlegada == 0) {
            encolar(&colaListos, p);
            p->estado = ESTADO_LISTO;
        }
    }

    SistemaES es;
    inicializarSistemaES(&es);

    int terminado       = 0;
    int reloj           = 0;
    int algoritmo       = ALG_FCFS;
    int quantum         = 20;
    int procesoPrivilId = -1;

    tablaSistema.algoritmoActual = ALG_FCFS;
    tablaSistema.quantumActual   = quantum;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    ArgHiloCreacion argCreacion = {&solicitudes, &mutex, &terminado};
    pthread_t thCreacion;
    pthread_create(&thCreacion, NULL, hiloCreacionProcesos, &argCreacion);

    vistaBienvenida();
    logEvento("Simulacion iniciada");

    /* Entrar en modo raw + non-blocking */
    termRaw();

    while (!terminado) {

        /* ── Leer tecla SIN BLOQUEAR ──────────────────────────────────────── */
        int c = getchar();   /* retorna EOF/−1 inmediatamente si no hay tecla */

        if (c != EOF && c != -1) {
            char tecla = (char)c;

            /* ── Q: salir ──────────────────────────────────────────────────── */
            if (tecla == 'q' || tecla == 'Q') {
                termRestore();
                printf("\n[Q] Terminando simulacion...\n");
                terminado = 1;
                break;
            }

            /* ── X: cambiar algoritmo ─────────────────────────────────────── */
            if (tecla == 'x' || tecla == 'X') {
                pthread_mutex_lock(&mutex);

                if (algoritmo == ALG_FCFS) {
                    char buf[64] = {0};
                    printf("\n[X] Cambiar a Round Robin\nIngrese Quantum (>0): ");
                    leerLineaSegura(buf, sizeof(buf));
                    int q = atoi(buf);
                    if (q <= 0) q = 20;
                    algoritmo = ALG_RR;
                    quantum   = q;
                    tablaSistema.algoritmoActual = ALG_RR;
                    tablaSistema.quantumActual   = q;
                    printf("[X] Algoritmo -> RR (Q=%d)\n", q);
                } else {
                    algoritmo = ALG_FCFS;
                    tablaSistema.algoritmoActual = ALG_FCFS;
                    printf("\n[X] Algoritmo -> FCFS\n");
                }
                logEvento("Cambio manual de algoritmo");
                pthread_mutex_unlock(&mutex);
            }

            /* ── A: apropiatividad ────────────────────────────────────────── */
            if (tecla == 'a' || tecla == 'A') {
                pthread_mutex_lock(&mutex);

                vistaMostrarMasRezagados(&colaListos);
                char idBuf[32] = {0};
                printf("Ingrese ID del proceso a privilegiar (ej: A-0): ");
                leerLineaSegura(idBuf, sizeof(idBuf));

                int encontrado = 0;
                for (int i = 0; i < TOTAL_PROCESOS; i++) {
                    Proceso *p = &tablaSistema.tablaBCPs[i];
                    if (strcmp(p->id, idBuf) == 0 &&
                        p->estado != ESTADO_TERMINADO) {
                        /* Quitar apropiatividad anterior */
                        if (procesoPrivilId >= 0)
                            tablaSistema.tablaBCPs[procesoPrivilId].esApropiativo = 0;
                        p->esApropiativo = 1;
                        procesoPrivilId  = i;
                        moverAlFrenteCola(&colaListos,   p);
                        moverAlFrenteCola(&es.disco,     p);
                        moverAlFrenteCola(&es.pantalla,  p);
                        moverAlFrenteCola(&es.teclado,   p);
                        moverAlFrenteCola(&es.impresora, p);
                        printf("[A] Proceso %s marcado como apropiativo\n", p->id);
                        logEvento("Proceso marcado como apropiativo");
                        encontrado = 1;
                        break;
                    }
                }
                if (!encontrado)
                    printf("[A] Proceso '%s' no encontrado o ya termino\n", idBuf);

                pthread_mutex_unlock(&mutex);
            }
        }

        /* ── Logica del simulador ─────────────────────────────────────────── */
        pthread_mutex_lock(&mutex);

        reloj++;
        resetarBitsR(reloj);

        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                if (p->esApropiativo) encolarAlFrente(&colaListos, p);
                else                  encolar(&colaListos, p);
                p->estado      = ESTADO_LISTO;
                p->yaIngresado = 1;
            }
        }

        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);
        actualizarEspera(&colaListos);

        procesarColaES(&es.disco,     &colaListos);
        procesarColaES(&es.pantalla,  &colaListos);
        procesarColaES(&es.teclado,   &colaListos);
        procesarColaES(&es.impresora, &colaListos);

        if (reloj % 300 == 0)
            redimensionarMemoriaPrincipal(&enEjecucion, reloj);

        if (reloj % 50 == 0) {
            int nuevoAlg = evaluarCambioAlgoritmo(&colaListos, &es);
            if (nuevoAlg != algoritmo) {
                algoritmo = nuevoAlg;
                tablaSistema.algoritmoActual = nuevoAlg;
                if (nuevoAlg == ALG_RR && quantum <= 0) {
                    quantum = 20;
                    tablaSistema.quantumActual = quantum;
                }
            }
        }

        if (!estaVaciaCola(&colaListos)) {
            if (algoritmo == ALG_FCFS)
                ejecutarFCFS(&colaListos, &es, &procesoPrivilId, reloj);
            else
                ejecutarRR(&colaListos, &es, &procesoPrivilId,
                           &quantum, &iteracionesRR,
                           histDesp, histCiclo, &histIdx, reloj);
        }

        calcularDesperdicioExterno();
        actualizarPromedioFinalizados(reloj);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes,
                                    &colaListos, &es, reloj);

        if (reloj % 100 == 0) {
            ejecutarMasterPVM(&enEjecucion, quantum);
            vistaMostrarTablaGlobal();
            vistaEstadoES(&es);
            mostrarEstadisticasMemoria();
            if (algoritmo == ALG_RR) {
                vistaBarrasAprovechamiento(&colaListos,
                                           histDesp, histCiclo, histIdx);
                mostrarEnvejecimiento(&colaListos);
                mostrarDesperdiciadores(&colaListos);
            }
            guardarBCPs(&enEjecucion, "bcps.log");
            guardarVariablesGlobales("variables.log");
            logEvento("Checkpoint");
        }

        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != ESTADO_TERMINADO) activos++;
        if (activos == 0 && solicitudes.tamanio == 0 &&
            estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&mutex);
        usleep(1000);
    }

    termRestore();
    terminado = 1;
    pthread_join(thCreacion, NULL);
    pthread_mutex_destroy(&mutex);

    guardarBCPs(&enEjecucion, "bcps.log");
    guardarVariablesGlobales("variables.log");
    logEvento("Simulacion finalizada");
    mostrarEstadisticasMemoria();
    vistaCierre(reloj, tablaSistema.procesosTerminados);
    return 0;
}