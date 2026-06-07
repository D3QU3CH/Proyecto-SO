#include <stdio.h>
#include "vista.h"

/* ═══════════════════════════════════════════════════════════════════════════
   TABLA GLOBAL DEL SISTEMA
   ═══════════════════════════════════════════════════════════════════════════ */
void vistaMostrarTablaGlobal(void)
{
    TablaProcesos *t = &tablaSistema;
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║     VARIABLES GLOBALES (ciclo %6d)       ║\n", t->cicloActual);
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Total procesos  : %-4d                     ║\n", t->totalProcesos);
    printf("║  Cola listos     : %-4d                     ║\n", t->procesosEnColaListos);
    printf("║  Ejecutando      : %-4d                     ║\n", t->procesosEjecutando);
    printf("║  En E/S          : %-4d                     ║\n", t->procesosEnES);
    printf("║  Terminados      : %-4d                     ║\n", t->procesosTerminados);
    printf("║  Solicitudes     : %-4d                     ║\n", t->procesosEnSolicitud);
    printf("║  Algoritmo       : %-6s                   ║\n",
           t->algoritmoActual == ALG_FCFS ? "FCFS" : "RR");
    printf("║  Quantum         : %-4d                     ║\n", t->quantumActual);
    printf("║  Cambios ctx     : %-6d                   ║\n", t->totalCambiosContexto);
    printf("║  Fallos pagina   : %-6d                   ║\n", t->totalFallosPagina);
    printf("║  Prom. espera    : %-6d                   ║\n", t->promedioEspera);
    printf("║  Mem. libre KB   : %-6d                   ║\n", t->memoriaLibreKB);
    printf("║  Desperdicio KB  : %-6d                   ║\n", t->desperdicioTotal);
    printf("╚══════════════════════════════════════════════╝\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   5 PROCESOS MÁS REZAGADOS (mayor ciclosRestantes)
   ═══════════════════════════════════════════════════════════════════════════ */
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

/* ═══════════════════════════════════════════════════════════════════════════
   5 BARRAS DE APROVECHAMIENTO (actual + 4 anteriores)
   FIX: el ciclo actual ya no se repite como entrada del historial.
        Las 4 barras anteriores vienen del historial y la 5ta es el estado
        calculado en este momento — sin duplicado.
   ═══════════════════════════════════════════════════════════════════════════ */
void vistaBarrasAprovechamiento(Cola *c, int histDesp[], int histCiclo[], int histIdx)
{
    /* Calcular aprovechamiento actual de la cola */
    int totalQ = 0, totalRafaga = 0, cnt = 0;
    for (NodoCola *n = c->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        int q      = tablaSistema.quantumActual;
        totalQ     += q;
        totalRafaga += (p->rafagaActual < q) ? p->rafagaActual : q;
        cnt++;
    }
    int aprovActual = (totalQ > 0 && cnt > 0)
                      ? (totalRafaga * 100) / totalQ
                      : 0;
    int despActual  = 100 - aprovActual;

    printf("\n--- APROVECHAMIENTO CPU (RR, Q=%d) ---\n",
           tablaSistema.quantumActual);

    /* Mostrar hasta 4 barras del historial (las más recientes) */
    int mostrados = 0;
    for (int i = 1; i <= 4 && mostrados < 4; i++) {
        int idx = (histIdx - i + 100) % 100;
        if (histCiclo[idx] == 0) continue;   /* entrada vacía */
        int aprov = 100 - histDesp[idx];
        printf("  Ciclo %4d | Aprov:%3d%% [", histCiclo[idx], aprov);
        for (int b = 0; b < 20; b++) printf("%c", b < aprov/5 ? '#' : '.');
        printf("] Desp:%3d%%\n", histDesp[idx]);
        mostrados++;
    }

    /* Barra del estado actual */
    printf("  Ciclo %4d | Aprov:%3d%% [", tablaSistema.cicloActual, aprovActual);
    for (int b = 0; b < 20; b++) printf("%c", b < aprovActual/5 ? '#' : '.');
    printf("] Desp:%3d%% <-- ACTUAL\n", despActual);
}

/* ═══════════════════════════════════════════════════════════════════════════
   ESTADO DE E/S
   ═══════════════════════════════════════════════════════════════════════════ */
void vistaEstadoES(SistemaES *es)
{
    printf("\n--- ESTADO E/S ---\n");
    printf("  Disco    (x2) : %d proceso(s)\n", es->disco.tamanio);
    printf("  Pantalla (x4) : %d proceso(s)\n", es->pantalla.tamanio);
    printf("  Teclado  (x8) : %d proceso(s)\n", es->teclado.tamanio);
    printf("  Impresora(x12): %d proceso(s)\n", es->impresora.tamanio);
}

/* ═══════════════════════════════════════════════════════════════════════════
   BIENVENIDA Y CIERRE
   ═══════════════════════════════════════════════════════════════════════════ */
void vistaBienvenida(void)
{
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║      SIMULADOR CPU-MEMORIA-DISTRIBUCION              ║\n");
    printf("║      Proyecto III — Sistemas Operativos              ║\n");
    printf("║      Buddy System + Paginacion NRU + PVM             ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  %3d procesos | %3d en ciclo | %3d en solicitudes    ║\n",
           TOTAL_PROCESOS, PROCESOS_EN_CICLO, PROCESOS_EN_SOLICITUD);
    printf("║  Algoritmo inicial : FCFS                            ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  Teclas:                                             ║\n");
    printf("║    [X] Cambiar algoritmo (FCFS <-> RR)               ║\n");
    printf("║    [A] Aplicar apropiatividad a un proceso           ║\n");
    printf("║    [Q] Salir de la simulacion                        ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
}

void vistaCierre(int reloj, int terminados)
{
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║            SIMULACION FINALIZADA                     ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  Ciclos totales     : %-6d                        ║\n", reloj);
    printf("║  Procesos terminados: %-6d                        ║\n", terminados);
    printf("╚══════════════════════════════════════════════════════╝\n\n");
}