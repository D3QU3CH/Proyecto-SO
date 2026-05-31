#include <stdio.h>
#include "vista.h"

static const char *nomEstado(int e)
{
    switch (e) {
        case ESTADO_LISTO:      return "LISTO";
        case ESTADO_EJECUTANDO: return "EJECUTANDO";
        case ESTADO_ESPERA_ES:  return "ESPERA_ES";
        case ESTADO_TERMINADO:  return "TERMINADO";
        default:                return "?";
    }
}

static const char *nomDispES(int d)
{
    switch (d) {
        case 0: return "Disco";
        case 1: return "Pantalla";
        case 2: return "Teclado";
        case 3: return "Impresora";
        default: return "Ninguno";
    }
}

void vistaMostrarTablaGlobal(void)
{
    TablaProcesos *t = &tablaSistema;
    printf("\n--- VARIABLES GLOBALES (ciclo %d) ---\n", t->cicloActual);
    printf(" Total          : %d\n", t->totalProcesos);
    printf(" Cola listos    : %d\n", t->procesosEnColaListos);
    printf(" Ejecutando     : %d\n", t->procesosEjecutando);
    printf(" En E/S         : %d\n", t->procesosEnES);
    printf(" Terminados     : %d\n", t->procesosTerminados);
    printf(" Solicitudes    : %d\n", t->procesosEnSolicitud);
    printf(" Algoritmo      : %s\n", t->algoritmoActual == ALG_FCFS ? "FCFS" : "RR");
    printf(" Quantum        : %d\n", t->quantumActual);
    printf(" Cambios ctx    : %d\n", t->totalCambiosContexto);
    printf(" Fallos pagina  : %d\n", t->totalFallosPagina);
    printf(" Prom. espera   : %d\n", t->promedioEspera);
    printf(" Mem. libre KB  : %d\n", t->memoriaLibreKB);
    printf(" Desperdicio KB : %d\n", t->desperdicioTotal);
}

// 5 procesos con mas ciclos restantes (los mas rezagados)
void vistaMostrarMasRezagados(Cola *c)
{
    printf("\n--- 5 PROCESOS MAS REZAGADOS ---\n");
    Proceso *top[5] = {NULL};
    for (NodoCola *n = c->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        for (int i = 0; i < 5; i++) {
            if (!top[i] || p->ciclosRestantes > top[i]->ciclosRestantes) {
                for (int j = 4; j > i; j--) top[j] = top[j-1];
                top[i] = p;
                break;
            }
        }
    }
    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | ciclosRest:%6d | espera:%4d | vecesEnCPU:%3d\n",
               i+1, top[i]->id, top[i]->ciclosRestantes,
               top[i]->tiempoEspera, top[i]->vecesEnCPU);
    if (!top[0]) printf("  (cola vacia)\n");
}

// 5 barras de aprovechamiento: estado actual + 4 anteriores
void vistaBarrasAprovechamiento(Cola *c, int histDesp[], int histCiclo[], int histIdx)
{
    int totalQ = 0, totalRafaga = 0, cnt = 0;
    for (NodoCola *n = c->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        totalQ     += tablaSistema.quantumActual;
        totalRafaga += (p->rafagaActual < tablaSistema.quantumActual)
                        ? p->rafagaActual : tablaSistema.quantumActual;
        cnt++;
    }
    int aprovActual = cnt ? (totalRafaga * 100) / (totalQ ? totalQ : 1) : 0;
    int despActual  = 100 - aprovActual;

    printf("\n--- APROVECHAMIENTO CPU (RR, Q=%d) ---\n", tablaSistema.quantumActual);

    // Mostrar los 4 anteriores del historico
    int indices[4], mostrar = 0;
    for (int i = 0; i < 4 && mostrar < 4; i++) {
        int idx = (histIdx - 1 - i + 100) % 100;
        if (histCiclo[idx] > 0) indices[mostrar++] = idx;
    }
    for (int i = mostrar - 1; i >= 0; i--) {
        int idx   = indices[i];
        int aprov = 100 - histDesp[idx];
        printf("  Ciclo %4d | Aprov:%3d%% [", histCiclo[idx], aprov);
        for (int b = 0; b < 20; b++) printf("%c", b < aprov / 5 ? '#' : '.');
        printf("] Desp:%3d%%\n", histDesp[idx]);
    }
    // Estado actual
    printf("  Ciclo %4d | Aprov:%3d%% [", tablaSistema.cicloActual, aprovActual);
    for (int b = 0; b < 20; b++) printf("%c", b < aprovActual / 5 ? '#' : '.');
    printf("] Desp:%3d%% <-- ACTUAL\n", despActual);
}

void vistaEstadoES(SistemaES *es)
{
    printf("\n--- ESTADO E/S ---\n");
    printf("  Disco    (x2) : %d proceso(s)\n", es->disco.tamanio);
    printf("  Pantalla (x4) : %d proceso(s)\n", es->pantalla.tamanio);
    printf("  Teclado  (x8) : %d proceso(s)\n", es->teclado.tamanio);
    printf("  Impresora(x12): %d proceso(s)\n", es->impresora.tamanio);
}

void vistaBienvenida(void)
{
    printf("\n================================================\n");
    printf("   SIMULADOR CPU-MEMORIA  (Proyecto III)\n");
    printf("   Buddy System + Paginacion NRU\n");
    printf("================================================\n");
    printf("  %d procesos | %d en ciclo | %d en solicitudes\n",
           TOTAL_PROCESOS, PROCESOS_EN_CICLO, PROCESOS_EN_SOLICITUD);
    printf("  Algoritmo inicial: FCFS\n");
    printf("  Teclas: [x] cambiar algoritmo   [a] apropiativo RR   [q] salir\n\n");
}

void vistaCierre(int reloj, int terminados)
{
    printf("\n================================================\n");
    printf("       SIMULACION FINALIZADA\n");
    printf("================================================\n");
    printf("  Ciclos totales: %d | Procesos terminados: %d\n\n", reloj, terminados);
}