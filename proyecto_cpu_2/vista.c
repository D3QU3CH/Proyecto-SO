#include <stdio.h>
#include "vista.h"

// ─────────────────────────────────────────────────────────────────────────────
// HISTORIAL DE APROVECHAMIENTO (ring buffer de 5 posiciones)
// ─────────────────────────────────────────────────────────────────────────────

static int historialUso[5] = {0, 0, 0, 0, 0};

void vistaPushHistorial(int uso)
{
    for (int i = 0; i < 4; i++)
        historialUso[i] = historialUso[i + 1];

    historialUso[4] = uso;
}

void vistaMostrarHistorialCPU(void)
{
    printf(NEGRITA "\n  # APROVECHAMIENTO CPU (ultimas 5 mediciones) #\n" RESET);
    printf("  ");

    for (int i = 0; i < 5; i++) {
        int uso       = historialUso[i];
        int desperdicio = 100 - uso;

        printf(VERDE "[");

        for (int j = 0; j < uso / 10; j++)
            printf("#");

        printf(ROJO);

        for (int j = 0; j < desperdicio / 10; j++)
            printf("-");

        printf(RESET "] %d%%  ", uso);
    }

    printf("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// BALANCE DE COLAS
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarBalanceColas(Cola *colaEnCiclo, SistemaES *es)
{
    int listos = 0;

    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente)
        if (n->proceso->estado == 0)
            listos++;

    int enES  = contarES(es);
    int total = listos + enES;

    printf(NEGRITA "\n  # BALANCE DE COLAS #\n" RESET);
    printf("  Listos: " VERDE "%d" RESET
           "  |  En E/S: " AMARILLO "%d" RESET
           "  |  Total activos: %d\n",
           listos, enES, total);

    if (total > 0) {
        int pct = (listos * 100) / total;
        printf("  CPU/Listos: %d%%  |  E/S: %d%%\n", pct, 100 - pct);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ENVEJECIMIENTO Y DESPERDICIO
// ─────────────────────────────────────────────────────────────────────────────

static void sortProcesosDesc(Proceso **arr, int n, int (*campo)(Proceso *))
{
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (campo(arr[j]) > campo(arr[i])) {
                Proceso *tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
}

static int getCiclos(Proceso *p)     { return p->ciclosRestantes; }
static int getDesperdicio(Proceso *p) { return p->desperdicio; }

void vistaMostrarEnvejecimiento(Cola *cola)
{
    Proceso *activos[256];
    int n = 0;

    for (Nodo *nd = cola->frente; nd != NULL && n < 256; nd = nd->siguiente)
        activos[n++] = nd->proceso;

    if (n == 0) return;

    sortProcesosDesc(activos, n, getCiclos);

    int mostrar = (n < 5) ? n : 5;

    printf(ROJO NEGRITA
           "\n  --- TOP %d PROCESOS MAS REZAGADOS "
           "(mas ciclos restantes) ---\n" RESET,
           mostrar);

    for (int i = 0; i < mostrar; i++)
        printf("  %d. " MAGENTA "%-8s" RESET
               " | Ciclos rest.: " AMARILLO "%6d" RESET
               " | Espera: %d\n",
               i + 1,
               activos[i]->id,
               activos[i]->ciclosRestantes,
               activos[i]->tiempoEspera);
}

void vistaMostrarTopDesperdicio(Cola *cola)
{
    Proceso *lista[256];
    int n = 0;

    for (Nodo *nd = cola->frente; nd != NULL && n < 256; nd = nd->siguiente)
        if (nd->proceso->desperdicio > 0)
            lista[n++] = nd->proceso;

    if (n == 0) return;

    sortProcesosDesc(lista, n, getDesperdicio);

    int mostrar = (n < 5) ? n : 5;

    printf(AMARILLO NEGRITA
           "\n  --- TOP %d PROCESOS QUE MAS DESPERDICIAN CPU ---\n" RESET,
           mostrar);

    for (int i = 0; i < mostrar; i++)
        printf("  %d. " CIAN "%-8s" RESET
               " | Desperdicio: " ROJO "%6d" RESET
               " | VecesEnCPU: %d\n",
               i + 1,
               lista[i]->id,
               lista[i]->desperdicio,
               lista[i]->vecesEnCPU);
}

// ─────────────────────────────────────────────────────────────────────────────
// ESTADO DEL SISTEMA
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarEstadoSistema(Cola *colaListos,
                               int algoritmo,
                               int quantum,
                               int terminados)
{
    int activos = colaListos->tamanio;
    int sumaEsp = 0, sumaCiclos = 0;

    for (Nodo *n = colaListos->frente; n != NULL; n = n->siguiente) {
        sumaEsp    += n->proceso->tiempoEspera;
        sumaCiclos += n->proceso->ciclosRestantes;
    }

    printf(NEGRITA AZUL
           "\n  ========== ESTADO DEL SISTEMA ==========\n" RESET);
    printf("  Algoritmo      : " VERDE "%s\n" RESET,
           algoritmo == 1 ? "FCFS" : "Round Robin");
    if (algoritmo == 2)
        printf("  Quantum        : " AMARILLO "%d\n" RESET, quantum);
    printf("  Procs. activos : %d\n", activos);
    printf("  Procs. term.   : " VERDE "%d\n" RESET, terminados);
    printf("  Suma espera    : %d\n", sumaEsp);
    printf("  Ciclos pend.   : %d\n", sumaCiclos);
    printf(AZUL "  =========================================\n\n" RESET);
}

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA (frutas)
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarMemoria(void)
{
    printf(NEGRITA AZUL
           "\n  ========== MEMORIA PRINCIPAL (Frutas) ==========\n" RESET);

    for (int i = 0; i < TAM_MEM; i++) {
        printf("  [%2d] " AMARILLO "%-12s" RESET "  %s\n",
               i, memoria[i],
               recursoOcupado[i]
                   ? ROJO "[EN USO]" RESET
                   : VERDE "[LIBRE ]" RESET);
    }

    printf(AZUL "  =================================================\n\n" RESET);
}

// ─────────────────────────────────────────────────────────────────────────────
// NRU – estado de marcos de pagina
// ─────────────────────────────────────────────────────────────────────────────

// Nombre de la clase NRU segun bits R y M

static const char *nombreClaseNRU(int cl)
{
    switch (cl) {
        case 0: return "Cl0(R=0,M=0)";
        case 1: return "Cl1(R=0,M=1)";
        case 2: return "Cl2(R=1,M=0)";
        case 3: return "Cl3(R=1,M=1)";
        default: return "---";
    }
}

void vistaMostrarNRU(Cola *cola)
{
    printf(NEGRITA CIAN
           "\n  ========== ESTADO NRU (marcos de pagina) ==========\n"
           RESET);

    int mostrados = 0;

    for (Nodo *nd = cola->frente; nd != NULL && mostrados < 10; nd = nd->siguiente) {
        Proceso *p = nd->proceso;

        printf("  Proceso: " MAGENTA "%-8s" RESET
               " | FallosPag: " ROJO "%3d" RESET
               " | Reemplazos: %d\n",
               p->id, p->fallosPagina, p->reemplazosNRU);

        printf("  Marcos: ");
        for (int i = 0; i < NRU_NUM_MARCOS; i++) {
            MarcoNRU *m = &p->marcosNRU[i];
            if (!m->valido) {
                printf(AZUL "[vacio] " RESET);
            } else {
                int cl = (m->bitR << 1) | m->bitM;
                const char *color = (cl == 0) ? VERDE :
                                    (cl == 1) ? AMARILLO :
                                    (cl == 2) ? CIAN : ROJO;
                printf("%sPag%02d/%s%s ",
                       color, m->numeroPagina,
                       nombreClaseNRU(cl), RESET);
            }
        }
        printf("\n");
        mostrados++;
    }

    if (mostrados == 0)
        printf("  (No hay procesos activos)\n");

    printf(CIAN "  ====================================================\n\n"
           RESET);
}

// ─────────────────────────────────────────────────────────────────────────────
// INICIO Y CIERRE
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarBienvenida(void)
{
    printf(NEGRITA AZUL
           "\n============================================\n"
           "  SIMULADOR DE PLANIFICACION DE CPU\n"
           "============================================\n" RESET);

    printf("  Procesos totales   : %d\n", TOTAL_PROCESOS);
    printf("  En ciclo inicial   : %d\n", EN_SISTEMA);
    printf("  En espera inicial  : %d\n", TOTAL_PROCESOS - EN_SISTEMA);

    printf("  Controles: "
           VERDE "X" RESET "=algoritmo  "
           VERDE "A" RESET "=prioridad(RR)  "
           VERDE "M" RESET "=memoria  "
           VERDE "S" RESET "=estado  "
           VERDE "N" RESET "=NRU\n");

    printf(AZUL "--------------------------------------------\n\n" RESET);
}

void vistaMostrarCierre(int ciclos, int terminados)
{
    printf(NEGRITA VERDE
           "\n============================================\n"
           "  SIMULACION FINALIZADA\n"
           "============================================\n" RESET);

    printf("  Ciclos totales    : %d\n", ciclos);
    printf("  Procesos term.    : " VERDE "%d\n" RESET, terminados);

    printf("  Logs generados:\n");
    printf("    tabla_procesos.log\n");
    printf("    variables_globales.log\n");
    printf("    eventos.log\n");
    printf("    socios.log\n\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// MENSAJES DE EVENTOS PUNTUALES
// ─────────────────────────────────────────────────────────────────────────────

void vistaMensajeAlgoritmo(int algoritmo)
{
    printf(NEGRITA VERDE "\n  [OK] Algoritmo cambiado a: %s\n" RESET,
           algoritmo == 1 ? "FCFS" : "Round Robin");
}

void vistaMensajeProcesoCritico(Proceso *p)
{
    printf(NEGRITA VERDE
           "\n  *** Proceso [%s] marcado como APROPIATIVO ***\n"
           "  Se colocara primero en la cola de Listos siempre.\n" RESET,
           p->id);
}

void vistaMensajeCambioAutomatico(int de, int a)
{
    printf(NEGRITA ROJO "\n  *** CAMBIO AUTOMATICO: %s -> %s ***\n" RESET,
           de == 1 ? "FCFS" : "RR",
           a  == 1 ? "FCFS" : "RR");
}