#include <stdio.h>
#include <string.h>
#include "control.h"
#include "log.h"

Proceso *seleccionarProcesoCritico(Cola *procesosEnCiclo)
{

    if (estaVacia(procesosEnCiclo))
    {
        printf("No hay procesos en la cola de listos.\n");
        return NULL;
    }

    for (Nodo *n = procesosEnCiclo->frente; n != NULL; n = n->siguiente)
    {
        n->proceso->esApropiativo = 0;
    }

    Proceso *top5[5] = {NULL, NULL, NULL, NULL, NULL};
    int encontrados = 0;

    for (Nodo *n = procesosEnCiclo->frente; n != NULL; n = n->siguiente)
    {

        Proceso *p = n->proceso;

        int yaEsta = 0;

        for (int i = 0; i < encontrados; i++)
        {
            if (top5[i] == p)
            {
                yaEsta = 1;
                break;
            }
        }

        if (yaEsta)
        {
            continue;
        }

        if (encontrados < 5)
        {

            top5[encontrados] = p;
            encontrados++;

            for (int i = encontrados - 1; i > 0; i--)
            {
                if (top5[i]->ciclosRestantes > top5[i - 1]->ciclosRestantes)
                {
                    Proceso *temp = top5[i];
                    top5[i] = top5[i - 1];
                    top5[i - 1] = temp;
                }
                else
                {
                    break;
                }
            }
        }
        else if (p->ciclosRestantes > top5[4]->ciclosRestantes)
        {

            top5[4] = p;

            for (int i = 4; i > 0; i--)
            {
                if (top5[i]->ciclosRestantes > top5[i - 1]->ciclosRestantes)
                {
                    Proceso *temp = top5[i];
                    top5[i] = top5[i - 1];
                    top5[i - 1] = temp;
                }
                else
                {
                    break;
                }
            }
        }
    }

    printf("\n-5 PROCESOS MAS REZAGADOS (mas ciclos por ejecutar) -\n");

    for (int i = 0; i < encontrados; i++)
    {
        printf("  %d. ID: %-8s | Ciclos restantes: %d | Veces en CPU: %d\n", i + 1, top5[i]->id, top5[i]->ciclosRestantes, top5[i]->vecesEnCPU);
    }

    char idElegido[20];

    printf("\nIngrese el ID del proceso a privilegiar: ");
    scanf("%s", idElegido);

    for (int i = 0; i < encontrados; i++)
    {
        if (strcmp(top5[i]->id, idElegido) == 0)
        {

            // LIMPIAR anteriores
            for (Nodo *n = procesosEnCiclo->frente; n != NULL; n = n->siguiente)
            {
                n->proceso->esApropiativo = 0;
            }

            top5[i]->esApropiativo = 1;

            printf("[OK] Proceso %s sera privilegiado hasta que termine.\n", top5[i]->id);
            logEvento("Proceso privilegiado por apropiatividad");
            return top5[i];
        }
    }

    printf("ID no encontrado en la lista.\n");
    return NULL;
}


void moverAlFrente(Cola *cola, Proceso *p)
{

    if (cola->frente == NULL || p == NULL)
    {
        return;
    }

    if (cola->frente->proceso == p)
    {
        return;
    }

    Nodo *anterior = NULL;
    Nodo *actual = cola->frente;

    while (actual != NULL && actual->proceso != p)
    {
        anterior = actual;
        actual = actual->siguiente;
    }

    if (actual == NULL)
    {
        return;
    }

    anterior->siguiente = actual->siguiente;

    if (actual == cola->final)
    {
        cola->final = anterior;
    }

    actual->siguiente = cola->frente;
    cola->frente = actual;

    printf("[OK] Proceso %s movido al frente de la cola de listos.\n", p->id);
}

int decidirCambio(Cola *colaListos, int algoritmoActual)
{

    if (estaVacia(colaListos))
    {
        return algoritmoActual;
    }

    //TABLA (2 porque algoritmoActual ya cuenta)
    int esperaTotal = 0;      // (Para la variable de la tabla EsperaPromedio)
    int desperdicioTotal = 0; // (Para la variable de la tabla DesperdicioProm)

    //BCP (5 reales)
    int totalCiclosRestantes = 0; // (Para la variable del BCP ciclosRestantes)
    int totalRafaga = 0;          // (Para la variable del BCP rafagaActual)
    int totalVecesCPU = 0;        // (Para la variable del BCP vecesEnCPU)
    int totalBloqueado = 0;       // (Para la variable del BCP bloqueado)
    int totalQuantum = 0;         // (Para la variable del BCP restanteQuantum)

    int totalProcesos = 0;

    for (Nodo *n = colaListos->frente; n != NULL; n = n->siguiente)
    {

        Proceso *p = n->proceso;

        //TABLA
        esperaTotal += p->tiempoEspera;
        desperdicioTotal += p->desperdicio;

        // BCP 
        totalCiclosRestantes += p->ciclosRestantes;
        totalRafaga += p->rafagaActual;
        totalVecesCPU += p->vecesEnCPU;
        totalBloqueado += p->bloqueado;
        totalQuantum += p->restanteQuantum;

        totalProcesos++;
    }

    if (totalProcesos == 0)
    {
        return algoritmoActual;
    }

    //PROMEDIOS
    int promEspera = esperaTotal / totalProcesos;
    int promDesperdicio = desperdicioTotal / totalProcesos;

    int promCiclos = totalCiclosRestantes / totalProcesos;
    int promRafaga = totalRafaga / totalProcesos;
    int promVecesCPU = totalVecesCPU / totalProcesos;
    int promBloqueado = totalBloqueado / totalProcesos;
    int promQuantum = totalQuantum / totalProcesos;

    //FCFS a RR 2 tabla + 5 BCP + algoritmo
    if (algoritmoActual == 1 &&promEspera > 120 &&     promDesperdicio > 25 && promCiclos > 8000 &&   promRafaga > 50 &&      promVecesCPU < 5 &&      promBloqueado > 1 &&    promQuantum < 15){ 

        printf("\n[AUTO] FCFS -> RR | E:%d D:%d C:%d R:%d V:%d B:%d Q:%d\n",
               promEspera, promDesperdicio,
               promCiclos, promRafaga,
               promVecesCPU, promBloqueado, promQuantum);

        logEvento("Cambio automatico FCFS -> RR");
        return 2;
    }

    //RR a FCFS
    if (algoritmoActual == 2 &&promEspera < 80 &&  promDesperdicio < 20 && promCiclos < 6000 &&  promRafaga < 40 && promVecesCPU >= 5 && promBloqueado == 0 && promQuantum > 5){

        printf("\n[AUTO] RR -> FCFS | E:%d D:%d C:%d R:%d V:%d B:%d Q:%d\n",
               promEspera, promDesperdicio,
               promCiclos, promRafaga,
               promVecesCPU, promBloqueado, promQuantum);

        logEvento("Cambio automatico RR -> FCFS");
        return 1;
    }

    return algoritmoActual;
}