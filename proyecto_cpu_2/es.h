#ifndef ES_H
#define ES_H

#include "cola.h"

typedef struct
{
    Cola disco;
    Cola pantalla;
    Cola teclado;
    Cola impresora;
} SistemaES;

void inicializarES(SistemaES *es);
void procesarES(SistemaES *es, Cola *colaListos);
void asignarTiempoES(Proceso *p, int tipo);

#endif