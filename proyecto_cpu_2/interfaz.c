#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#include "interfaz.h"
#include "control.h"
#include "log.h"

//DETECTAR TECLA SIN BLOQUEO
int hayTecla() {
    struct termios oldt, newt;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    int c = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (c != EOF) {
        ungetc(c, stdin);
        return 1;
    }

    return 0;
}

//LEER TECLA 
char leerTecla() {
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char c = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return c;
}

//MANEJO DE ENTRADA 
void manejarEntrada(Cola* colaListos, int* algoritmo) {

    if (hayTecla()) {

        char tecla = leerTecla();

        //CAMBIO DE ALGORITMO
        if (tecla == 'x' || tecla == 'X') {

            int opcion;

            printf("\n- CAMBIO DE ALGORITMO -\n");
            printf("1. FCFS\n2. Round Robin\nSeleccione: ");
            scanf("%d", &opcion);

            if(opcion == 1 || opcion == 2){
                *algoritmo = opcion;
                printf("-NUEVO ALGORITMO: %s\n", opcion == 1 ? "FCFS" : "RR");
                logEvento("Cambio manual de algoritmo");
            }
        }

        //APROPIATIVIDAD
        if (tecla == 'a' || tecla == 'A') {

            Proceso* critico = seleccionarProcesoCritico(colaListos);

            if (critico != NULL) {
                moverAlFrente(colaListos, critico);
                printf("\n- PROCESO %s MOVIDO AL FRENTE\n", critico->id);
                logEvento("Proceso movido al frente");
            }
        }
    }
}