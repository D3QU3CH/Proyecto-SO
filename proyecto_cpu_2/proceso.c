#include <stdio.h>
#include <stdlib.h>
#include "proceso.h"

void inicializarProceso(Proceso* p, int index) {

    sprintf(p->id, "%c-%d", 'A' + (index % 26), index);

    p->tiempoLlegada = 0;

    p->ciclosTotales = rand() % 85001;
    p->ciclosRestantes = p->ciclosTotales;

    p->estado = 0;

    p->rafagaActual = 0;
    p->tiempoEspera = 0;
    p->tiempoEjecucion = 0;

    p->tipoProceso = 0; //(0 INTENSIVO EN CPU Y 1 INTESIVO EN E/S)
    p->desperdicio = 0;

    p->vecesEnCPU = 0;
    p->esApropiativo = 0;

    p->dispositivoES = -1;
    p->tiempoES = 0;

    p->bloqueado = 0;

    p->tiempoRespuesta = 0;
    p->tiempoRetorno = 0;

    p->iteraciones = 0;

    p->aprovechamiento = 0;
    p->usoMemoria = rand() % 100;

    p->variable1 = -1;
    p->variable2 = -1;

    p->enSeccionCritica = 0;

    p->restanteQuantum = 0;

    p->cambiosContexto = 0;
}