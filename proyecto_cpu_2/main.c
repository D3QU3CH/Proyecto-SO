#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"
#include "master.h"

/* ═══════════════════════════════════════════════════════════════════════════
   ESTADO GLOBAL DE TERMINAL
   ═══════════════════════════════════════════════════════════════════════════ */
static struct termios g_termOrig;
static int            g_termSaved = 0;

static void termSave(void)
{
    tcgetattr(STDIN_FILENO, &g_termOrig);
    g_termSaved = 1;
}

static void termRestore(void)
{
    if (!g_termSaved) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termOrig);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
}

/*
 * termRaw — modo raw + non-blocking.
 * Se mantiene ISIG activo para que Ctrl+C funcione.
 */
static void termRaw(void)
{
    struct termios t = g_termOrig;
    t.c_lflag &= ~(ICANON | ECHO);   /* NO se quita ISIG */
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f | O_NONBLOCK);
}

/* Modo canónico bloqueante con eco para leer texto del usuario */
static void termBlocking(void)
{
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
    struct termios t = g_termOrig;
    t.c_lflag |= (ICANON | ECHO);
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

/* Retorna el carácter leído o 0 si no había nada (sin bloqueo) */
static char leerTeclaSinBloquear(void)
{
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

/*
 * leerLineaSegura — cambia a bloqueante, lee byte a byte con eco manual,
 * vuelve a raw al terminar.
 */
static int leerLineaSegura(char *buf, int maxlen)
{
    termBlocking();
    fflush(stdout);
    int len = 0;
    while (len < maxlen - 1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;
        if (c == '\n' || c == '\r') break;
        if (c == 127 || c == '\b') {
            if (len > 0) { len--; write(STDOUT_FILENO, "\b \b", 3); }
            continue;
        }
        buf[len++] = c;
        write(STDOUT_FILENO, &c, 1);
    }
    buf[len] = '\0';
    write(STDOUT_FILENO, "\n", 1);
    termRaw();
    return len;
}

/* ── Handler de señal para restaurar terminal ante SIGINT/SIGTERM ─────────── */
static volatile sig_atomic_t g_terminado = 0;

static void manejadorSenhal(int sig)
{
    (void)sig;
    g_terminado = 1;
    termRestore();
    /* No llamamos exit() aquí: dejamos que el bucle principal lo detecte */
}

/* ═══════════════════════════════════════════════════════════════════════════
   HILO DE CREACIÓN DE PROCESOS (solicitudes)
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    Lista           *solicitudes;
    pthread_mutex_t *mutex;
    volatile int    *terminado;
} ArgHiloCreacion;

static void *hiloCreacionProcesos(void *arg)
{
    ArgHiloCreacion *a = (ArgHiloCreacion *)arg;
    for (int i = 0; i < TOTAL_PROCESOS && !(*a->terminado); i++) {
        Proceso *p = &tablaSistema.tablaBCPs[i];

        pthread_mutex_lock(a->mutex);
        if (p->yaIngresado) {
            pthread_mutex_unlock(a->mutex);
            continue;
        }
        pthread_mutex_unlock(a->mutex);

        /* Sleep aleatorio 1-50 ms para simular llegada escalonada */
        usleep((rand() % 50 + 1) * 1000);

        pthread_mutex_lock(a->mutex);
        /* Re-verificar dentro del mutex para evitar condición de carrera */
        if (!p->yaIngresado) {
            if (asignarMemoriaBuddy(p, p->memoriaUsadaKB) >= 0) {
                /* Registrar el índice del bloque inicial en el BCP */
                /* asignarMemoriaBuddy actualiza p->bloqueMemoriaKB y
                   p->desperdicioInterno, pero no idxsBuddy[].
                   Lo hacemos aquí para el bloque inicial. */
                /* Buscar el bloque recién asignado */
                for (int b = 0; b < memoriaBuddy.numBloques; b++) {
                    if (!memoriaBuddy.bloques[b].libre &&
                        memoriaBuddy.bloques[b].indexProceso ==
                            (int)(p - tablaSistema.tablaBCPs) &&
                        p->numBloquesBuddy == 0) {
                        p->idxsBuddy[0]    = b;
                        p->numBloquesBuddy = 1;
                        break;
                    }
                }
                asignarSlotMemoria(p);
                asignarPaginasProceso(p);
            }
        }
        pthread_mutex_unlock(a->mutex);
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    srand((unsigned)time(NULL));

    /* Guardar estado del terminal y registrar handlers de señal */
    termSave();
    signal(SIGINT,  manejadorSenhal);
    signal(SIGTERM, manejadorSenhal);

    /* Registrar termRestore para ejecución al salir */
    atexit(termRestore);

    /* ── Inicialización ─────────────────────────────────────────────────── */
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

    /* Asignar memoria a los 150 procesos iniciales */
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        int idx = asignarMemoriaBuddy(p, p->memoriaUsadaKB);
        if (idx < 0) continue;
        /* Registrar índice del bloque inicial */
        p->idxsBuddy[0]    = idx;
        p->numBloquesBuddy = 1;
        asignarSlotMemoria(p);
        asignarPaginasProceso(p);
        p->yaIngresado = 1;
    }

    /* Cola de listos inicial: procesos con tiempoLlegada == 0 */
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

    /* ── Variables de control ───────────────────────────────────────────── */
    volatile int terminado       = 0;
    int          reloj           = 0;
    int          algoritmo       = ALG_FCFS;
    int          quantum         = 20;
    int          procesoPrivilId = -1;

    /* Ciclo del último cambio manual (bloquea el automático 200 ciclos) */
    int cicloUltimoCambioManual = -200;

    /* Historial de aprovechamiento para barras */
    static int histDesp[100];
    static int histCiclo[100];
    int histIdx       = 0;
    int iteracionesRR = 0;

    memset(histDesp,  0, sizeof(histDesp));
    memset(histCiclo, 0, sizeof(histCiclo));

    tablaSistema.algoritmoActual = ALG_FCFS;
    tablaSistema.quantumActual   = quantum;

    /* ── Mutex compartido ───────────────────────────────────────────────── */
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    /* ── Hilo de creación de procesos desde solicitudes ─────────────────── */
    ArgHiloCreacion argCreacion = {&solicitudes, &mutex, &terminado};
    pthread_t thCreacion;
    pthread_create(&thCreacion, NULL, hiloCreacionProcesos, &argCreacion);

    vistaBienvenida();
    logEvento("Simulacion iniciada");
    termRaw();

    /* ════════════════════════════════════════════════════════════════════
       BUCLE PRINCIPAL
       ════════════════════════════════════════════════════════════════════ */
    while (!terminado && !g_terminado) {

        /* ── Lectura de teclado sin bloqueo ─────────────────────────────── */
        char tecla = leerTeclaSinBloquear();

        if (tecla != 0) {

            /* Q: salir */
            if (tecla == 'q' || tecla == 'Q') {
                termRestore();
                printf("\n[Q] Terminando simulacion...\n");
                terminado = 1;
                break;
            }

            /* X: toggle de algoritmo */
            if (tecla == 'x' || tecla == 'X') {
                if (algoritmo == ALG_FCFS) {
                    char buf[64] = {0};
                    printf("\n[X] Cambiar a Round Robin\nIngrese Quantum (>0): ");
                    fflush(stdout);
                    leerLineaSegura(buf, sizeof(buf));
                    int q = atoi(buf);
                    if (q <= 0) q = 20;

                    pthread_mutex_lock(&mutex);
                    algoritmo                    = ALG_RR;
                    quantum                      = q;
                    tablaSistema.algoritmoActual = ALG_RR;
                    tablaSistema.quantumActual   = q;
                    cicloUltimoCambioManual      = reloj;
                    pthread_mutex_unlock(&mutex);
                    printf("[X] Algoritmo -> RR (Q=%d)\n", q);
                } else {
                    printf("\n[X] Cambiar a FCFS? (s/n): ");
                    fflush(stdout);
                    char conf[4] = {0};
                    leerLineaSegura(conf, sizeof(conf));
                    if (conf[0]=='s' || conf[0]=='S' ||
                        conf[0]=='y' || conf[0]=='Y') {
                        pthread_mutex_lock(&mutex);
                        algoritmo                    = ALG_FCFS;
                        tablaSistema.algoritmoActual = ALG_FCFS;
                        cicloUltimoCambioManual      = reloj;
                        pthread_mutex_unlock(&mutex);
                        printf("[X] Algoritmo -> FCFS\n");
                    } else {
                        printf("[X] Cancelado, sigue en RR (Q=%d)\n", quantum);
                    }
                }
                logEvento("Cambio manual de algoritmo");
                /* Drenar buffer de teclas repetidas */
                { char tmp; while (read(STDIN_FILENO, &tmp, 1) == 1); }
            }

            /* A: apropiatividad */
            if (tecla == 'a' || tecla == 'A') {
                pthread_mutex_lock(&mutex);
                vistaMostrarMasRezagados(&colaListos);
                pthread_mutex_unlock(&mutex);

                char idBuf[32] = {0};
                printf("Ingrese ID del proceso a privilegiar (ej: A-0): ");
                fflush(stdout);
                leerLineaSegura(idBuf, sizeof(idBuf));

                pthread_mutex_lock(&mutex);
                int encontrado = 0;
                for (int i = 0; i < TOTAL_PROCESOS; i++) {
                    Proceso *p = &tablaSistema.tablaBCPs[i];
                    if (strcmp(p->id, idBuf) == 0 &&
                        p->estado != ESTADO_TERMINADO) {
                        /* Quitar apropiatividad al proceso anterior */
                        if (procesoPrivilId >= 0)
                            tablaSistema.tablaBCPs[procesoPrivilId]
                                .esApropiativo = 0;
                        p->esApropiativo = 1;
                        procesoPrivilId  = i;
                        moverAlFrenteCola(&colaListos,   p);
                        moverAlFrenteCola(&es.disco,     p);
                        moverAlFrenteCola(&es.pantalla,  p);
                        moverAlFrenteCola(&es.teclado,   p);
                        moverAlFrenteCola(&es.impresora, p);
                        printf("[A] Proceso %s marcado como apropiativo\n",
                               p->id);
                        logEvento("Proceso marcado como apropiativo");
                        encontrado = 1;
                        break;
                    }
                }
                if (!encontrado)
                    printf("[A] Proceso '%s' no encontrado o ya termino\n",
                           idBuf);
                pthread_mutex_unlock(&mutex);
                { char tmp; while (read(STDIN_FILENO, &tmp, 1) == 1); }
            }
        }

        /* ── Lógica del simulador ────────────────────────────────────────── */
        pthread_mutex_lock(&mutex);

        reloj++;
        resetarBitsR(reloj);

        /* Ingresar procesos de enEjecucion que ya llegaron */
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                if (p->esApropiativo) encolarAlFrente(&colaListos, p);
                else                  encolar(&colaListos, p);
                p->estado      = ESTADO_LISTO;
                p->yaIngresado = 1;
            }
        }

        /* Ingresar procesos de solicitudes que ya llegaron */
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);

        /* Actualizar tiempo de espera de procesos en cola */
        actualizarEspera(&colaListos);

        /* Procesar colas E/S */
        procesarColaES(&es.disco,     &colaListos);
        procesarColaES(&es.pantalla,  &colaListos);
        procesarColaES(&es.teclado,   &colaListos);
        procesarColaES(&es.impresora, &colaListos);

        /* Redimensión de memoria cada 300 ciclos */
        if (reloj % 300 == 0)
            redimensionarMemoriaPrincipal(&enEjecucion, reloj);

        /* Cambio automático de algoritmo (bloqueado 200 ciclos post-manual) */
        if (reloj % 50 == 0 &&
            (reloj - cicloUltimoCambioManual) > 200) {
            int nuevoAlg = evaluarCambioAlgoritmo(&colaListos, &es);
            if (nuevoAlg != algoritmo) {
                algoritmo                    = nuevoAlg;
                tablaSistema.algoritmoActual = nuevoAlg;
                if (nuevoAlg == ALG_RR && quantum <= 0) {
                    quantum                    = 20;
                    tablaSistema.quantumActual = quantum;
                }
            }
        }

        /* Ejecutar scheduling */
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

        /* ── Checkpoint cada 100 ciclos ──────────────────────────────────── */
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

        /* Verificar si todos los procesos terminaron */
        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != ESTADO_TERMINADO) activos++;
        if (activos == 0 &&
            solicitudes.tamanio == 0 &&
            estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&mutex);
        usleep(1000); /* 1 ms entre ciclos */
    }

    /* ── Finalización ───────────────────────────────────────────────────── */
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