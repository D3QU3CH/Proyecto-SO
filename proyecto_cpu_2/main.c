#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include "modelo.h"
#include "controlador.h"
#include "vista.h"

/* ═══════════════════════════════════════════════════
   TERMINAL — raw/bloqueo/restaurar
   ═══════════════════════════════════════════════════ */
static struct termios g_termOrig;
static int            g_termGuardado = 0;
static volatile sig_atomic_t g_salir = 0;

static void termGuardar(void)
{
    tcgetattr(STDIN_FILENO, &g_termOrig);
    g_termGuardado = 1;
}

static void termRestaurar(void)
{
    if (!g_termGuardado) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termOrig);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
}

static void termRaw(void)
{
    struct termios t = g_termOrig;
    t.c_lflag &= ~(ICANON | ECHO);   /* ISIG activo → Ctrl+C funciona */
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f | O_NONBLOCK);
}

static void termBloqueo(void)
{
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
    struct termios t = g_termOrig;
    t.c_lflag |= (ICANON | ECHO);
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static char leerTecla(void)
{
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

/* Lee una línea en modo bloqueante, luego vuelve a raw */
static int leerLinea(char *buf, int maxlen)
{
    termBloqueo();
    fflush(stdout);
    int len = 0;
    while (len < maxlen - 1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;
        if (c == '\n' || c == '\r') break;
        if ((c == 127 || c == '\b') && len > 0) {
            len--;
            write(STDOUT_FILENO, "\b \b", 3);
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

static void manejadorSenal(int sig) { (void)sig; g_salir = 1; termRestaurar(); }

/* ═══════════════════════════════════════════════════
   MANEJO DE TECLAS DE CONTROL
   ═══════════════════════════════════════════════════ */

/* [X] Cambio de algoritmo */
static void manejarTeclaX(int *algoritmo, int *quantum,
                           int reloj, int *cicloUltimoCambio,
                           pthread_mutex_t *mutex)
{
    char buf[64] = {0};
    if (*algoritmo == ALG_FCFS) {
        printf("\n[X] Cambiar a Round Robin\n    Ingrese Quantum (>0): ");
        fflush(stdout);
        leerLinea(buf, sizeof(buf));
        int q = atoi(buf);
        if (q <= 0) q = 20;

        pthread_mutex_lock(mutex);
        *algoritmo                   = ALG_RR;
        *quantum                     = q;
        tablaSistema.algoritmoActual = ALG_RR;
        tablaSistema.quantumActual   = q;
        *cicloUltimoCambio           = reloj;
        pthread_mutex_unlock(mutex);
        printf("[X] Algoritmo -> Round Robin (Q=%d)\n", q);
    } else {
        printf("\n[X] Cambiar a FCFS? (s/n): ");
        fflush(stdout);
        leerLinea(buf, sizeof(buf));
        if (buf[0] == 's' || buf[0] == 'S' || buf[0] == 'y' || buf[0] == 'Y') {
            pthread_mutex_lock(mutex);
            *algoritmo                   = ALG_FCFS;
            tablaSistema.algoritmoActual = ALG_FCFS;
            *cicloUltimoCambio           = reloj;
            pthread_mutex_unlock(mutex);
            printf("[X] Algoritmo -> FCFS\n");
        } else {
            printf("[X] Cancelado. Sigue en RR (Q=%d)\n", *quantum);
        }
    }
    /* Drenar teclas repetidas */
    { char tmp; while (read(STDIN_FILENO, &tmp, 1) == 1); }
}

/* [A] Apropiatividad — solo un proceso puede ser apropiativo */
static void manejarTeclaA(Cola *colaListos, SistemaES *es,
                           int *procesoPrivilId, pthread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
    vistaMostrarMasRezagados(colaListos);
    pthread_mutex_unlock(mutex);

    char idBuf[32] = {0};
    printf("    Ingrese ID del proceso a privilegiar (ej: A-0): ");
    fflush(stdout);
    leerLinea(idBuf, sizeof(idBuf));

    pthread_mutex_lock(mutex);
    int encontrado = 0;
    for (int i = 0; i < TOTAL_PROCESOS; i++) {
        Proceso *p = &tablaSistema.tablaBCPs[i];
        if (strcmp(p->id, idBuf) == 0 && p->estado != ESTADO_TERMINADO) {
            /* Quitar apropiatividad al anterior */
            if (*procesoPrivilId >= 0 && *procesoPrivilId != i)
                tablaSistema.tablaBCPs[*procesoPrivilId].esApropiativo = 0;
            p->esApropiativo = 1;
            *procesoPrivilId = i;
            /* Moverlo al frente de todas las colas donde esté */
            moverAlFrenteCola(colaListos,  p);
            moverAlFrenteCola(&es->disco,     p);
            moverAlFrenteCola(&es->pantalla,  p);
            moverAlFrenteCola(&es->teclado,   p);
            moverAlFrenteCola(&es->impresora, p);
            printf("[A] %s ahora es apropiativo (al frente de listos y E/S)\n", p->id);
            encontrado = 1;
            break;
        }
    }
    if (!encontrado)
        printf("[A] Proceso '%s' no encontrado o ya terminó\n", idBuf);
    pthread_mutex_unlock(mutex);
    { char tmp; while (read(STDIN_FILENO, &tmp, 1) == 1); }
}

/* ═══════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════ */
int main(void)
{
    srand((unsigned)time(NULL));

    termGuardar();
    signal(SIGINT,  manejadorSenal);
    signal(SIGTERM, manejadorSenal);
    atexit(termRestaurar);

    /* ── Inicializar todo ────────────────────────── */
    inicializarBuddy();
    inicializarTablaSistema();

    Lista enEjecucion, solicitudes;
    inicializarLista(&enEjecucion);
    inicializarLista(&solicitudes);

    cargarPalabras("libro1.txt");
    cargarFrases("frases.txt");
    inicializarPaginacion();

    /* Poblar: 150 en ciclo, 100 en solicitudes */
    poblarListas(&enEjecucion, &solicitudes);

    /* Asignar memoria (Buddy + páginas) a los 150 procesos del ciclo */
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        int idx = asignarMemoriaBuddy(p, p->memoriaUsadaKB);
        if (idx >= 0) {
            p->idxsBuddy[0]    = idx;
            p->numBloquesBuddy = 1;
        }
        asignarPaginasProceso(p);
        p->yaIngresado = 1;
    }

    /* Cola de listos: solo los que llegan en t=0 */
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

    /* ── Variables de control ───────────────────── */
    int reloj               = 0;
    int algoritmo           = ALG_FCFS;
    int quantum             = 40;
    int procesoPrivilId     = -1;
    int cicloUltimoCambio   = -300;  /* para no bloquear el auto-cambio al inicio */
    int iteracionesRR       = 0;
    volatile int terminado  = 0;

    static int histDesp[100];
    static int histCiclo[100];
    int histIdx = 0;
    memset(histDesp, 0, sizeof(histDesp));
    memset(histCiclo, 0, sizeof(histCiclo));

    tablaSistema.algoritmoActual = ALG_FCFS;
    tablaSistema.quantumActual   = quantum;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    vistaBienvenida();
    termRaw();

    /* ════════════════════════════════════════════
       BUCLE PRINCIPAL
       ════════════════════════════════════════════ */
    while (!terminado && !g_salir) {

        /* ── Teclado sin bloqueo ─────────────── */
        char tecla = leerTecla();
        if (tecla != 0) {
            if (tecla == 'q' || tecla == 'Q') {
                printf("\n[Q] Saliendo...\n");
                break;
            }
            if (tecla == 'x' || tecla == 'X')
                manejarTeclaX(&algoritmo, &quantum, reloj,
                              &cicloUltimoCambio, &mutex);
            if (tecla == 'a' || tecla == 'A')
                manejarTeclaA(&colaListos, &es, &procesoPrivilId, &mutex);
        }

        /* ── Lógica del simulador ────────────── */
        pthread_mutex_lock(&mutex);
        reloj++;
        tablaSistema.cicloActual = reloj;

        /* Resetear bits R cada 50 ciclos (NRU) */
        resetarBitsR(reloj);

        /* Ingresar procesos del ciclo que ya llegaron */
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                if (p->esApropiativo) encolarAlFrente(&colaListos, p);
                else                  encolar(&colaListos, p);
                p->estado      = ESTADO_LISTO;
                p->yaIngresado = 1;
            }
        }

        /* Ingresar procesos de solicitudes que llegaron */
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);

        /* Incrementar tiempo de espera */
        actualizarEspera(&colaListos);

        /* Avanzar colas E/S */
        procesarColaES(&es.disco,     &colaListos);
        procesarColaES(&es.pantalla,  &colaListos);
        procesarColaES(&es.teclado,   &colaListos);
        procesarColaES(&es.impresora, &colaListos);

        /* Redimensionar memoria cada 300 ciclos */
        if (reloj % 300 == 0)
            redimensionarMemoriaPrincipal(&enEjecucion, reloj);

        if (reloj % 200 == 0 && (reloj - cicloUltimoCambio) > 800) {
            int nuevo = evaluarCambioAlgoritmo(&colaListos, &es);
            if (nuevo != algoritmo) {
                algoritmo                    = nuevo;
                tablaSistema.algoritmoActual = nuevo;
                if (nuevo == ALG_RR && quantum <= 0) {
                    quantum = 20;
                    tablaSistema.quantumActual = 20;
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

        /* Estadísticas */
        calcularDesperdicioExterno();
        actualizarPromedioFinalizados(reloj);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes,
                                    &colaListos, &es, reloj);

        /* ── Reporte cada 100 ciclos ─────────── */
        if (reloj % 100 == 0) {
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
        }

        /* ── Verificar fin de simulación ─────── */
        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != ESTADO_TERMINADO) activos++;
        if (activos == 0 && solicitudes.tamanio == 0 && estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&mutex);
        usleep(500);  /* 0.5 ms entre ciclos */
    }

    /* ── Cierre ─────────────────────────────── */
    termRestaurar();
    pthread_mutex_destroy(&mutex);

    guardarBCPs(&enEjecucion, "bcps.log");
    guardarVariablesGlobales("variables.log");
    mostrarEstadisticasMemoria();
    vistaCierre(reloj, tablaSistema.procesosTerminados);
    return 0;
}