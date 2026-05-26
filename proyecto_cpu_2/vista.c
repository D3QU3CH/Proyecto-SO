#include <stdio.h>
#include "vista.h"

// ─────────────────────────────────────────────────────────────────────────────
// AUXILIARES
// ─────────────────────────────────────────────────────────────────────────────

static const char *nomEstado(int e)
{
    switch (e) {
        case 0: return "LISTO";
        case 1: return "EJECUTANDO";
        case 2: return "ESPERA_ES";
        case 3: return "TERMINADO";
        case 4: return "BLOQ_SC";
        default: return "?";
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

// ─────────────────────────────────────────────────────────────────────────────
// BCP individual
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarBCP(Proceso *p)
{
    printf(NEGRITA AZUL "\n  BCP: %s\n" RESET, p->id);
    printf("   1. ID/Nombre      : %s / %s\n",  p->id, p->nombre);
    printf("   3. Llegada        : %d\n",        p->tiempoLlegada);
    printf("   4. CiclosTotales  : %d\n",        p->ciclosTotales);
    printf("   5. CiclosRest.    : %d\n",        p->ciclosRestantes);
    printf("   6. RafagaActual   : %d\n",        p->rafagaActual);
    printf("   7. TiempoEjec.    : %d\n",        p->tiempoEjecucion);
    printf("   8. TiempoEspera   : %d\n",        p->tiempoEspera);
    printf("  11. Estado         : %s\n",        nomEstado(p->estado));
    printf("  12. VecesEnCPU     : %d\n",        p->vecesEnCPU);
    printf("  15. CambioCtx      : %d\n",        p->cambiosContexto);
    printf("  17. TipoProceso    : %s\n",        p->tipoProceso == 0 ? "CPU" : "ES");
    printf("  20. DispositivoES  : %s\n",        nomDispES(p->dispositivoES));
    printf("  22. Bloqueado      : %s\n",        p->bloqueado ? "SI" : "NO");
    printf("  EX. Mem.Real/Buddy : %d KB / %d KB  (desp: %d KB)\n",
           p->memoriaUsadaKB, p->bloqueMemoriaKB, p->desperdicioInterno);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lista resumida (enEjecucion o solicitudes)
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarLista(Lista *l, const char *titulo)
{
    printf(NEGRITA VERDE "\n  === %s (%d procesos) ===\n" RESET, titulo, l->tamanio);
    int i = 1;
    for (Nodo *n = l->cabeza; n; n = n->siguiente, i++) {
        Proceso *p = n->proceso;
        printf("  %3d. " MAGENTA "%-8s" RESET
               " | %-10s | llegada:%3d | ciclos:%6d"
               " | rafaga:%2d | cc:%2d | mem:%dKB(buddy:%dKB)\n",
               i, p->id, nomEstado(p->estado),
               p->tiempoLlegada, p->ciclosRestantes,
               p->rafagaActual, p->cambiosContexto,
               p->memoriaUsadaKB, p->bloqueMemoriaKB);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Cola de listos
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarColaListos(Cola *c)
{
    printf(NEGRITA AMARILLO "\n  === Cola Listos (%d) ===\n" RESET, c->tamanio);
    int i = 1;
    for (NodoCola *n = c->frente; n; n = n->siguiente, i++) {
        Proceso *p = n->proceso;
        printf("  %2d. " CIAN "%-8s" RESET " | espera:%d | ciclosRest:%d\n",
               i, p->id, p->tiempoEspera, p->ciclosRestantes);
    }
    if (!c->tamanio) printf("  (vacia)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Tabla global del sistema
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarTablaGlobal(void)
{
    TablaProcesos *t = &tablaSistema;
    printf(NEGRITA AZUL "\n  === VARIABLES GLOBALES ===\n" RESET);
    printf("  1.  Total procesos      : %d\n", t->totalProcesos);
    printf("  2.  En ciclo            : %d\n", t->procesosEnCiclo);
    printf("  3.  En solicitudes      : %d\n", t->procesosEnSolicitud);
    printf("  4.  Cola listos         : %d\n", t->procesosEnColaListos);
    printf("  5.  Ejecutando          : %d\n", t->procesosEjecutando);
    printf("  6.  En E/S              : %d\n", t->procesosEnES);
    printf("  7.  Terminados          : %d\n", t->procesosTerminados);
    printf("  8.  Bloqueados          : %d\n", t->procesosBloqueados);
    printf("  9.  Algoritmo actual    : %d\n", t->algoritmoActual);
    printf(" 10.  Quantum actual      : %d\n", t->quantumActual);
    printf(" 11.  Ciclo actual        : %d\n", t->cicloActual);
    printf(" 12.  Cambios contexto    : %d\n", t->totalCambiosContexto);
    printf(" 13.  Fallos pagina       : %d\n", t->totalFallosPagina);
    printf(" 14.  Suma espera         : %d\n", t->sumaEspera);
    printf(" 15.  Suma ciclos rest.   : %d\n", t->sumaCiclosRestantes);
    printf(" 16.  Prom. espera        : %d\n", t->promedioEspera);
    printf(" 17.  Prom. ciclos        : %d\n", t->promedioCiclos);
    printf(" 18.  Ingresados dinam.   : %d\n", t->procesosIngresadosDinam);
    printf(" 19.  Mem. libre (KB)     : %d\n", t->memoriaLibreKB);
    printf(" 20.  Desperdicio total   : %d\n", t->desperdicioTotal);
}

// ─────────────────────────────────────────────────────────────────────────────
// Buddy System
// ─────────────────────────────────────────────────────────────────────────────

void vistaMostrarBuddy(void)
{
    int libres = 0, ocupados = 0;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        if (memoriaBuddy.bloques[i].tamanioKB == 0) continue;
        if (memoriaBuddy.bloques[i].libre) libres++;
        else                               ocupados++;
    }

    printf(NEGRITA CIAN "\n  === BUDDY SYSTEM ===\n" RESET);
    printf("  Total:1024KB | Libre:%dKB | Usado:%dKB | Desp.int:%dKB\n",
           memoriaBuddy.memoriaLibreKB,
           memoriaBuddy.memoriaUsadaKB,
           memoriaBuddy.desperdicioInternoTotal);
    printf("  Bloques: %d total | %d ocupados | %d libres\n\n",
           memoriaBuddy.numBloques, ocupados, libres);

    printf("  %-4s %-6s %-6s %-10s\n", "Idx", "TamKB", "Base", "Estado");
    int mostrados = 0;
    for (int i = 0; i < memoriaBuddy.numBloques && mostrados < 20; i++) {
        BloqueBS *b = &memoriaBuddy.bloques[i];
        if (b->tamanioKB == 0) continue;
        printf("  %-4d %-6d %-6d %s\n",
               i, b->tamanioKB, b->baseDir,
               b->libre ? VERDE "LIBRE" RESET : ROJO "OCUPADO" RESET);
        mostrados++;
    }
    if (memoriaBuddy.numBloques > mostrados)
        printf("  ... (%d bloques mas)\n", memoriaBuddy.numBloques - mostrados);
}

// ─────────────────────────────────────────────────────────────────────────────
// Estado E/S
// ─────────────────────────────────────────────────────────────────────────────

void vistaEstadoES(SistemaES *es)
{
    printf(NEGRITA AMARILLO "\n  === E/S ===\n" RESET);
    printf("  Disco(x2)    : %d proceso(s)\n", es->disco.tamanio);
    printf("  Pantalla(x4) : %d proceso(s)\n", es->pantalla.tamanio);
    printf("  Teclado(x8)  : %d proceso(s)\n", es->teclado.tamanio);
    printf("  Impresora(x12): %d proceso(s)\n", es->impresora.tamanio);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bienvenida y cierre
// ─────────────────────────────────────────────────────────────────────────────

void vistaBienvenida(void)
{
    printf(NEGRITA AZUL
           "\n╔══════════════════════════════════════╗\n"
           "║   SIMULADOR CPU-MEMORIA  (P-III)     ║\n"
           "╚══════════════════════════════════════╝\n" RESET);
    printf("  250 procesos | 150 en ciclo | 100 en solicitudes\n");
    printf("  Memoria: 1024 KB (Buddy System)\n");
    printf("  Teclas: [s]=tabla global  [b]=buddy  [e]=ES  [q]=salir\n\n");
}



void vistaCierre(int reloj, int terminados)
{
    printf(NEGRITA VERDE
           "\n╔══════════════════════════════════════╗\n"
           "║       SIMULACION FINALIZADA          ║\n"
           "╚══════════════════════════════════════╝\n" RESET);
    printf("  Ciclos totales: %d | Procesos terminados: %d\n\n",
           reloj, terminados);
}