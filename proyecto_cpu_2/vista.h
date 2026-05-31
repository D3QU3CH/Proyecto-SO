#ifndef VISTA_H
#define VISTA_H

#include "modelo.h"

void vistaBienvenida(void);
void vistaCierre(int reloj, int terminados);
void vistaMostrarTablaGlobal(void);
void vistaMostrarMasRezagados(Cola *c);
void vistaBarrasAprovechamiento(Cola *c, int histDesp[], int histCiclo[], int histIdx);
void vistaEstadoES(SistemaES *es);

#endif