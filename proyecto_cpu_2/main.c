#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cola.h"
#include "sistema.h"
#include "planificador.h"
#include "es.h"
#include "memoria.h"
#include "control.h"
#include "log.h"
#include "interfaz.h"

int main()
{
    srand(time(NULL));

    //INICIALIZACION

    Cola procesosEnCiclo;
    inicializarCola(&procesosEnCiclo);

    Cola nuevasSolicitudes;
    inicializarCola(&nuevasSolicitudes);

    inicializarSistema();

    cargarProcesosEnCola(&procesosEnCiclo, &nuevasSolicitudes);

    SistemaES es;
    inicializarES(&es);

    extern Cola colaTerminados;
    inicializarCola(&colaTerminados);

    inicializarMemoria();

    int algoritmo;
    printf("\n1. FCFS\n2. Round Robin\nSeleccione: ");
    scanf("%d", &algoritmo);

    if (algoritmo != 1 && algoritmo != 2)
        algoritmo = 1;

    int quantum;
    printf("Ingrese quantum: ");
    scanf("%d", &quantum);

    if (quantum <= 0)
        quantum = 10;

    int ciclos = 0;

    //CICLO DEL SISTEMA 

    while (!estaVacia(&procesosEnCiclo))
    {
        ciclos++;

        // 1. Ingreso dinamico
        ingresarProcesosNuevos(&procesosEnCiclo, ciclos);

        // 2. ES
        procesarES(&es, &procesosEnCiclo);

        // 3. CPU
        if (algoritmo == 1)
        {
            ejecutarFCFS(&procesosEnCiclo, &es, &algoritmo, ciclos);
        }
        else
        {
    
            ejecutarRR(&procesosEnCiclo, &nuevasSolicitudes, &es, &algoritmo, &quantum, ciclos);
        }

        // 4. Metricas cada 20 ciclos
        if (ciclos % 20 == 0)
        {
            algoritmo = decidirCambio(&procesosEnCiclo, algoritmo);

            mostrarBalanceColas(&procesosEnCiclo, &es);

            guardarTablaProcesos(&procesosEnCiclo, &nuevasSolicitudes);

            guardarVariablesGlobales(
                &procesosEnCiclo,
                &nuevasSolicitudes,
                algoritmo,
                quantum,
                ciclos,
                TOTAL_PROCESOS - EN_SISTEMA
            );

            logEvento("Checkpoint GLOBAL");
        }
    }

    logEvento("Simulacion finalizada");

    printf("\nSIMULACION FINALIZADA\n");

    liberarCola(&procesosEnCiclo);
    liberarCola(&colaTerminados);

    return 0;
}