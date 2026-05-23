#include <stdio.h>
#include <time.h>
#include "log.h"
#include "sistema.h"

//CARGAR BCP (25 variables)

void guardarTablaProcesos(Cola *procesosCiclo, Cola *nuevasSolicitudes)
{

    FILE *f = fopen("tabla_procesos.txt", "w");
    if (!f)
        return;

    fprintf(f,
            "%-10s %-15s %-15s %-18s %-10s %-15s %-15s %-18s %-15s %-15s %-15s %-18s %-15s %-15s %-12s %-18s %-18s %-15s %-12s %-12s %-12s %-12s %-20s %-18s %-18s\n",
            "ID",
            "TiempoLlegada",
            "CiclosTotales",
            "CiclosRestantes",
            "Estado",
            "RafagaActual",
            "TiempoEspera",
            "TiempoEjecucion",
            "TipoProceso",
            "Desperdicio",
            "VecesEnCPU",
            "EsApropiativo",
            "DispositivoES",
            "TiempoES",
            "Bloqueado",
            "TiempoRespuesta",
            "TiempoRetorno",
            "Iteraciones",
            "UsoCPU",
            "UsoMemoria",
            "Variable1",
            "Variable2",
            "EnSeccionCritica",
            "RestanteQuantum",
            "CambiosContexto");

    fprintf(f,
            "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

    //PRIMERO PROCESOS EN EJECUCION (PCP)
    for (Nodo *n = procesosCiclo->frente; n != NULL; n = n->siguiente)
    {

        Proceso *p = n->proceso;

        fprintf(f,
                "%-10s %-15d %-15d %-18d %-10d %-15d %-15d %-18d %-15d %-15d %-15d %-18d %-15d %-15d %-12d %-18d %-18d %-15d %-12d %-12d %-12d %-12d %-20d %-18d %-18d\n",

                p->id,
                p->tiempoLlegada,
                p->ciclosTotales,
                p->ciclosRestantes,
                p->estado,
                p->rafagaActual,
                p->tiempoEspera,
                p->tiempoEjecucion,
                p->tipoProceso,
                p->desperdicio,
                p->vecesEnCPU,
                p->esApropiativo,
                p->dispositivoES,
                p->tiempoES,
                p->bloqueado,
                p->tiempoRespuesta,
                p->tiempoRetorno,
                p->iteraciones,
                p->aprovechamiento,
                p->usoMemoria,
                p->variable1,
                p->variable2,
                p->enSeccionCritica,
                p->restanteQuantum,
                p->cambiosContexto);
    }

    //SEGUNDO NUEVAS SOLICITUDES (PLP)
    for (Nodo *n = nuevasSolicitudes->frente; n != NULL; n = n->siguiente)
    {

        Proceso *p = n->proceso;

        fprintf(f,
                "%-10s %-15d %-15d %-18d %-10d %-15d %-15d %-18d %-15d %-15d %-15d %-18d %-15d %-15d %-12d %-18d %-18d %-15d %-12d %-12d %-12d %-12d %-20d %-18d %-18d\n",

                p->id,
                p->tiempoLlegada,
                p->ciclosTotales,
                p->ciclosRestantes,
                p->estado,
                p->rafagaActual,
                p->tiempoEspera,
                p->tiempoEjecucion,
                p->tipoProceso,
                p->desperdicio,
                p->vecesEnCPU,
                p->esApropiativo,
                p->dispositivoES,
                p->tiempoES,
                p->bloqueado,
                p->tiempoRespuesta,
                p->tiempoRetorno,
                p->iteraciones,
                p->aprovechamiento,
                p->usoMemoria,
                p->variable1,
                p->variable2,
                p->enSeccionCritica,
                p->restanteQuantum,
                p->cambiosContexto);
    }

    fclose(f);
}

void guardarVariablesGlobales(Cola *procesosCiclo, Cola *nuevasSolicitudes,int algoritmo, int quantum, int iteracionCPU, int procesosNuevos)
{

    FILE *f = fopen("variables_tabla.txt", "w");
    if (!f)
        return;

    int activos = 0, listos = 0, bloqueados = 0, terminados = 0;
    int esperaTotal = 0, desperdicioTotal = 0, retornoTotal = 0, cpuTotal = 0;
    int cola1 = 0, cola2 = 0, cola3 = 0, cola4 = 0;
    int memoria = 0, cambios = 0;

    //PROCESOS EN EJECUCION (PCP)
    for (Nodo *n = procesosCiclo->frente; n != NULL; n = n->siguiente)
    {

        Proceso *p = n->proceso;

        activos++;

        if (p->estado == 0)
            listos++;
        if (p->estado == 2)
            bloqueados++;
        if (p->estado == 3)
            terminados++;

        esperaTotal += p->tiempoEspera;
        desperdicioTotal += p->desperdicio;
        retornoTotal += p->tiempoRetorno;
        cpuTotal += p->tiempoEjecucion;

        memoria += p->usoMemoria;
        cambios += p->cambiosContexto;

        if (p->dispositivoES == 0)
            cola1++;
        if (p->dispositivoES == 1)
            cola2++;
        if (p->dispositivoES == 2)
            cola3++;
        if (p->dispositivoES == 3)
            cola4++;
    }

    //NUEVAS SOLICITUDES (PLP)
    for (Nodo *n = nuevasSolicitudes->frente; n != NULL; n = n->siguiente)
    {

        Proceso *p = n->proceso;

        activos++; // tambien cuentan en el sistema

        memoria += p->usoMemoria;
    }

    int esperaProm = activos ? esperaTotal / activos : 0;
    int desperdicioProm = activos ? desperdicioTotal / activos : 0;
    int retornoProm = activos ? retornoTotal / activos : 0;
    int cpuProm = activos ? cpuTotal / activos : 0;

    fprintf(f, "Algoritmo %d\n", algoritmo);
    fprintf(f, "Quantum %d\n", quantum);
    fprintf(f, "ProcesosActivos %d\n", activos);

    extern int totalTerminados;
    fprintf(f, "ProcesosTerminados %d\n", totalTerminados);

    fprintf(f, "ProcesosBloqueados %d\n", bloqueados);
    fprintf(f, "ProcesosListos %d\n", listos);
    fprintf(f, "CPU_Promedio %d\n", cpuProm);
    fprintf(f, "DesperdicioProm %d\n", desperdicioProm);
    fprintf(f, "EsperaPromedio %d\n", esperaProm);
    fprintf(f, "RetornoPromedio %d\n", retornoProm);
    fprintf(f, "CambiosContexto %d\n", cambios);
    fprintf(f, "IteracionCPU %d\n", iteracionCPU);
    fprintf(f, "ColaES1 %d\n", cola1);
    fprintf(f, "ColaES2 %d\n", cola2);
    fprintf(f, "ColaES3 %d\n", cola3);
    fprintf(f, "ColaES4 %d\n", cola4);
    fprintf(f, "MemoriaUsada %d\n", memoria);
    fprintf(f, "MemoriaLibre %d\n", 10000 - memoria);
    fprintf(f, "BalanceColas %d\n", listos - bloqueados);
    fprintf(f, "ProcesosNuevos %d\n", procesosNuevos);

    fclose(f);
}

//LOG EVENTOS

void logEvento(const char *msg)
{

    FILE *f = fopen("eventos.log", "a");
    if (!f)
        return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    fprintf(f, "[%02d:%02d:%02d] %s\n",
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec,
            msg);

    fclose(f);
}

//TOP 5 GENERICO 

static void obtenerTop5(Cola *cola, int campo, Proceso *top[5], int *n)
{
    *n = 0;

    for (Nodo *a = cola->frente; a != NULL; a = a->siguiente)
    {
        Proceso *p = a->proceso;

        int repetido = 0;
        for (int i = 0; i < *n; i++)
        {
            if (top[i] == p)
            {
                repetido = 1;
                break;
            }
        }
        if (repetido) continue;

        int valor = 0;

        if (campo == 1) valor = p->tiempoEspera;
        if (campo == 2) valor = p->vecesEnCPU;
        if (campo == 3) valor = p->desperdicio;

        if (*n < 5)
        {
            top[*n] = p;
            (*n)++;
        }
        else
        {
            int menor = 0;

            for (int i = 1; i < 5; i++)
            {
                int v1 = 0, v2 = 0;

                if (campo == 1)
                {
                    v1 = top[i]->tiempoEspera;
                    v2 = top[menor]->tiempoEspera;
                }
                if (campo == 2)
                {
                    v1 = top[i]->vecesEnCPU;
                    v2 = top[menor]->vecesEnCPU;
                }
                if (campo == 3)
                {
                    v1 = top[i]->desperdicio;
                    v2 = top[menor]->desperdicio;
                }

                if (v1 < v2)
                    menor = i;
            }

            int menorValor = 0;

            if (campo == 1) menorValor = top[menor]->tiempoEspera;
            if (campo == 2) menorValor = top[menor]->vecesEnCPU;
            if (campo == 3) menorValor = top[menor]->desperdicio;

            if (valor > menorValor)
                top[menor] = p;
        }
    }

    // ORDENAR DE MAYOR A MENOR (BURBUJA SIMPLE)
    for (int i = 0; i < *n - 1; i++)
    {
        for (int j = i + 1; j < *n; j++)
        {
            int v1 = 0, v2 = 0;

            if (campo == 1)
            {
                v1 = top[i]->tiempoEspera;
                v2 = top[j]->tiempoEspera;
            }
            if (campo == 2)
            {
                v1 = top[i]->vecesEnCPU;
                v2 = top[j]->vecesEnCPU;
            }
            if (campo == 3)
            {
                v1 = top[i]->desperdicio;
                v2 = top[j]->desperdicio;
            }

            if (v2 > v1)
            {
                Proceso *tmp = top[i];
                top[i] = top[j];
                top[j] = tmp;
            }
        }
    }
}

//DESPERDICIO

void mostrarTopDesperdicio(Cola *cola)
{

    Proceso *top[5];
    int n;

    obtenerTop5(cola, 3, top, &n);

    printf("\n- TOP 5 DESPERDICIO CPU -\n");

    for (int i = 0; i < n; i++)
        printf(" %d. %s | Desperdicio: %d\n",
               i + 1,
               top[i]->id,
               top[i]->desperdicio);
}

//ENVEJECIMIENTO

void mostrarEnvejecimiento(Cola *cola)
{

    Proceso *top[5];
    int n;

    obtenerTop5(cola, 2, top, &n);

    printf("\n- TOP 5 ENVEJECIMIENTO -\n");

    for (int i = 0; i < n; i++)
        printf(" %d. %s | VecesCPU: %d | Rest: %d\n",
               i + 1,
               top[i]->id,
               top[i]->vecesEnCPU,
               top[i]->ciclosRestantes);
}