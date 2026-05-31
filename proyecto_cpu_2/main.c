#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"

static int histDesp[100];
static int histCiclo[100];
static int histIdx = 0;

// Hilo que va creando los procesos de solicitudes con un sleep aleatorio 1-50ms
// Emula que los procesos van llegando al sistema en momentos distintos
typedef struct {
    Lista           *solicitudes;
    Cola            *colaListos;
    pthread_mutex_t *mutex;
    int             *terminado;
    int             *reloj;
} ArgHiloCreacion;

void *hiloCreacionProcesos(void *arg)
{
    ArgHiloCreacion *a = (ArgHiloCreacion *)arg;

    // Recorre todos los BCPs que aun no tienen tiempoLlegada procesado
    // Simula la creacion diferenciada con sleep entre 1-50ms
    for (int i = 0; i < TOTAL_PROCESOS && !(*a->terminado); i++) {
        Proceso *p = &tablaSistema.tablaBCPs[i];
        // Solo procesar los que son de solicitudes (yaIngresado == 0)
        if (p->yaIngresado) continue;

        // Sleep aleatorio entre 1-50 ms para emular creacion escalonada
        int sleepMs = rand() % 50 + 1;
        usleep(sleepMs * 1000);

        // Asignar memoria al proceso cuando "llega" al sistema
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

    // 1. Inicializar
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

    // Asignar Buddy + slot + paginas a los 150 del ciclo inicial
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (asignarMemoriaBuddy(p, p->memoriaUsadaKB) < 0) continue;
        asignarSlotMemoria(p);
        asignarPaginasProceso(p);
        p->yaIngresado = 1;
    }

    // Cola de listos ordenada por tiempo de llegada (FCFS)
    Cola colaListos;
    inicializarCola(&colaListos);

    // Encolar los del ciclo que ya llegaron en t=0
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (p->tiempoLlegada == 0) {
            encolar(&colaListos, p);
            p->estado = ESTADO_LISTO;
        }
    }

    SistemaES es;
    inicializarSistemaES(&es);

    // 2. Estado de control
    int terminado       = 0;
    int reloj           = 0;
    int algoritmo       = ALG_FCFS;
    int quantum         = 20;
    int procesoPrivilId = -1;
    int iteracionesRR   = 0;

    tablaSistema.algoritmoActual = ALG_FCFS;
    tablaSistema.quantumActual   = quantum;

    // 3. Contexto de hilos
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

    ArgHiloCreacion argCreacion = {&solicitudes, &colaListos, &ctx.mutexPrincipal, &terminado, &reloj};

    pthread_t thDisco, thPantalla, thTeclado, thImpresora, thReloj, thEntrada, thCreacion;
    pthread_create(&thDisco,     NULL, hiloDispositivoES,    &argDisco);
    pthread_create(&thPantalla,  NULL, hiloDispositivoES,    &argPantalla);
    pthread_create(&thTeclado,   NULL, hiloDispositivoES,    &argTeclado);
    pthread_create(&thImpresora, NULL, hiloDispositivoES,    &argImpresora);
    pthread_create(&thReloj,     NULL, hiloReloj,            &ctx);
    pthread_create(&thEntrada,   NULL, hiloEntrada,          &ctx);
    pthread_create(&thCreacion,  NULL, hiloCreacionProcesos, &argCreacion);

    vistaBienvenida();
    logEvento("Simulacion iniciada");

    // 4. Loop principal
    while (!terminado) {
        pthread_mutex_lock(&ctx.mutexPrincipal);

        reloj++;
        resetarBitsR(reloj);

        // Encolar procesos del ciclo que llegan segun su tiempoLlegada
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                encolar(&colaListos, p);
                p->estado = ESTADO_LISTO;
                p->yaIngresado = 1;
            }
        }

        // Encolar procesos de solicitudes que ya llegaron (se eliminan de la lista)
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);

        // Incrementar espera de los que estan esperando en cola listos
        actualizarEspera(&colaListos);

        // Redimension automatica cada 300 ciclos
        if (reloj % 300 == 0)
            redimensionarMemoriaPrincipal(&enEjecucion, reloj);

        // Evaluacion automatica de cambio de algoritmo cada 50 ciclos
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

            if (algoritmo == ALG_FCFS) {
                // FCFS: primer proceso de la cola ejecuta su rafaga completa
                Proceso *p = desencolar(&colaListos);
                procesarEntradaCPU(p);
                tablaSistema.totalCambiosContexto++;

                if (p->ciclosRestantes <= 0) {
                    procesarTerminacion(p);
                    if (procesoPrivilId == (int)(p - tablaSistema.tablaBCPs)) {
                        printf("[FCFS] Proceso %s termino (era apropiativo)\n", p->id);
                        procesoPrivilId = -1;
                    }
                } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
                    asignarES(p, &es);
                } else {
                    p->estado = ESTADO_LISTO;
                    if (p->esApropiativo)
                        encolarAlFrente(&colaListos, p);
                    else
                        encolar(&colaListos, p);
                }

            } else {
                // RR: el proceso tiene un maximo de quantum ciclos
                // Si rafagaActual > quantum: se interrumpe y vuelve al final
                // Si rafagaActual <= quantum: termina su turno (o va a E/S)
                Proceso *p = desencolar(&colaListos);
                procesarEntradaCPU(p);
                tablaSistema.totalCambiosContexto++;
                iteracionesRR++;

                int q     = tablaSistema.quantumActual;
                int usado = (p->rafagaActual < q) ? p->rafagaActual : q;
                int desp  = q - usado;

                p->aprovechamiento = (usado * 100) / (q ? q : 1);
                p->desperdicio     = desp;

                histDesp[histIdx]  = desp * 100 / (q ? q : 1);
                histCiclo[histIdx] = reloj;
                histIdx = (histIdx + 1) % 100;

                // Ajuste automatico de quantum cada 20 iteraciones RR
                ajustarQuantumAutomatico(&colaListos, &es, iteracionesRR);
                quantum = tablaSistema.quantumActual;

                if (p->ciclosRestantes <= 0) {
                    procesarTerminacion(p);
                    if (procesoPrivilId == (int)(p - tablaSistema.tablaBCPs)) {
                        printf("[RR] Proceso %s termino (era apropiativo)\n", p->id);
                        procesoPrivilId = -1;
                    }
                } else if (p->rafagaActual > q) {
                    // Rafaga supero el quantum: proceso interrumpido, vuelve al final
                    p->estado = ESTADO_LISTO;
                    if (p->esApropiativo)
                        encolarAlFrente(&colaListos, p);
                    else
                        encolar(&colaListos, p);
                } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
                    // Uso menos que el quantum y es ES-bound: va a E/S
                    asignarES(p, &es);
                } else {
                    // Uso menos que el quantum, no va a E/S: vuelve a cola
                    p->estado = ESTADO_LISTO;
                    if (p->esApropiativo)
                        encolarAlFrente(&colaListos, p);
                    else
                        encolar(&colaListos, p);
                }
            }
        }

        // Estadisticas por ciclo
        calcularDesperdicioExterno();
        actualizarPromedioFinalizados(reloj);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes, &colaListos, &es, reloj);

        // Checkpoint cada 100 ciclos
        if (reloj % 100 == 0) {
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

        // Condicion de fin: todos los procesos terminaron
        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != ESTADO_TERMINADO) activos++;
        if (activos == 0 && solicitudes.tamanio == 0 && estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&ctx.mutexPrincipal);
        usleep(1000);
    }

    // 5. Cierre
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
    pthread_join(thEntrada,   NULL);
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