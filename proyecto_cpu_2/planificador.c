#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "planificador.h"
#include "es.h"
#include "memoria.h"
#include "control.h"
#include "interfaz.h"
#include "log.h"
#include "sistema.h"
#include "cola.h"

//CANTIDAD PROCESOS E/S
int contarES(SistemaES *es)
{
    int total = 0;
    Nodo *n;

    n = es->disco.frente;
    while (n)
    {
        total++;
        n = n->siguiente;
    }

    n = es->pantalla.frente;
    while (n)
    {
        total++;
        n = n->siguiente;
    }

    n = es->teclado.frente;
    while (n)
    {
        total++;
        n = n->siguiente;
    }

    n = es->impresora.frente;
    while (n)
    {
        total++;
        n = n->siguiente;
    }

    return total;
}

void evaluarColas(int *quantum, Cola *colaEnCiclo, SistemaES *es)
{
    int listos = 0;
    int espera = contarES(es);

    Nodo *actual = colaEnCiclo->frente;
    while (actual != NULL)
    {
        if (actual->proceso->estado == 0)
            listos++;

        actual = actual->siguiente;
    }

    int total = listos + espera;
    if (total == 0)
        return;

    int porcentaje = (listos * 100) / total;

    if (porcentaje > 75)
    {
        *quantum += 5;
        printf("-Aumenta quantum: %d\n", *quantum);
    }
    else if (porcentaje < 25)
    {
        if (*quantum > 5)
        {
            *quantum -= 5;
            printf("-Disminuye quantum: %d\n", *quantum);
        }
    }
    else
    {
        printf("- COLAS BALANCEADAs-\n");
    }
}

Cola colaTerminados;
int totalTerminados = 0;

int historialUso[5] = {0, 0, 0, 0, 0};

void mostrarHistorialCPU()
{
    printf("\n# APROVECHAMIENTO CPU # ");

    for (int i = 0; i < 5; i++)
    {
        int uso = historialUso[i];
        int desperdicio = 100 - uso;

        printf("[");

        for (int j = 0; j < uso / 10; j++)
            printf("#");

        for (int j = 0; j < desperdicio / 10; j++)
            printf("-");

        printf("] %d%% ", uso);
    }

    printf("\n");
}

void pushHistorial(int uso)
{
    for (int i = 0; i < 4; i++)
        historialUso[i] = historialUso[i + 1];

    historialUso[4] = uso;
}

void actualizarEspera(Cola *colaEnCiclo)
{
    Nodo *actual = colaEnCiclo->frente;

    while (actual != NULL)
    {
        if (actual->proceso->estado == 0)
            actual->proceso->tiempoEspera++;

        actual = actual->siguiente;
    }
}

void mostrarBalanceColas(Cola *colaEnCiclo, SistemaES *es)
{
    int listos = 0;

    Nodo *actual = colaEnCiclo->frente;

    while (actual != NULL)
    {
        if (actual->proceso->estado == 0)
            listos++;

        actual = actual->siguiente;
    }

    int espera = contarES(es);

    printf("\n# BALANCE DE COLAS #\n");
    printf("Listos vs Espera %d / %d\n", listos, espera);
}


void ejecutarFCFS(Cola *colaEnCiclo, SistemaES *es, int *algoritmo, int reloj)
{
    if (estaVacia(colaEnCiclo))
        return;

    manejarEntrada(colaEnCiclo, algoritmo);
    actualizarEspera(colaEnCiclo);

    Nodo *n = colaEnCiclo->frente;
    while (n)
    {
        if (n->proceso->esApropiativo)
        {
            moverAlFrente(colaEnCiclo, n->proceso);
            break;
        }
        n = n->siguiente;
    }

    Proceso *p = desencolar(colaEnCiclo);
    if (!p) return;

    p->estado = 1;
    p->vecesEnCPU++;

    if (p->vecesEnCPU == 1)
        p->tiempoRespuesta = reloj - p->tiempoLlegada;

    p->iteraciones++;

    int cambioContexto = rand() % 21 + 10;
    p->cambiosContexto++;
    usleep(cambioContexto * 1000);

    int rafaga = rand() % 61 + 10;
    if (rafaga > p->ciclosRestantes)
        rafaga = p->ciclosRestantes;

    p->rafagaActual = rafaga;

    usleep(20000);

    p->ciclosRestantes -= rafaga;
    p->tiempoEjecucion += rafaga;

    if (p->ciclosRestantes <= 0)
    {
        p->estado = 3;
        p->tiempoRetorno = reloj - p->tiempoLlegada;
        totalTerminados++;
        encolar(&colaTerminados, p);
        printf("- %s TERMINO\n", p->id);
    }
    else
    {
        p->estado = 2;
        int tipo = rand() % 4;

        asignarTiempoES(p, tipo);

        p->bloqueado = 1;
        p->dispositivoES = tipo;

        if (tipo == 0) encolar(&es->disco, p);
        if (tipo == 1) encolar(&es->pantalla, p);
        if (tipo == 2) encolar(&es->teclado, p);
        if (tipo == 3) encolar(&es->impresora, p);

        encolar(colaEnCiclo, p);
    }
}

void ejecutarRR(Cola *colaEnCiclo, Cola *nuevasSolicitudes, SistemaES *es, int *algoritmo, int *quantum, int reloj)
{
    static int contador = 0;

    if (!estaVacia(colaEnCiclo))
    {

        manejarEntrada(colaEnCiclo, algoritmo);

        contador++;

        actualizarEspera(colaEnCiclo);

        //INTENTAR DESBLOQUEAR PROCESOS
        Nodo *n = colaEnCiclo->frente;

        while (n)
        {
            if (n->proceso->estado == 4)
            {

                int r1 = rand() % TAM_MEM;
                int r2 = rand() % TAM_MEM;

                while (r2 == r1)
                    r2 = rand() % TAM_MEM;

                if (usarRecurso(r1) && usarRecurso(r2))
                {

                    n->proceso->estado = 0;
                    n->proceso->bloqueado = 0;
                    n->proceso->enSeccionCritica = 0;

                    liberarRecurso(r1);
                    liberarRecurso(r2);
                }
            }
            n = n->siguiente;
        }

        Nodo *aux = colaEnCiclo->frente;
        while (aux)
        {
            if (aux->proceso->esApropiativo)
            {
                moverAlFrente(colaEnCiclo, aux->proceso);
                break;
            }
            aux = aux->siguiente;
        }

        //SALTAR BLOQUEADOS
        Nodo *actual = colaEnCiclo->frente;
        Nodo *anterior = NULL;

        while (actual != NULL && actual->proceso->estado == 4)
        {
            anterior = actual;
            actual = actual->siguiente;
        }

        if (actual == NULL)
        {
            pushHistorial(0);
            mostrarHistorialCPU();
            return;
        }

        if (actual != colaEnCiclo->frente)
        {
            anterior->siguiente = actual->siguiente;
            actual->siguiente = colaEnCiclo->frente;
            colaEnCiclo->frente = actual;
        }

        Proceso *p = desencolar(colaEnCiclo);
        if (!p)
            return;

        p->estado = 1;
        p->vecesEnCPU++;

        if (p->vecesEnCPU == 1)
            p->tiempoRespuesta = reloj - p->tiempoLlegada;

        p->iteraciones++;

        int r1 = rand() % TAM_MEM;
        int r2 = rand() % TAM_MEM;

        while (r2 == r1)
            r2 = rand() % TAM_MEM;

        int intentos = 0;
        int ok1 = 0, ok2 = 0;

        while (intentos < 5)
        {
            r1 = rand() % TAM_MEM;
            r2 = rand() % TAM_MEM;

            while (r2 == r1)
                r2 = rand() % TAM_MEM;

            ok1 = usarRecurso(r1);
            ok2 = usarRecurso(r2);

            if (ok1 && ok2)
                break;

            if (ok1)
                liberarRecurso(r1);
            if (ok2)
                liberarRecurso(r2);

            intentos++;
        }

        if (!(ok1 && ok2))
        {
            p->estado = 4;
            p->bloqueado = 1;
            encolar(colaEnCiclo, p);
            return;
        }

        if (!ok1 || !ok2)
        {

            p->enSeccionCritica = 1;

            if (ok1)
                liberarRecurso(r1);
            if (ok2)
                liberarRecurso(r2);

            p->estado = 4;
            p->bloqueado = 1;

            pushHistorial(0);

            encolar(colaEnCiclo, p);

            mostrarHistorialCPU();

            return;
        }

        p->variable1 = r1;
        p->variable2 = r2;

        p->enSeccionCritica = 1;

        int cambioContexto = rand() % 21 + 10;
        p->cambiosContexto++;
        usleep(cambioContexto * 1000);

        int rafaga = rand() % 61 + 10;

        int ejecuta = rafaga;

        if (ejecuta > *quantum)
            ejecuta = *quantum;

        if (ejecuta > p->ciclosRestantes)
            ejecuta = p->ciclosRestantes;

        p->rafagaActual = ejecuta;

        usleep(20000);

        p->ciclosRestantes -= ejecuta;
        p->tiempoEjecucion += ejecuta;
        p->tiempoRetorno = reloj - p->tiempoLlegada;

        p->aprovechamiento = (*quantum == 0) ? 0 : (ejecuta * 100) / (*quantum);

        if (ejecuta < *quantum)
            p->desperdicio += (*quantum - ejecuta);

        if (p->variable1 != -1)
            liberarRecurso(p->variable1);
        if (p->variable2 != -1)
            liberarRecurso(p->variable2);

        p->enSeccionCritica = 0;

        if (p->ciclosRestantes <= 0)
        {

            p->restanteQuantum = 0;

            pushHistorial(p->aprovechamiento);

            p->estado = 3;

            totalTerminados++;

            if (p->esApropiativo)
            {
                printf("- PROCESO PRIORITARIO %s FINALIZADO\n", p->id);
                logEvento("Proceso prioritario finalizado");
            }

            encolar(&colaTerminados, p);

            printf("- %s TERMINO\n", p->id);

            mostrarHistorialCPU();
        }
        else
        {

            if (rafaga > *quantum)
            {

                pushHistorial(p->aprovechamiento);

                p->estado = 0;

                encolar(colaEnCiclo, p);

                p->restanteQuantum = rafaga - ejecuta;

                mostrarHistorialCPU();
            }
            else
            {

                pushHistorial(p->aprovechamiento);

                p->estado = 2;

                int tipo = rand() % 4;

                asignarTiempoES(p, tipo);

                p->bloqueado = 1;
                p->dispositivoES = tipo;

                if (tipo == 0)
                    encolar(&es->disco, p);
                if (tipo == 1)
                    encolar(&es->pantalla, p);
                if (tipo == 2)
                    encolar(&es->teclado, p);
                if (tipo == 3)
                    encolar(&es->impresora, p);

                encolar(colaEnCiclo, p);

                mostrarHistorialCPU();
            }
        }

        if (contador % 20 == 0)
        {

            evaluarColas(quantum, colaEnCiclo, es);

            mostrarBalanceColas(colaEnCiclo, es);

            mostrarEnvejecimiento(colaEnCiclo);
            mostrarTopDesperdicio(colaEnCiclo);

            guardarTablaProcesos(colaEnCiclo, nuevasSolicitudes);

            guardarVariablesGlobales(
                colaEnCiclo,
                nuevasSolicitudes,
                *algoritmo,
                *quantum,
                contador,
                0);

            logEvento("Checkpoint RR");
        }
    }
}