#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"
#include "master.h"

static int histDesp[100];
static int histCiclo[100];
static int histIdx     = 0;
static int iteracionesRR = 0;

typedef struct {
    Lista           *solicitudes;
    pthread_mutex_t *mutex;
    int             *terminado;
} ArgHiloCreacion;

// Hilo que crea procesos de solicitudes con sleep aleatorio 1-50ms
// Emula llegada escalonada: cuando sale el primer proceso del CPU
// probablemente ya llegaron 2-5 mas
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

    // Asignar memoria a los 150 del ciclo inicial
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (asignarMemoriaBuddy(p, p->memoriaUsadaKB) < 0) continue;
        asignarSlotMemoria(p);
        asignarPaginasProceso(p);
        p->yaIngresado = 1;
    }

    // Cola de listos: orden por tiempo de llegada (FCFS inicial)
    Cola colaListos;
    inicializarCola(&colaListos);

    // Encolar los del ciclo que llegan en t=0
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

    ContextoHilos ctx;
    ctx.procesosEnEjecucion = &enEjecucion;
    ctx.solicitudes         = &solicitudes;
    ctx.colaListos          = &colaListos;
    ctx.es                  = &es;
    ctx.reloj               = &reloj;
    ctx.terminado           = &terminado;
    ctx.algoritmo           = &algoritmo;
    ctx.quantum             = &quantum;
    ctx.procesoPrivilId     = &procesoPrivilId;

    pthread_mutex_init(&ctx.mutexPrincipal, NULL);
    sem_init(&ctx.semDisco,     0, 0);
    sem_init(&ctx.semPantalla,  0, 0);
    sem_init(&ctx.semTeclado,   0, 0);
    sem_init(&ctx.semImpresora, 0, 0);

    ArgHiloES argDisco     = {&es.disco,     &colaListos, &ctx.mutexPrincipal, &ctx.semDisco,     &terminado, "Disco"};
    ArgHiloES argPantalla  = {&es.pantalla,  &colaListos, &ctx.mutexPrincipal, &ctx.semPantalla,  &terminado, "Pantalla"};
    ArgHiloES argTeclado   = {&es.teclado,   &colaListos, &ctx.mutexPrincipal, &ctx.semTeclado,   &terminado, "Teclado"};
    ArgHiloES argImpresora = {&es.impresora, &colaListos, &ctx.mutexPrincipal, &ctx.semImpresora, &terminado, "Impresora"};
    ArgHiloCreacion argCreacion = {&solicitudes, &ctx.mutexPrincipal, &terminado};

    pthread_t thDisco, thPantalla, thTeclado, thImpresora, thReloj, thCreacion;
    pthread_create(&thDisco,     NULL, hiloDispositivoES,    &argDisco);
    pthread_create(&thPantalla,  NULL, hiloDispositivoES,    &argPantalla);
    pthread_create(&thTeclado,   NULL, hiloDispositivoES,    &argTeclado);
    pthread_create(&thImpresora, NULL, hiloDispositivoES,    &argImpresora);
    pthread_create(&thReloj,     NULL, hiloReloj,            &ctx);
    pthread_create(&thCreacion,  NULL, hiloCreacionProcesos, &argCreacion);

    vistaBienvenida();
    logEvento("Simulacion iniciada");

    while (!terminado) {

        // Detectar teclado sin bloquear (X=cambiar algoritmo, A=apropiativo, Q=salir)
        manejarEntrada(&ctx);

        if (*ctx.terminado) break;

        pthread_mutex_lock(&ctx.mutexPrincipal);

        reloj++;
        resetarBitsR(reloj);

        // Encolar procesos del ciclo segun tiempoLlegada
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                if (p->esApropiativo) encolarAlFrente(&colaListos, p);
                else                  encolar(&colaListos, p);
                p->estado = ESTADO_LISTO;
                p->yaIngresado = 1;
            }
        }

        // Encolar solicitudes que llegaron (se eliminan de la lista)
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);

        // Incrementar espera de los que estan en cola listos
        actualizarEspera(&colaListos);

        // Redimension automatica cada 300 ciclos
        if (reloj % 300 == 0)
            redimensionarMemoriaPrincipal(&enEjecucion, reloj);

        // Cambio automatico de algoritmo cada 50 ciclos
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

        // Ejecutar algoritmo activo
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

        // Checkpoint cada 100 ciclos
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

        // Condicion de fin: todos terminaron y no hay mas en colas
        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != ESTADO_TERMINADO) activos++;
        if (activos == 0 && solicitudes.tamanio == 0 && estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&ctx.mutexPrincipal);
        usleep(1000);
    }

    // Cierre limpio
    terminado = 1;
    sem_post(&ctx.semDisco);
    sem_post(&ctx.semPantalla);
    sem_post(&ctx.semTeclado);
    sem_post(&ctx.semImpresora);

    pthread_join(thDisco,     NULL);
    pthread_join(thPantalla,  NULL);
    pthread_join(thTeclado,   NULL);
    pthread_join(thImpresora, NULL);
    pthread_join(thReloj,     NULL);
    pthread_join(thCreacion,  NULL);

    pthread_mutex_destroy(&ctx.mutexPrincipal);
    sem_destroy(&ctx.semDisco);
    sem_destroy(&ctx.semPantalla);
    sem_destroy(&ctx.semTeclado);
    sem_destroy(&ctx.semImpresora);

    guardarBCPs(&enEjecucion, "bcps.log");
    guardarVariablesGlobales("variables.log");
    logEvento("Simulacion finalizada");
    mostrarEstadisticasMemoria();
    vistaCierre(reloj, tablaSistema.procesosTerminados);
    return 0;
}