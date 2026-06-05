#include <stdio.h>
#include <string.h>
#include <pvm3.h>
#include "master.h"

// ─── HELPERS ──────────────────────────────────────────────────────────────────
static void serializarProcesos(Lista *enEjecucion, ProcesoSerial *buf, int *total)
{
    *total = 0;
    for (Nodo *n = enEjecucion->cabeza; n && *total < TOTAL_PROCESOS; n = n->siguiente) {
        Proceso *p = n->proceso;
        ProcesoSerial *s = &buf[(*total)++];
        strncpy(s->id, p->id, 12);
        s->estado          = p->estado;
        s->ciclosRestantes = p->ciclosRestantes;
        s->tiempoEspera    = p->tiempoEspera;
        s->dispositivoES   = p->dispositivoES;

        int q = tablaSistema.quantumActual;
        int desp = q - p->rafagaActual;
        s->desperdicio = (desp > 0) ? desp : 0;
        
        s->aprovechamiento = p->aprovechamiento;
        s->rafagaActual    = p->rafagaActual;
        s->vecesEnCPU      = p->vecesEnCPU;
    }
}

// Envía un arreglo de ProcesoSerial por PVM (como bytes)
static void enviarSubconjunto(int tid, ProcesoSerial *arr, int n, int tag)
{
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&n, 1, 1);
    // Empaquetamos campo a campo para portabilidad
    for (int i = 0; i < n; i++) {
        pvm_pkstr(arr[i].id);
        pvm_pkint(&arr[i].estado,          1, 1);
        pvm_pkint(&arr[i].ciclosRestantes, 1, 1);
        pvm_pkint(&arr[i].tiempoEspera,    1, 1);
        pvm_pkint(&arr[i].dispositivoES,   1, 1);
        pvm_pkint(&arr[i].desperdicio,     1, 1);
        pvm_pkint(&arr[i].aprovechamiento, 1, 1);
        pvm_pkint(&arr[i].rafagaActual,    1, 1);
        pvm_pkint(&arr[i].vecesEnCPU,      1, 1);
    }
    pvm_send(tid, tag);
}

static void recibirResultadoStats(int tid, ResultadoStats *r)
{
    pvm_recv(tid, MSG_RESULTADO_STATS);
    pvm_upkint(&r->finalizados,        1, 1);
    pvm_upkint(&r->enEspera,           1, 1);
    pvm_upkint(&r->enES,               1, 1);
    pvm_upkfloat(&r->promCiclosPendientes, 1, 1);
    for (int i = 0; i < 3; i++) { pvm_upkstr(r->topId[i]); pvm_upkint(&r->topDesp[i], 1, 1); }
}

static void recibirResultadoRR(int tid, ResultadoRR *r)
{
    pvm_recv(tid, MSG_RESULTADO_RR);
    for (int i = 0; i < 3; i++) { pvm_upkstr(r->perjId[i]); pvm_upkint(&r->perjDelta[i], 1, 1); }
    for (int i = 0; i < 3; i++) { pvm_upkstr(r->despId[i]); pvm_upkint(&r->despVal[i],   1, 1); }
    pvm_upkfloat(&r->promAprovechamiento, 1, 1);
    pvm_upkint(&r->totalRetornos, 1, 1);
}

// ─── MERGE TOP-5 ──────────────────────────────────────────────────────────────
typedef struct { char id[12]; int val; } Par;

static void insertarTop5(Par top[], int *sz, const char *id, int val)
{
    for (int i = 0; i < *sz; i++) {
        if (val > top[i].val) {
            for (int j = (*sz < 5 ? *sz : 4); j > i; j--) top[j] = top[j-1];
            strncpy(top[i].id, id, 12); top[i].val = val;
            if (*sz < 5) (*sz)++;
            return;
        }
    }
    if (*sz < 5) { strncpy(top[*sz].id, id, 12); top[*sz].val = val; (*sz)++; }
}

// ─── MASTER PRINCIPAL ─────────────────────────────────────────────────────────
void ejecutarMasterPVM(Lista *enEjecucion, int quantum)
{
    // Serializar todos los procesos
    static ProcesoSerial todos[TOTAL_PROCESOS];
    int total = 0;
    serializarProcesos(enEjecucion, todos, &total);
    if (total == 0) { printf("[PVM] No hay procesos para distribuir\n"); return; }

    int mitad = total / 2;
    int resto = total - mitad;

    // Crear 2 slaves
    int tids[2];
    int info = pvm_spawn("slave", NULL, PvmTaskDefault, "", 2, tids);
    if (info < 2) {
        printf("[PVM] Error al crear slaves (info=%d). Verifique PVM.\n", info);
        for (int i = 0; i < info; i++) pvm_kill(tids[i]);
        return;
    }

    printf("[PVM] Slaves creados: TID0=%d TID1=%d\n", tids[0], tids[1]);

    // ── TAREA 1: estadísticas parciales ──────────────────────────────────────
    enviarSubconjunto(tids[0], todos,         mitad, MSG_DATOS_PROCESOS);
    enviarSubconjunto(tids[1], todos + mitad, resto, MSG_DATOS_PROCESOS);

    ResultadoStats rs0, rs1;
    recibirResultadoStats(tids[0], &rs0);
    recibirResultadoStats(tids[1], &rs1);

    // Integrar tarea 1
    int totFinalizados = rs0.finalizados + rs1.finalizados;
    int totEspera      = rs0.enEspera    + rs1.enEspera;
    int totES          = rs0.enES        + rs1.enES;
    float promCiclos   = (rs0.promCiclosPendientes + rs1.promCiclosPendientes) / 2.0f;

    Par top5Desp[5]; int szDesp = 0;
    for (int i = 0; i < 3; i++) {
        insertarTop5(top5Desp, &szDesp, rs0.topId[i], rs0.topDesp[i]);
        insertarTop5(top5Desp, &szDesp, rs1.topId[i], rs1.topDesp[i]);
    }

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║        PVM - TAREA 1: ESTADISTICAS GLOBALES      ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Procesos finalizados : %-4d  (S0:%-3d S1:%-3d)  ║\n", totFinalizados, rs0.finalizados, rs1.finalizados);
    printf("║  Procesos en espera   : %-4d  (S0:%-3d S1:%-3d)  ║\n", totEspera, rs0.enEspera, rs1.enEspera);
    printf("║  Procesos en E/S      : %-4d  (S0:%-3d S1:%-3d)  ║\n", totES, rs0.enES, rs1.enES);
    printf("║  Prom ciclos CPU pend : %-8.2f                 ║\n", promCiclos);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  TOP Desperdiciadores de CPU:                    ║\n");
    for (int i = 0; i < szDesp; i++)
        printf("║    %d. %-10s  desperdicio=%d              \n", i+1, top5Desp[i].id, top5Desp[i].val);
    printf("╚══════════════════════════════════════════════════╝\n");

    // ── TAREA 2: análisis RR ──────────────────────────────────────────────────
    // Enviamos el quantum actual junto a los datos
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&quantum, 1, 1);
    pvm_send(tids[0], MSG_DATOS_RR);
    enviarSubconjunto(tids[0], todos,         mitad, MSG_DATOS_RR);

    pvm_initsend(PvmDataDefault);
    pvm_pkint(&quantum, 1, 1);
    pvm_send(tids[1], MSG_DATOS_RR);
    enviarSubconjunto(tids[1], todos + mitad, resto, MSG_DATOS_RR);

    ResultadoRR rr0, rr1;
    recibirResultadoRR(tids[0], &rr0);
    recibirResultadoRR(tids[1], &rr1);

    // Integrar tarea 2
    float promAprov = (rr0.promAprovechamiento + rr1.promAprovechamiento) / 2.0f;
    int   totRetornos = rr0.totalRetornos + rr1.totalRetornos;

    Par top5Perj[5]; int szPerj = 0;
    Par top5DespRR[5]; int szDespRR = 0;
    for (int i = 0; i < 3; i++) {
        insertarTop5(top5Perj,   &szPerj,   rr0.perjId[i], rr0.perjDelta[i]);
        insertarTop5(top5Perj,   &szPerj,   rr1.perjId[i], rr1.perjDelta[i]);
        insertarTop5(top5DespRR, &szDespRR, rr0.despId[i], rr0.despVal[i]);
        insertarTop5(top5DespRR, &szDespRR, rr1.despId[i], rr1.despVal[i]);
    }

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║       PVM - TAREA 2: ANALISIS ROUND ROBIN        ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Quantum actual       : %-4d                     ║\n", quantum);
    printf("║  Prom aprovechamiento : %-6.2f%%                  ║\n", promAprov);
    printf("║  Total retornos cola  : %-6d                   ║\n", totRetornos);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  TOP 5 MAS PERJUDICADOS (quantum pequeño):       ║\n");
    for (int i = 0; i < (szPerj < 5 ? szPerj : 5); i++)
        printf("║    %d. %-10s  exceso=%d ciclos\n", i+1, top5Perj[i].id, top5Perj[i].val);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  TOP 5 MAYOR DESPERDICIO CPU (RR):               ║\n");
    for (int i = 0; i < (szDespRR < 5 ? szDespRR : 5); i++)
        printf("║    %d. %-10s  desperdicio=%d\n", i+1, top5DespRR[i].id, top5DespRR[i].val);
    printf("╚══════════════════════════════════════════════════╝\n");

    // Señal de fin
    for (int i = 0; i < 2; i++) {
        pvm_initsend(PvmDataDefault);
        pvm_send(tids[i], MSG_FIN);
    }

    pvm_exit();
}