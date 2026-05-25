#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"

int main(void)
{
    srand((unsigned)time(NULL));

    // ─────────────────────────────────────────────────────────────────────────
    // 1. INICIALIZACION
    //
    // Estructura de datos segun el enunciado:
    //
    //   tablaProcesos[250]              <- arreglo fijo, nunca pierde procesos
    //   listaProcesosEnEjecucion[150]  <- punteros a los primeros 150 BCPs
    //   listaNuevasSolicitudes[100]    <- punteros a los 100 BCPs restantes
    //   colaListos                     <- la UNICA cola dinamica del planificador
    //
    // Los procesos entran y salen de colaListos pero NUNCA de las listas fijas.
    // Solo cambia su campo `estado` en el BCP.
    // ─────────────────────────────────────────────────────────────────────────

    Cola colaListos;
    inicializarCola(&colaListos);
    inicializarCola(&colaTerminados);

    // Crear los 250 BCPs con IDs, tiempoLlegada y ciclosTotales
    inicializarSistema();

    // Llenar las dos listas fijas y encolar los 150 iniciales en colaListos
    cargarListas(&colaListos);

    SistemaES es;
    inicializarES(&es);
    inicializarMemoria();

    // ─────────────────────────────────────────────────────────────────────────
    // 2. CONFIGURACION INICIAL
    // ─────────────────────────────────────────────────────────────────────────

    vistaMostrarBienvenida();

    int algoritmo;
    printf("  1. FCFS (First Come First Served)\n");
    printf("  2. Round Robin\n");
    printf("  Seleccione algoritmo: ");
    if (scanf("%d", &algoritmo) != 1 || (algoritmo != 1 && algoritmo != 2))
        algoritmo = 1;

    int quantum = 10;
    if (algoritmo == 2)
    {
        printf("  Ingrese el quantum inicial: ");
        if (scanf("%d", &quantum) != 1 || quantum <= 0)
            quantum = 10;
    }

    printf("\n  [OK] Algoritmo: %s", algoritmo == 1 ? "FCFS" : "Round Robin");
    if (algoritmo == 2) printf(" | Quantum: %d", quantum);
    printf("\n\n");

    logEvento("Simulacion iniciada");

    // ─────────────────────────────────────────────────────────────────────────
    // 3. SEMAFOROS Y CONTEXTO COMPARTIDO ENTRE HILOS
    // ─────────────────────────────────────────────────────────────────────────

    int terminado = 0;
    int ciclos    = 0;

    sem_t semDisco, semPantalla, semTeclado, semImpresora;
    sem_init(&semDisco,     0, 0);
    sem_init(&semPantalla,  0, 0);
    sem_init(&semTeclado,   0, 0);
    sem_init(&semImpresora, 0, 0);

    ContextoHilos ctx;
    ctx.colaListos   = &colaListos;
    ctx.es           = &es;
    ctx.algoritmo    = &algoritmo;
    ctx.quantum      = &quantum;
    ctx.ciclos       = &ciclos;
    ctx.terminado    = &terminado;
    ctx.semDisco     = &semDisco;
    ctx.semPantalla  = &semPantalla;
    ctx.semTeclado   = &semTeclado;
    ctx.semImpresora = &semImpresora;

    pthread_mutex_init(&ctx.mutexPrincipal, NULL);
    pthread_mutex_init(&ctx.mutexSocios,    NULL);

    // Argumentos para cada hilo de dispositivo E/S
    // Cada hilo recibe su cola de dispositivo y la colaListos a la que devuelve
    ArgHiloES argDisco     = { &es.disco,     &colaListos,
                                &ctx.mutexPrincipal, &semDisco,
                                &terminado, "Disco" };
    ArgHiloES argPantalla  = { &es.pantalla,  &colaListos,
                                &ctx.mutexPrincipal, &semPantalla,
                                &terminado, "Pantalla" };
    ArgHiloES argTeclado   = { &es.teclado,   &colaListos,
                                &ctx.mutexPrincipal, &semTeclado,
                                &terminado, "Teclado" };
    ArgHiloES argImpresora = { &es.impresora, &colaListos,
                                &ctx.mutexPrincipal, &semImpresora,
                                &terminado, "Impresora" };

    // ─────────────────────────────────────────────────────────────────────────
    // 4. LANZAMIENTO DE HILOS
    //
    // Hilos de E/S: uno por dispositivo, atienden su cola en paralelo
    // Hilo reloj:   limpia bits R cada 50ms y despierta los hilos E/S
    // Hilo teclado: escucha entrada del usuario sin bloquear el ciclo
    // ─────────────────────────────────────────────────────────────────────────

    pthread_t thDisco, thPantalla, thTeclado, thImpresora;
    pthread_t thReloj, thEntrada;

    pthread_create(&thDisco,     NULL, hiloDispositivoES, &argDisco);
    pthread_create(&thPantalla,  NULL, hiloDispositivoES, &argPantalla);
    pthread_create(&thTeclado,   NULL, hiloDispositivoES, &argTeclado);
    pthread_create(&thImpresora, NULL, hiloDispositivoES, &argImpresora);
    pthread_create(&thReloj,     NULL, hiloReloj,          &ctx);
    pthread_create(&thEntrada,   NULL, hiloTeclado,         &ctx);

    // ─────────────────────────────────────────────────────────────────────────
    // 5. CICLO PRINCIPAL (hilo main actua como el planificador de CPU)
    //
    // Condicion de fin: colaListos vacia Y sin procesos en ningun dispositivo E/S
    // Y sin procesos pendientes en listaNuevasSolicitudes.
    // Los procesos terminados (estado=3) siguen en sus listas pero no se encolan.
    // ─────────────────────────────────────────────────────────────────────────

    while (1)
    {
        pthread_mutex_lock(&ctx.mutexPrincipal);

        // Verificar condicion de fin
        int sinPendientes = (contarPendientesNuevas() == 0);
        int sinListos     = estaVacia(&colaListos);
        int sinES         = (contarES(&es) == 0);

        if (sinListos && sinES && sinPendientes)
        {
            pthread_mutex_unlock(&ctx.mutexPrincipal);
            break;
        }

        ciclos++;

        // a) Ingresar procesos de listaNuevasSolicitudes cuyo tiempoLlegada llego
        //    El proceso se encola en colaListos pero no sale de su lista
        ingresarNuevosSegunReloj(&colaListos, ciclos);

        // b) Ejecutar un ciclo del planificador
        if (algoritmo == 1)
            ejecutarFCFS(&colaListos, &es, &algoritmo, ciclos,
                         &ctx.mutexSocios);
        else
            ejecutarRR(&colaListos, &es,
                       &algoritmo, &quantum, ciclos,
                       &ctx.mutexSocios);

        // c) Checkpoint global cada 20 ciclos
        if (ciclos % 20 == 0)
        {
            int antes = algoritmo;
            algoritmo = decidirCambio(&colaListos, algoritmo);
            if (algoritmo != antes)
                vistaMensajeCambioAutomatico(antes, algoritmo);

            vistaMostrarBalanceColas(&colaListos, &es);
            guardarTablaProcesos();     // recorre las listas fijas completas
            guardarVariablesGlobales(&colaListos, algoritmo, quantum, ciclos);
            logEvento("Checkpoint GLOBAL");
        }

        pthread_mutex_unlock(&ctx.mutexPrincipal);
        usleep(1000);   // cede CPU al resto de hilos
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 6. CIERRE DE HILOS
    // ─────────────────────────────────────────────────────────────────────────

    terminado = 1;
    sem_post(&semDisco);
    sem_post(&semPantalla);
    sem_post(&semTeclado);
    sem_post(&semImpresora);

    pthread_join(thDisco,     NULL);
    pthread_join(thPantalla,  NULL);
    pthread_join(thTeclado,   NULL);
    pthread_join(thImpresora, NULL);
    pthread_join(thReloj,     NULL);
    pthread_join(thEntrada,   NULL);

    pthread_mutex_destroy(&ctx.mutexPrincipal);
    pthread_mutex_destroy(&ctx.mutexSocios);
    sem_destroy(&semDisco);
    sem_destroy(&semPantalla);
    sem_destroy(&semTeclado);
    sem_destroy(&semImpresora);

    // ─────────────────────────────────────────────────────────────────────────
    // 7. GUARDADO FINAL
    // Las listas fijas siguen intactas con todos los BCPs, incluso los terminados
    // ─────────────────────────────────────────────────────────────────────────

    guardarTablaProcesos();
    guardarVariablesGlobales(&colaListos, algoritmo, quantum, ciclos);
    logEvento("Simulacion finalizada");

    // Solo liberar los nodos de las colas dinamicas.
    // Los BCPs viven en tablaProcesos[] (stack), no hay malloc de BCPs.
    liberarCola(&colaListos);
    liberarCola(&colaTerminados);

    vistaMostrarCierre(ciclos, totalTerminados);
    return 0;
}