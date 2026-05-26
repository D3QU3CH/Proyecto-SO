#ifndef VISTA_H
#define VISTA_H

#include "modelo.h"

#define RESET    "\033[0m"
#define ROJO     "\033[31m"
#define VERDE    "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL     "\033[34m"
#define MAGENTA  "\033[35m"
#define CIAN     "\033[36m"
#define NEGRITA  "\033[1m"

void vistaBienvenida(void);
void vistaCierre(int reloj, int terminados);
void vistaMostrarLista(Lista *l, const char *titulo);
void vistaMostrarColaListos(Cola *c);
void vistaMostrarTablaGlobal(void);
void vistaMostrarBuddy(void);
void vistaEstadoES(SistemaES *es);
void vistaMostrarBCP(Proceso *p);
void vistaMostrarPaginacion(Proceso *p);
void vistaMostrarMasRezagados(Cola *c);
void vistaBarrasAprovechamiento(Cola *c, int histDesp[], int histCiclo[], int histIdx);

#endif