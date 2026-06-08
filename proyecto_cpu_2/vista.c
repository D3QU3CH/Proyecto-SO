#include <stdio.h>
#include "vista.h"

// Muestra la tabla con las variables globales del sistema
void vistaMostrarTablaGlobal(void)
{
    TablaProcesos *t = &tablaSistema;
    const char *alg  = (t->algoritmoActual == ALG_FCFS) ? "FCFS" : "RR";

    printf("\n--- VARIABLES GLOBALES (ciclo %d) ---n", t->cicloActual);
    printf("  Total procesos : %d\n",   t->totalProcesos);
    printf("  Cola listos    : %d\n",   t->procesosEnColaListos);
    printf("  Ejecutando     : %d\n",   t->procesosEjecutando);
    printf("  En E/S         : %d\n",   t->procesosEnES);
    printf("  Terminados     : %d\n",   t->procesosTerminados);
    printf("  Solicitudes    : %d\n",   t->procesosEnSolicitud);
    printf("  Algoritmo      : %s\n",   alg);
    printf("  Quantum        : %d\n",   t->quantumActual);
    printf("  Cambios ctx    : %d\n",   t->totalCambiosContexto);
    printf("  Fallos pagina  : %d\n",   t->totalFallosPagina);
    printf("  Prom. espera   : %d\n",   t->promedioEspera);
    printf("  Mem. libre KB  : %d\n",   t->memoriaLibreKB);
    printf("  Desperdicio KB : %d\n",   t->desperdicioTotal);
    printf("----------------------------------------------------\n");
}

// Muestra los 5 procesos con mas ciclos restantes (los mas rezagados)
void vistaMostrarMasRezagados(Cola *c)
{
    Proceso *top[5] = {NULL};

    printf("\n--- 5 PROCESOS MAS REZAGADOS ---\n");

    for (NodoCola *n = c->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;

        for (int i = 0; i < 5; i++) {
            // Si el proceso tiene mas ciclos restantes que el que esta en esta posicion
            if (!top[i] || p->ciclosRestantes > top[i]->ciclosRestantes) {
                // Correr los demas hacia abajo para insertar aqui
                for (int j = 4; j > i; j--)
                    top[j] = top[j - 1];
                top[i] = p;
                break;
            }
        }
    }

    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | ciclosRest: %6d | espera: %4d | vecesEnCPU: %3d\n",
               i + 1, top[i]->id, top[i]->ciclosRestantes,
               top[i]->tiempoEspera, top[i]->vecesEnCPU);

    if (!top[0])
        printf("  (cola vacia)\n");
}

// Calcula el porcentaje de aprovechamiento promedio de los procesos en la cola
static int calcularAprovechamientoActual(Cola *c)
{
    int totalQ     = 0;
    int totalUsado = 0;
    int q          = tablaSistema.quantumActual;

    for (NodoCola *n = c->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;

        // El proceso usa su rafaga completa o el quantum, lo que sea menor
        int usado = (p->rafagaActual < q) ? p->rafagaActual : q;

        totalUsado += usado;
        totalQ     += q;
    }

    if (totalQ == 0)
        return 0;

    return (totalUsado * 100) / totalQ;
}

// Imprime una sola barra de aprovechamiento
static void imprimirBarra(int ciclo, int aprov, int esActual)
{
    int llenos = aprov / 5; // cada bloque representa 5%

    printf("  Ciclo %4d | Aprov: %3d%% [", ciclo, aprov);

    for (int b = 0; b < 20; b++)
        printf("%c", b < llenos ? '#' : '.');

    if (esActual)
        printf("] Desp: %3d%% <-- ACTUAL\n", 100 - aprov);
    else
        printf("] Desp: %3d%%\n", 100 - aprov);
}

// Muestra 4 barras historicas y 1 barra del estado actual
void vistaBarrasAprovechamiento(Cola *c, int histDesp[], int histCiclo[], int histIdx)
{
    int aprovActual = calcularAprovechamientoActual(c);
    int cicloActual = tablaSistema.cicloActual;

    printf("\n--- APROVECHAMIENTO CPU (RR, Q=%d) ---\n", tablaSistema.quantumActual);

    // Recorrer el historial circular desde la entrada mas reciente hacia atras
    int mostrados = 0;
    for (int i = 1; i <= 100 && mostrados < 4; i++) {
        int idx = (histIdx - i + 100) % 100;

        // Saltar si la entrada esta vacia o es del ciclo actual
        if (histCiclo[idx] == 0 || histCiclo[idx] == cicloActual)
            continue;

        imprimirBarra(histCiclo[idx], 100 - histDesp[idx], 0);
        mostrados++;
    }

    // Barra actual al final
    imprimirBarra(cicloActual, aprovActual, 1);
}

// Muestra cuantos procesos hay en cada cola de E/S
void vistaEstadoES(SistemaES *es)
{
    printf("\n--- ESTADO E/S ---\n");
    printf("  Disco     (x2) : %d proceso(s)\n", es->disco.tamanio);
    printf("  Pantalla  (x4) : %d proceso(s)\n", es->pantalla.tamanio);
    printf("  Teclado   (x8) : %d proceso(s)\n", es->teclado.tamanio);
    printf("  Impresora(x12) : %d proceso(s)\n", es->impresora.tamanio);
}

// Pantalla inicial al arrancar el simulador
void vistaBienvenida(void)
{
    printf("\n--- SIMULADOR CPU-MEMORIA-DISTRIBUCION ---\n");
    printf("  Proyecto III - Sistemas Operativos\n");
    printf("  Buddy System + Paginacion NRU\n");
    printf("  %d procesos | %d en ciclo | %d en solicitudes\n",
           TOTAL_PROCESOS, PROCESOS_EN_CICLO, PROCESOS_EN_SOLICITUD);
    printf("  Algoritmo inicial: FCFS\n");
    printf("\n  Teclas:\n");
    printf("    [X] Cambiar algoritmo (FCFS <-> RR)\n");
    printf("    [A] Aplicar apropiatividad a un proceso\n");
    printf("    [Q] Salir\n");
    printf("------------------------------------------------\n\n");
}

// Pantalla final al terminar la simulacion
void vistaCierre(int reloj, int terminados)
{
    printf("\n--- SIMULACION FINALIZADA ---\n");
    printf("  Ciclos totales     : %d\n", reloj);
    printf("  Procesos terminados: %d\n", terminados);
    printf("--------------------------------\n\n");
}