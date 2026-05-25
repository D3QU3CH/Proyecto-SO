#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"

int main(void)
{
    srand((unsigned)time(NULL));

    // ── 1. INICIALIZACION ─────────────────────────────────────────────────────
    cargarLibro("libro1.txt");
    inicializarBuddy();
    inicializarTablaSistema();

    Lista enEjecucion, solicitudes;
    inicializarLista(&enEjecucion);
    inicializarLista(&solicitudes);
    poblarListas(&enEjecucion, &solicitudes);

    Cola colaListos;
    inicializarCola(&colaListos);

    SistemaES es;
    inicializarSistemaES(&es);

    // ── 2. CONTEXTO DE HILOS ─────────────────────────────────────────────────
    int terminado = 0;
    int reloj = 0;

    ContextoHilos ctx;
    ctx.procesosEnEjecucion = &enEjecucion;
    ctx.nuevasSolicitudes = &solicitudes;
    ctx.colaListos = &colaListos;
    ctx.es = &es;
    ctx.reloj = &reloj;
    ctx.terminado = &terminado;

    pthread_mutex_init(&ctx.mutexPrincipal, NULL);
    pthread_mutex_init(&ctx.mutexMemoria, NULL);
    sem_init(&ctx.semDisco, 0, 0);
    sem_init(&ctx.semPantalla, 0, 0);
    sem_init(&ctx.semTeclado, 0, 0);
    sem_init(&ctx.semImpresora, 0, 0);

    ArgHiloES argDisco = {&es.disco, &colaListos,
                          &ctx.mutexPrincipal, &ctx.semDisco,
                          &terminado, "Disco"};
    ArgHiloES argPantalla = {&es.pantalla, &colaListos,
                             &ctx.mutexPrincipal, &ctx.semPantalla,
                             &terminado, "Pantalla"};
    ArgHiloES argTeclado = {&es.teclado, &colaListos,
                            &ctx.mutexPrincipal, &ctx.semTeclado,
                            &terminado, "Teclado"};
    ArgHiloES argImpresora = {&es.impresora, &colaListos,
                              &ctx.mutexPrincipal, &ctx.semImpresora,
                              &terminado, "Impresora"};

    // ── 3. LANZAR HILOS ──────────────────────────────────────────────────────
    pthread_t thDisco, thPantalla, thTeclado, thImpresora, thReloj, thEntrada;
    pthread_create(&thDisco, NULL, hiloDispositivoES, &argDisco);
    pthread_create(&thPantalla, NULL, hiloDispositivoES, &argPantalla);
    pthread_create(&thTeclado, NULL, hiloDispositivoES, &argTeclado);
    pthread_create(&thImpresora, NULL, hiloDispositivoES, &argImpresora);
    pthread_create(&thReloj, NULL, hiloReloj, &ctx);
    pthread_create(&thEntrada, NULL, hiloEntrada, &ctx);

    // ── 4. BIENVENIDA ────────────────────────────────────────────────────────
    vistaBienvenida();
    vistaMostrarLista(&enEjecucion, "procesosEnEjecucion");
    vistaMostrarBuddy();
    logEvento("Simulacion iniciada");

    // ── 5. LOOP PRINCIPAL ────────────────────────────────────────────────────
    // Solo prueba estructuras: avanza reloj, ingresa procesos, actualiza espera
    while (!terminado)
    {
        pthread_mutex_lock(&ctx.mutexPrincipal);

        reloj++;
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);
        actualizarEspera(&colaListos);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes,
                                    &colaListos, &es, reloj);

        if (reloj % 100 == 0)
        {
            vistaMostrarColaListos(&colaListos);
            vistaMostrarTablaGlobal();
            guardarBCPs(&enEjecucion, "bcps.log");
            guardarVariablesGlobales("variables.log");
            logEvento("Checkpoint");
        }

        pthread_mutex_unlock(&ctx.mutexPrincipal);
        usleep(1000);
    }

    // ── 6. CIERRE ────────────────────────────────────────────────────────────
    terminado = 1;
    sem_post(&ctx.semDisco);
    sem_post(&ctx.semPantalla);
    sem_post(&ctx.semTeclado);
    sem_post(&ctx.semImpresora);

    pthread_join(thDisco, NULL);
    pthread_join(thPantalla, NULL);
    pthread_join(thTeclado, NULL);
    pthread_join(thImpresora, NULL);
    pthread_join(thReloj, NULL);
    pthread_join(thEntrada, NULL);

    pthread_mutex_destroy(&ctx.mutexPrincipal);
    pthread_mutex_destroy(&ctx.mutexMemoria);
    sem_destroy(&ctx.semDisco);
    sem_destroy(&ctx.semPantalla);
    sem_destroy(&ctx.semTeclado);
    sem_destroy(&ctx.semImpresora);

    guardarBCPs(&enEjecucion, "bcps.log");
    guardarVariablesGlobales("variables.log");
    logEvento("Simulacion finalizada");

    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
        liberarProceso(n->proceso);

    vistaCierre(reloj, tablaSistema.procesosTerminados);
    return 0;
}