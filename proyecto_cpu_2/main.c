#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "modelo.h"
#include "vista.h"
#include "controlador.h"

int main(void)
{
    srand((unsigned)time(NULL));


    // 1. INICIALIZACION

    Cola procesosEnCiclo,  nuevasSolicitudes;
    inicializarCola(&procesosEnCiclo);
    inicializarCola(&nuevasSolicitudes);
    inicializarCola(&colaTerminados);

    inicializarSistema();
    cargarProcesosEnCola(&procesosEnCiclo, &nuevasSolicitudes);

    SistemaES es;
    inicializarES(&es);
    inicializarMemoria();

    // 2. CONFIGURACION INICIAL (usuario elige algoritmo y quantum)

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

    // 3. CICLO PRINCIPAL

    int ciclos = 0;

    while (!estaVacia(&procesosEnCiclo))
    {
        ciclos++;

        // a) Ingreso dinamico 
        ingresarProcesosNuevos(&procesosEnCiclo, ciclos);

        // b) Procesar E/S 
        procesarES(&es, &procesosEnCiclo);

        // c) Ejecutar CPU
        if (algoritmo == 1)
            ejecutarFCFS(&procesosEnCiclo, &es, &algoritmo, ciclos);
        else
            ejecutarRR(&procesosEnCiclo, &nuevasSolicitudes,
                       &es, &algoritmo, &quantum, ciclos);

        // d) Checkpoint global cada 20 ciclos
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

        // e) Teclado (sin bloquear)
        manejarEntrada(&procesosEnCiclo, &algoritmo, quantum, totalTerminados);
    }

    // 4. CIERRE

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