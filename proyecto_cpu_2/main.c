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

/* Ciclo en que se hizo el último cambio MANUAL de algoritmo.
   Durante 200 ciclos tras un cambio manual, el cambio automático
   queda bloqueado para que el usuario vea el efecto. */
static int cicloUltimoCambioManual = -200;

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
   read() directo al fd — stdio bufferiza y no respeta O_NONBLOCK en Linux.
   ISIG se mantiene activo para que Ctrl+C funcione.
   ═══════════════════════════════════════════════════════════════════════════ */
static struct termios g_termOrig;

static void termSave(void)   { tcgetattr(STDIN_FILENO, &g_termOrig); }

static void termRestore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termOrig);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
}

/* raw + nonblocking — SIN tocar ISIG para que Ctrl+C siga funcionando */
static void termRaw(void)
{
    struct termios t = g_termOrig;
    t.c_lflag &= ~(ICANON | ECHO);   /* NO quitamos ISIG */
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f | O_NONBLOCK);
}

/* canónico bloqueante con eco para leer texto del usuario */
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

/* Retorna el carácter leído o 0 si no había nada (nunca bloquea) */
static char leerTeclaSinBloquear(void)
{
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

/* Lee línea del usuario: cambia a bloqueante, lee byte a byte, vuelve a raw */
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

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════════ */
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
        if (p->tiempoLlegada == 0) { encolar(&colaListos, p); p->estado = ESTADO_LISTO; }
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
    termRaw();

    while (!terminado) {

        /* ── Tecla sin bloquear ───────────────────────────────────────────── */
        char tecla = leerTeclaSinBloquear();

        if (tecla != 0) {

            /* Q: salir */
            if (tecla == 'q' || tecla == 'Q') {
                termRestore();
                printf("\n[Q] Terminando simulacion...\n");
                terminado = 1;
                break;
            }

            /* ── X: toggle de algoritmo ───────────────────────────────────── */
            if (tecla == 'x' || tecla == 'X') {

                if (algoritmo == ALG_FCFS) {
                    /* FCFS → RR: pedir quantum */
                    char buf[64] = {0};
                    printf("\n[X] Cambiar a Round Robin\nIngrese Quantum (>0): ");
                    fflush(stdout);
                    leerLineaSegura(buf, sizeof(buf));
                    int q = atoi(buf);
                    if (q <= 0) q = 20;

                    pthread_mutex_lock(&mutex);
                    algoritmo  = ALG_RR;
                    quantum    = q;
                    tablaSistema.algoritmoActual = ALG_RR;
                    tablaSistema.quantumActual   = q;
                    cicloUltimoCambioManual      = reloj;
                    pthread_mutex_unlock(&mutex);
                    printf("[X] Algoritmo -> RR (Q=%d)\n", q);

                } else {
                    /* RR → FCFS: pedir confirmacion para evitar cambio accidental */
                    printf("\n[X] Cambiar a FCFS? (s/n): ");
                    fflush(stdout);
                    char conf[4] = {0};
                    leerLineaSegura(conf, sizeof(conf));
                    if (conf[0] == 's' || conf[0] == 'S' || conf[0] == 'y' || conf[0] == 'Y') {
                        pthread_mutex_lock(&mutex);
                        algoritmo  = ALG_FCFS;
                        tablaSistema.algoritmoActual = ALG_FCFS;
                        cicloUltimoCambioManual      = reloj;
                        pthread_mutex_unlock(&mutex);
                        printf("[X] Algoritmo -> FCFS\n");
                    } else {
                        printf("[X] Cancelado, sigue en RR (Q=%d)\n", quantum);
                    }
                }
                logEvento("Cambio manual de algoritmo");

                /* Drenar cualquier tecla repetida que haya quedado en el buffer */
                { char tmp; while (read(STDIN_FILENO, &tmp, 1) == 1); }
            }

            /* ── A: apropiatividad ────────────────────────────────────────── */
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
                    if (strcmp(p->id, idBuf) == 0 && p->estado != ESTADO_TERMINADO) {
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

        /* ── Lógica del simulador ─────────────────────────────────────────── */
        pthread_mutex_lock(&mutex);

        reloj++;
        resetarBitsR(reloj);

        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                if (p->esApropiativo) encolarAlFrente(&colaListos, p);
                else                  encolar(&colaListos, p);
                p->estado = ESTADO_LISTO; p->yaIngresado = 1;
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

        /* Cambio automático — bloqueado 200 ciclos tras cambio manual */
        if (reloj % 50 == 0 && (reloj - cicloUltimoCambioManual) > 200) {
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
        actualizarVariablesGlobales(&enEjecucion, &solicitudes, &colaListos, &es, reloj);

        if (reloj % 100 == 0) {
            ejecutarMasterPVM(&enEjecucion, quantum);
            vistaMostrarTablaGlobal();
            vistaEstadoES(&es);
            mostrarEstadisticasMemoria();
            if (algoritmo == ALG_RR) {
                vistaBarrasAprovechamiento(&colaListos, histDesp, histCiclo, histIdx);
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
        if (activos == 0 && solicitudes.tamanio == 0 && estaVaciaCola(&colaListos))
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


    system("ssh slave1 'rm -f /tmp/pvm_slave.log'");
    system("ssh slave2 'rm -f /tmp/pvm_slave.log'");

    return 0;
}