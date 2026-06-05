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

/* Lee una tecla sin bloquear. Devuelve 1 y escribe en *out si hay tecla. */
static int teclaDisponible(char *out)
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN]  = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (c != EOF) { *out = (char)c; return 1; }
    return 0;
}

/* Pone el terminal en modo normal (canónico) para leer con scanf */
static void modoNormal(void)
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO);
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

int main(void)
{
    srand((unsigned)time(NULL));

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

    while (!terminado) {

        /* ── PASO 1: leer tecla SIN mutex ─────────────────────────────────── */
        char tecla = 0;
        int  hayTecla = teclaDisponible(&tecla);

        /* ── PASO 2: si hay tecla que necesita input del usuario,
               procesarla ANTES de tomar el mutex                   ───────── */
        if (hayTecla) {
            if (tecla == 'q' || tecla == 'Q') {
                printf("\n[Q] Terminando simulacion...\n");
                terminado = 1;
                break;
            }

            if (tecla == 'x' || tecla == 'X') {
                /* Cambio de algoritmo: necesita scanf → modo normal ANTES del mutex */
                modoNormal();
                int nuevoAlg, nuevoQ = quantum;
                if (algoritmo == ALG_FCFS) {
                    printf("\n[X] Cambiar a Round Robin\nIngrese Quantum (>0): ");
                    fflush(stdout);
                    if (scanf("%d", &nuevoQ) != 1 || nuevoQ <= 0) nuevoQ = 20;
                    nuevoAlg = ALG_RR;
                } else {
                    nuevoAlg = ALG_FCFS;
                }
                /* Ahora sí tomamos el mutex para actualizar estado */
                pthread_mutex_lock(&mutex);
                algoritmo = nuevoAlg;
                quantum   = nuevoQ;
                tablaSistema.algoritmoActual = nuevoAlg;
                tablaSistema.quantumActual   = nuevoQ;
                if (nuevoAlg == ALG_RR)
                    printf("[X] Algoritmo -> RR (Q=%d)\n", nuevoQ);
                else
                    printf("\n[X] Algoritmo -> FCFS\n");
                logEvento("Cambio manual de algoritmo");
                pthread_mutex_unlock(&mutex);
            }

            if (tecla == 'a' || tecla == 'A') {
                modoNormal();
                /* Mostrar rezagados no necesita mutex (solo lectura rápida) */
                vistaMostrarMasRezagados(&colaListos);
                printf("Ingrese ID del proceso a privilegiar (ej: A-0): ");
                fflush(stdout);
                char idBuf[32] = {0};
                if (scanf("%31s", idBuf) == 1) {
                    pthread_mutex_lock(&mutex);
                    int encontrado = 0;
                    for (int i = 0; i < TOTAL_PROCESOS; i++) {
                        Proceso *p = &tablaSistema.tablaBCPs[i];
                        if (strcmp(p->id, idBuf) == 0 &&
                            p->estado != ESTADO_TERMINADO) {
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
                        printf("[A] Proceso '%s' no encontrado\n", idBuf);
                    pthread_mutex_unlock(&mutex);
                }
            }
        }

        /* ── PASO 3: lógica del simulador ─────────────────────────────────── */
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