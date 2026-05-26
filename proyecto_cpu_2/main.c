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

    // 1. Inicializar estructuras
    inicializarBuddy();
    inicializarTablaSistema();

    Lista enEjecucion, solicitudes;
    inicializarLista(&enEjecucion);
    inicializarLista(&solicitudes);

    cargarPalabras("libro1.txt");
    inicializarMemoriaPrincipal();

    poblarListas(&enEjecucion, &solicitudes);

    // Asignar memoria Buddy a los 150 del ciclo
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
    {
        Proceso *p = n->proceso;
        int idx = asignarMemoriaBuddy(p, p->memoriaUsadaKB);
        if (idx < 0)
            printf("  [BUDDY] Sin espacio para %s\n", p->id);
    }

    // Después del loop que asigna Buddy a los 150:
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
        asignarSlotMemoria(n->proceso); // ← agrega esta línea

    Cola colaListos;
    inicializarCola(&colaListos);

    SistemaES es;
    inicializarSistemaES(&es);

    // 2. Contexto de hilos
    int terminado = 0, reloj = 0;

    ContextoHilos ctx;
    ctx.procesosEnEjecucion = &enEjecucion;
    ctx.solicitudes = &solicitudes;
    ctx.colaListos = &colaListos;
    ctx.es = &es;
    ctx.reloj = &reloj;
    ctx.terminado = &terminado;

    pthread_mutex_init(&ctx.mutexPrincipal, NULL);
    sem_init(&ctx.semDisco, 0, 0);
    sem_init(&ctx.semPantalla, 0, 0);
    sem_init(&ctx.semTeclado, 0, 0);
    sem_init(&ctx.semImpresora, 0, 0);

    ArgHiloES argDisco = {&es.disco, &colaListos, &ctx.mutexPrincipal, &ctx.semDisco, &terminado, "Disco"};
    ArgHiloES argPantalla = {&es.pantalla, &colaListos, &ctx.mutexPrincipal, &ctx.semPantalla, &terminado, "Pantalla"};
    ArgHiloES argTeclado = {&es.teclado, &colaListos, &ctx.mutexPrincipal, &ctx.semTeclado, &terminado, "Teclado"};
    ArgHiloES argImpresora = {&es.impresora, &colaListos, &ctx.mutexPrincipal, &ctx.semImpresora, &terminado, "Impresora"};

    // 3. Lanzar hilos
    pthread_t thDisco, thPantalla, thTeclado, thImpresora, thReloj, thEntrada;
    pthread_create(&thDisco, NULL, hiloDispositivoES, &argDisco);
    pthread_create(&thPantalla, NULL, hiloDispositivoES, &argPantalla);
    pthread_create(&thTeclado, NULL, hiloDispositivoES, &argTeclado);
    pthread_create(&thImpresora, NULL, hiloDispositivoES, &argImpresora);
    pthread_create(&thReloj, NULL, hiloReloj, &ctx);
    pthread_create(&thEntrada, NULL, hiloEntrada, &ctx);

    // 4. Mostrar estado inicial
    vistaBienvenida();
    vistaMostrarLista(&enEjecucion, "procesosEnEjecucion");
    vistaMostrarLista(&solicitudes, "solicitudes");
    vistaMostrarBuddy();
    logEvento("Simulacion iniciada");

    // 5. Loop principal
    while (!terminado)
    {
        pthread_mutex_lock(&ctx.mutexPrincipal);

        reloj++;
        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);
        actualizarEspera(&colaListos);

        // ── SCHEDULER FCFS: tomar el siguiente de la cola y ejecutarlo ──
        if (!estaVaciaCola(&colaListos))
        {
            Proceso *p = desencolar(&colaListos);

            // Requisitos: re-sortear cc, crecer memoria, descontar ciclos
            procesarEntradaCPU(p);

            if (p->ciclosRestantes <= 0)
            {
                // Proceso terminado: liberar Buddy + slot
                procesarTerminacion(p);
            }
            else if (p->tipoProceso == 1 && rand() % 3 == 0)
            {
                // Proceso ES-bound: enviar a E/S con probabilidad 1/3
                asignarES(p, &es);
            }
            else
            {
                // Vuelve a la cola de listos para la siguiente rafaga
                encolar(&colaListos, p);
            }

            tablaSistema.totalCambiosContexto++;
        }

        // Actualizar estadisticas de memoria (enunciado: datos de rendimiento)
        calcularDesperdicioExterno();
        actualizarPromedioFinalizados(reloj);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes,
                                    &colaListos, &es, reloj);

        if (reloj % 100 == 0)
        {
            vistaMostrarColaListos(&colaListos);
            vistaMostrarTablaGlobal();
            mostrarEstadisticasMemoria(); // <-- mostrar estadisticas completas
            guardarBCPs(&enEjecucion, "bcps.log");
            guardarVariablesGlobales("variables.log");
            logEvento("Checkpoint");
        }

        pthread_mutex_unlock(&ctx.mutexPrincipal);
        usleep(1000);
    }

    // 6. Cierre
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
    sem_destroy(&ctx.semDisco);
    sem_destroy(&ctx.semPantalla);
    sem_destroy(&ctx.semTeclado);
    sem_destroy(&ctx.semImpresora);

    guardarBCPs(&enEjecucion, "bcps.log");
    guardarVariablesGlobales("variables.log");
    logEvento("Simulacion finalizada");

    vistaCierre(reloj, tablaSistema.procesosTerminados);
    return 0;
}