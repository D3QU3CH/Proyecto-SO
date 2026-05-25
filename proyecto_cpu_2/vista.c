#include <stdio.h>
#include "vista.h"

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

// ── BCP completo ──────────────────────────────────────────────────────────────

void vistaMostrarBCP(Proceso *p)
{
    printf(NEGRITA AZUL "\n  BCP: %s\n" RESET, p->id);
    printf("   1. ID/Nombre      : %s / %s\n", p->id, p->nombre);
    printf("   3. Llegada        : %d\n", p->tiempoLlegada);
    printf("   4. CiclosTotales  : %d\n", p->ciclosTotales);
    printf("   5. CiclosRest.    : %d\n", p->ciclosRestantes);
    printf("   7. TiempoEjec.    : %d\n", p->tiempoEjecucion);
    printf("   8. TiempoEspera   : %d\n", p->tiempoEspera);
    printf("  11. Estado         : %s\n", nomEstado(p->estado));
    printf("  12. VecesEnCPU     : %d\n", p->vecesEnCPU);
    printf("  17. TipoProceso    : %s\n", p->tipoProceso == 0 ? "CPU" : "ES");
    printf("  20. DispositivoES  : %s\n", nomDispES(p->dispositivoES));
    printf("  22. Bloqueado      : %s\n", p->bloqueado ? "SI" : "NO");
    printf("  EX. Mem. bloque    : %d KB | Usado: %d KB | Desp: %d KB\n",
           p->bloqueMemoriaKB, p->memoriaUsadaKB, p->desperdicioInterno);
    printf("  EX. Marcos NRU     : %d | Fallos: %d\n",
           p->numMarcos, p->fallosPagina);
}

// ── Lista resumida ────────────────────────────────────────────────────────────

void vistaMostrarLista(Lista *l, const char *titulo)
{
    printf(NEGRITA VERDE "\n  %s (%d)\n" RESET, titulo, l->tamanio);
    int i = 1;
    for (Nodo *n = l->cabeza; n; n = n->siguiente, i++) {
        Proceso *p = n->proceso;
        // Si tiene bloque buddy asignado mostrarlo, si no el pedido original
        int mem = p->bloqueMemoriaKB > 0 ? p->bloqueMemoriaKB : p->memoriaUsadaKB;
        printf("  %3d. " MAGENTA "%-8s" RESET " | %-10s | Llegada:%3d | Ciclos:%6d | Buddy:%3dKB Real:%2dKB\n",
               i, p->id, nomEstado(p->estado),
               p->tiempoLlegada, p->ciclosRestantes,
               p->bloqueMemoriaKB, p->memoriaUsadaKB);
        (void)mem;
    }
}

// ── Cola de listos ────────────────────────────────────────────────────────────

void vistaMostrarColaListos(Cola *c)
{
    printf(NEGRITA AMARILLO "\n  Cola Listos (%d)\n" RESET, c->tamanio);
    int i = 1;
    for (NodoCola *n = c->frente; n; n = n->siguiente, i++) {
        Proceso *p = n->proceso;
        printf("  %2d. " CIAN "%-8s" RESET " | Espera:%d | Ciclos:%d\n",
               i, p->id, p->tiempoEspera, p->ciclosRestantes);
    }
    if (!c->tamanio) printf("  (vacia)\n");
}

// ── Tabla global ──────────────────────────────────────────────────────────────

void vistaMostrarTablaGlobal(void)
{
    TablaProcesos *t = &tablaSistema;
    printf(NEGRITA AZUL "\n  === VARIABLES GLOBALES ===\n" RESET);
    printf("  Total/Ciclo/Solic  : %d / %d / %d\n",
           t->totalProcesos, t->procesosEnCiclo, t->procesosEnSolicitud);
    printf("  ColaListos/Ejec/ES : %d / %d / %d\n",
           t->procesosEnColaListos, t->procesosEjecutando, t->procesosEnES);
    printf("  Terminados/Bloq    : %d / %d\n",
           t->procesosTerminados, t->procesosBloqueados);
    printf("  Ciclo actual       : %d\n",  t->cicloActual);
    printf("  Prom. espera/ciclos: %d / %d\n",
           t->promedioEspera, t->promedioCiclos);
    printf("  Mem. libre/desp.   : %d KB / %d KB\n",
           t->memoriaLibreKB, t->desperdicioTotal);
    printf("  Ingresados dinam.  : %d\n", t->procesosIngresadosDinam);
}

// ── Buddy system ─────────────────────────────────────────────────────────────

void vistaMostrarBuddy(void)
{
    int libres = 0, ocupados = 0;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        if (memoriaBuddy.bloques[i].tamanioKB == 0) continue;
        if (memoriaBuddy.bloques[i].libre) libres++;
        else ocupados++;
    }

    printf(NEGRITA CIAN "\n  === BUDDY SYSTEM ===\n" RESET);
    printf("  Total: %d KB | Libre: %d KB | Usado: %d KB | Desp.int: %d KB\n",
           MEMORIA_TOTAL_KB,
           memoriaBuddy.memoriaLibreKB,
           memoriaBuddy.memoriaUsadaKB,
           memoriaBuddy.desperdicioInternoTotal);
    printf("  Bloques: %d total | %d ocupados | %d libres\n\n",
           memoriaBuddy.numBloques, ocupados, libres);

    printf("  %-4s %-6s %-6s %-8s\n", "Idx", "TamKB", "Base", "Estado");
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

// ── NRU de un proceso ─────────────────────────────────────────────────────────

void vistaMostrarNRU(Proceso *p)
{
    printf(NEGRITA CIAN "\n  NRU: %s | Marcos:%d | Fallos:%d | Reempl:%d\n" RESET,
           p->id, p->numMarcos, p->fallosPagina, p->reemplazosNRU);

    for (int i = 0; i < p->numMarcos; i++) {
        MarcoNRU *m = &p->marcos[i];
        if (!m->valido) { printf("  [%2d] vacio\n", i); continue; }
        int cl = (m->bitR << 1) | m->bitM;
        printf("  [%2d] Pag%03d R=%d M=%d Clase%d | %s\n",
               i, m->numeroPagina, m->bitR, m->bitM, cl,
               m->palabras[0][0] ? m->palabras[0] : "---");
    }
}

// ── E/S ───────────────────────────────────────────────────────────────────────

void vistaEstadoES(SistemaES *es)
{
    printf(NEGRITA AMARILLO "\n  E/S: " RESET
           "Disco:%d Pantalla:%d Teclado:%d Impresora:%d\n",
           es->disco.tamanio, es->pantalla.tamanio,
           es->teclado.tamanio, es->impresora.tamanio);
}

// ── Inicio / cierre ───────────────────────────────────────────────────────────

void vistaBienvenida(void)
{
    printf(NEGRITA AZUL
           "\n╔═══════════════════════════════════╗\n"
           "║  SIMULADOR CPU-MEMORIA (P-III)    ║\n"
           "╚═══════════════════════════════════╝\n" RESET);
    printf("  Procesos: %d | Ciclo: %d | Solic: %d\n",
           TOTAL_PROCESOS, EN_EJECUCION, EN_SOLICITUDES);
    printf("  Memoria: %d KB (Buddy) | Paginas: NRU\n", MEMORIA_TOTAL_KB);
    printf("  Teclas: [s]=estado  [b]=buddy  [n]=NRU  [q]=salir\n\n");
}

void vistaCierre(int reloj, int terminados)
{
    printf(NEGRITA VERDE
           "\n╔═══════════════════════════════════╗\n"
           "║       SIMULACION FINALIZADA       ║\n"
           "╚═══════════════════════════════════╝\n" RESET);
    printf("  Ciclos: %d | Terminados: %d\n", reloj, terminados);
    printf("  Logs: bcps.log  variables.log  eventos.log\n\n");
}