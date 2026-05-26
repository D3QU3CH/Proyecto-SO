#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"

// Historial de aprovechamiento para barras RR (100 entradas circulares)
static int histDesp[100];
static int histCiclo[100];
static int histIdx  = 0;

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

    // Asignar Buddy + slot + paginas a los procesos del ciclo
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (asignarMemoriaBuddy(p, p->memoriaUsadaKB) < 0)
            continue;
        asignarSlotMemoria(p);
        asignarPaginasProceso(p);
    }

    Cola colaListos;
    inicializarCola(&colaListos);

    // Encolar los procesos del ciclo en colaListos (ya "llegaron")
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
        encolar(&colaListos, n->proceso);

    SistemaES es;
    inicializarSistemaES(&es);

    // 2. Estado de control
    int terminado      = 0;
    int reloj          = 0;
    int algoritmo      = ALG_FCFS;
    int quantum        = 20;
    int procesoPrivilId = -1;
    int iteracionesRR  = 0;

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

    ArgHiloES argDisco    = {&es.disco,     &colaListos, &ctx.mutexPrincipal, &ctx.semDisco,     &terminado, "Disco"};
    ArgHiloES argPantalla = {&es.pantalla,  &colaListos, &ctx.mutexPrincipal, &ctx.semPantalla,  &terminado, "Pantalla"};
    ArgHiloES argTeclado  = {&es.teclado,   &colaListos, &ctx.mutexPrincipal, &ctx.semTeclado,   &terminado, "Teclado"};
    ArgHiloES argImpresora= {&es.impresora, &colaListos, &ctx.mutexPrincipal, &ctx.semImpresora, &terminado, "Impresora"};

    pthread_t thDisco, thPantalla, thTeclado, thImpresora, thReloj, thEntrada;
    pthread_create(&thDisco,     NULL, hiloDispositivoES, &argDisco);
    pthread_create(&thPantalla,  NULL, hiloDispositivoES, &argPantalla);
    pthread_create(&thTeclado,   NULL, hiloDispositivoES, &argTeclado);
    pthread_create(&thImpresora, NULL, hiloDispositivoES, &argImpresora);
    pthread_create(&thReloj,     NULL, hiloReloj, &ctx);
    pthread_create(&thEntrada,   NULL, hiloEntrada, &ctx);

    // 4. Estado inicial
    vistaBienvenida();
    logEvento("Simulacion iniciada");

    // 5. Loop principal
    while (!terminado) {
        pthread_mutex_lock(&ctx.mutexPrincipal);

        reloj++;
        resetarBitsR(reloj);
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);
        actualizarEspera(&colaListos);

        // Evaluar cambio automatico de algoritmo cada 50 ciclos
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
                // ── FCFS ──────────────────────────────────────────────────
                Proceso *p = desencolar(&colaListos);
                procesarEntradaCPU(p);

                if (p->ciclosRestantes <= 0) {
                    procesarTerminacion(p);
                    if (procesoPrivilId == (int)(p - tablaSistema.tablaBCPs)) {
                        printf("  [RR] Proceso apropiativo %s finalizo\n", p->id);
                        procesoPrivilId = -1;
                    }
                } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
                    asignarES(p, &es, &colaListos);
                } else {
                    if (p->esApropiativo)
                        encolarAlFrente(&colaListos, p);
                    else
                        encolar(&colaListos, p);
                }

            } else {
                // ── ROUND ROBIN ───────────────────────────────────────────
                Proceso *p = desencolar(&colaListos);
                procesarEntradaCPU(p);
                iteracionesRR++;

                int q = tablaSistema.quantumActual;
                // Cuanto uso del quantum en esta rafaga
                int usado = (p->rafagaActual < q) ? p->rafagaActual : q;
                int desp  = q - usado;
                p->aprovechamiento = (usado * 100) / (q ? q : 1);
                p->desperdicio     = desp;

                // Guardar en historial
                histDesp[histIdx]  = desp * 100 / (q ? q : 1);
                histCiclo[histIdx] = reloj;
                histIdx = (histIdx + 1) % 100;

                // Ajuste automatico cada 20 iteraciones RR
                ajustarQuantumAutomatico(&colaListos, &es, iteracionesRR);
                quantum = tablaSistema.quantumActual;

                if (p->ciclosRestantes <= 0) {
                    procesarTerminacion(p);
                    if (procesoPrivilId == (int)(p - tablaSistema.tablaBCPs)) {
                        printf("  [RR] Proceso apropiativo %s finalizo\n", p->id);
                        procesoPrivilId = -1;
                    }
                } else if (p->tipoProceso == 1 && rand() % 3 == 0) {
                    asignarES(p, &es, &colaListos);
                } else {
                    // Si uso mas del quantum, va al final; si es apropiativo va al frente
                    if (p->esApropiativo)
                        encolarAlFrente(&colaListos, p);
                    else
                        encolar(&colaListos, p);
                }
            }

            tablaSistema.totalCambiosContexto++;
        }

        // Estadisticas
        calcularDesperdicioExterno();
        actualizarPromedioFinalizados(reloj);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes, &colaListos, &es, reloj);

        // Checkpoint cada 100 ciclos
        if (reloj % 100 == 0) {
            vistaMostrarTablaGlobal();
            mostrarEstadisticasMemoria();
            if (algoritmo == ALG_RR) {
                vistaBarrasAprovechamiento(&colaListos, histDesp, histCiclo, histIdx);
                mostrarEnvejecimiento(&colaListos);
            }
            guardarBCPs(&enEjecucion, "bcps.log");
            guardarVariablesGlobales("variables.log");
            logEvento("Checkpoint");
        }

        // Verificar si todos terminaron
        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != 3) activos++;
        if (activos == 0 && solicitudes.tamanio == 0 && estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&ctx.mutexPrincipal);
        usleep(1000);
    }

    // 6. Cierre
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