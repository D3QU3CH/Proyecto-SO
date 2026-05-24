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
    // 1. INICIALIZACION DE ESTRUCTURAS
    // ─────────────────────────────────────────────────────────────────────────

    Cola procesosEnCiclo, nuevasSolicitudes;
    inicializarCola(&procesosEnCiclo);
    inicializarCola(&nuevasSolicitudes);
    inicializarCola(&colaTerminados);

    inicializarSistema();                    // crea BCPs + asigna socios
    cargarProcesosEnCola(&procesosEnCiclo, &nuevasSolicitudes);

    SistemaES es;
    inicializarES(&es);
    inicializarMemoria();

    // ─────────────────────────────────────────────────────────────────────────
    // 2. CONFIGURACION INICIAL (usuario elige algoritmo y quantum)
    // ─────────────────────────────────────────────────────────────────────────

    vistaMostrarBienvenida();

    int algoritmo;
    printf("  1. FCFS (First Come First Served)\n");
    printf("  2. Round Robin\n");
    printf("  Seleccione algoritmo: ");
    if (scanf("%d", &algoritmo) != 1 || (algoritmo != 1 && algoritmo != 2))
        algoritmo = 1;

    int quantum = 10;
    if (algoritmo == 2) {
        printf("  Ingrese el quantum: ");
        if (scanf("%d", &quantum) != 1 || quantum <= 0)
            quantum = 10;
    }

    printf("\n  [OK] Algoritmo: %s",
           algoritmo == 1 ? "FCFS" : "Round Robin");
    if (algoritmo == 2) printf(" | Quantum: %d", quantum);
    printf("\n\n");

    logEvento("Simulacion iniciada");

    // ─────────────────────────────────────────────────────────────────────────
    // 3. SEMAFOROS Y CONTEXTO COMPARTIDO
    // ─────────────────────────────────────────────────────────────────────────

    int terminado = 0;
    int ciclos    = 0;

    sem_t semDisco, semPantalla, semTeclado, semImpresora;
    sem_init(&semDisco,     0, 0);
    sem_init(&semPantalla,  0, 0);
    sem_init(&semTeclado,   0, 0);
    sem_init(&semImpresora, 0, 0);

    ContextoHilos ctx;
    ctx.procesosEnCiclo   = &procesosEnCiclo;
    ctx.nuevasSolicitudes = &nuevasSolicitudes;
    ctx.es                = &es;
    ctx.algoritmo         = &algoritmo;
    ctx.quantum           = &quantum;
    ctx.ciclos            = &ciclos;
    ctx.terminado         = &terminado;
    ctx.semDisco          = &semDisco;
    ctx.semPantalla       = &semPantalla;
    ctx.semTeclado        = &semTeclado;
    ctx.semImpresora      = &semImpresora;

    pthread_mutex_init(&ctx.mutexPrincipal, NULL);
    pthread_mutex_init(&ctx.mutexSocios,    NULL);

    // Argumentos para cada hilo de E/S
    ArgHiloES argDisco     = { &es.disco,     &procesosEnCiclo,
                                &ctx.mutexPrincipal, &semDisco,
                                &terminado, "Disco" };
    ArgHiloES argPantalla  = { &es.pantalla,  &procesosEnCiclo,
                                &ctx.mutexPrincipal, &semPantalla,
                                &terminado, "Pantalla" };
    ArgHiloES argTeclado   = { &es.teclado,   &procesosEnCiclo,
                                &ctx.mutexPrincipal, &semTeclado,
                                &terminado, "Teclado" };
    ArgHiloES argImpresora = { &es.impresora, &procesosEnCiclo,
                                &ctx.mutexPrincipal, &semImpresora,
                                &terminado, "Impresora" };

    // ─────────────────────────────────────────────────────────────────────────
    // 4. LANZAMIENTO DE HILOS
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
    // 5. CICLO PRINCIPAL (hilo main = CPU planificador)
    // ─────────────────────────────────────────────────────────────────────────

    while (1) {

        pthread_mutex_lock(&ctx.mutexPrincipal);

        // Condicion de fin: cola vacia y sin procesos en E/S
        if (estaVacia(&procesosEnCiclo) && contarES(&es) == 0) {
            pthread_mutex_unlock(&ctx.mutexPrincipal);
            break;
        }

        ciclos++;

        // a) Ingreso dinamico de procesos segun reloj
        ingresarProcesosNuevos(&procesosEnCiclo, ciclos);

        // b) Ejecutar CPU (hilos de E/S trabajan en paralelo via semaforos)
        if (algoritmo == 1)
            ejecutarFCFS(&procesosEnCiclo, &es, &algoritmo, ciclos,
                         &ctx.mutexSocios);
        else
            ejecutarRR(&procesosEnCiclo, &nuevasSolicitudes,
                       &es, &algoritmo, &quantum, ciclos,
                       &ctx.mutexSocios);

        // c) Checkpoint global cada 20 ciclos
        if (ciclos % 20 == 0) {
            int antes = algoritmo;
            algoritmo = decidirCambio(&procesosEnCiclo, algoritmo);
            if (algoritmo != antes)
                vistaMensajeCambioAutomatico(antes, algoritmo);

            vistaMostrarBalanceColas(&procesosEnCiclo, &es);
            guardarTablaProcesos(&procesosEnCiclo, &nuevasSolicitudes);
            guardarVariablesGlobales(&procesosEnCiclo, &nuevasSolicitudes,
                                     algoritmo, quantum, ciclos,
                                     TOTAL_PROCESOS - EN_SISTEMA);
            logEvento("Checkpoint GLOBAL");
        }

        pthread_mutex_unlock(&ctx.mutexPrincipal);

        usleep(1000);   // cede CPU para que los otros hilos avancen
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 6. CIERRE DE HILOS
    // ─────────────────────────────────────────────────────────────────────────

    terminado = 1;

    // Despertar hilos bloqueados en semaforos para que terminen
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
    // 7. GUARDADO FINAL Y LIBERACION
    // ─────────────────────────────────────────────────────────────────────────

    guardarTablaProcesos(&procesosEnCiclo, &nuevasSolicitudes);
    guardarVariablesGlobales(&procesosEnCiclo, &nuevasSolicitudes,
                              algoritmo, quantum, ciclos, 0);
    logEvento("Simulacion finalizada");

    liberarCola(&procesosEnCiclo);
    liberarCola(&nuevasSolicitudes);
    liberarCola(&colaTerminados);

    vistaMostrarCierre(ciclos, totalTerminados);
    return 0;
}