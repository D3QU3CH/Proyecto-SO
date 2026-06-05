#ifndef SLAVE_H
#define SLAVE_H

#include "master.h"

// El slave es un ejecutable independiente; no expone funciones al main.
// Este header queda para incluirlo si se compila de forma modular.

void ejecutarSlave(void);

#endif