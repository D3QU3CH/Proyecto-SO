#include <stdio.h>
#include <string.h>
#include <float.h>
#include <pvm3.h>
#include "master.h"
#include "slave.h"
#include <unistd.h>

// ─── RECIBIR SUBCONJUNTO ──────────────────────────────────────────────────────
static int recibirSubconjunto(ProcesoSerial *buf, int tag)
{
    pvm_recv(-1, tag);
    int n = 0;
    pvm_upkint(&n, 1, 1);
    for (int i = 0; i < n; i++) {
        pvm_upkstr(buf[i].id);
        pvm_upkint(&buf[i].estado,          1, 1);
        pvm_upkint(&buf[i].ciclosRestantes, 1, 1);
        pvm_upkint(&buf[i].tiempoEspera,    1, 1);
        pvm_upkint(&buf[i].dispositivoES,   1, 1);
        pvm_upkint(&buf[i].desperdicio,     1, 1);
        pvm_upkint(&buf[i].aprovechamiento, 1, 1);
        pvm_upkint(&buf[i].rafagaActual,    1, 1);
        pvm_upkint(&buf[i].vecesEnCPU,      1, 1);
    }
    return n;
}

// ─── TOP-3 LOCAL ──────────────────────────────────────────────────────────────
static void insertar3(char ids[][12], int vals[], int *sz, const char *id, int v)
{
    for (int i = 0; i < *sz; i++) {
        if (v > vals[i]) {
            for (int j = (*sz < 3 ? *sz : 2); j > i; j--) {
                strncpy(ids[j], ids[j-1], 12); vals[j] = vals[j-1];
            }
            strncpy(ids[i], id, 12); vals[i] = v;
            if (*sz < 3) (*sz)++;
            return;
        }
    }
    if (*sz < 3) { strncpy(ids[*sz], id, 12); vals[*sz] = v; (*sz)++; }
}

// ─── TAREA 1: ESTADÍSTICAS ────────────────────────────────────────────────────
static void procesarStats(ProcesoSerial *arr, int n)
{
    ResultadoStats r;
    memset(&r, 0, sizeof(r));

    long sumCiclos = 0; int cntCiclos = 0;
    int szDesp = 0;

    for (int i = 0; i < n; i++) {
        ProcesoSerial *p = &arr[i];
        if (p->estado == ESTADO_TERMINADO)  r.finalizados++;
        if (p->estado == ESTADO_LISTO)      r.enEspera++;
        if (p->estado == ESTADO_ESPERA_ES)  r.enES++;
        if (p->estado != ESTADO_TERMINADO) { sumCiclos += p->ciclosRestantes; cntCiclos++; }
        insertar3(r.topId, r.topDesp, &szDesp, p->id, p->desperdicio);
    }
    r.promCiclosPendientes = cntCiclos ? (float)sumCiclos / cntCiclos : 0.0f;
    // rellenar posiciones vacías del top
    for (int i = szDesp; i < 3; i++) { strncpy(r.topId[i], "N/A", 12); r.topDesp[i] = 0; }

    int master = pvm_parent();
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&r.finalizados,            1, 1);
    pvm_pkint(&r.enEspera,               1, 1);
    pvm_pkint(&r.enES,                   1, 1);
    pvm_pkfloat(&r.promCiclosPendientes, 1, 1);
    for (int i = 0; i < 3; i++) { pvm_pkstr(r.topId[i]); pvm_pkint(&r.topDesp[i], 1, 1); }
    pvm_send(master, MSG_RESULTADO_STATS);
}

// ─── TAREA 2: ANÁLISIS RR ────────────────────────────────────────────────────
static void procesarRR(ProcesoSerial *arr, int n, int quantum)
{
    ResultadoRR r;
    memset(&r, 0, sizeof(r));

    int szPerj = 0, szDesp = 0;
    long sumAprov = 0;

    for (int i = 0; i < n; i++) {
        ProcesoSerial *p = &arr[i];

        // Perjudicados: rafagaActual > quantum → se cortó, exceso = rafaga - quantum
        int exceso = p->rafagaActual - quantum;
        if (exceso < 0) exceso = 0;
        insertar3(r.perjId, r.perjDelta, &szPerj, p->id, exceso);

        insertar3(r.despId, r.despVal, &szDesp, p->id, p->desperdicio);

        sumAprov          += p->aprovechamiento;
        r.totalRetornos   += p->vecesEnCPU;
    }

    r.promAprovechamiento = n ? (float)sumAprov / n : 0.0f;

    for (int i = szPerj; i < 3; i++) { strncpy(r.perjId[i], "N/A", 12); r.perjDelta[i] = 0; }
    for (int i = szDesp; i < 3; i++) { strncpy(r.despId[i], "N/A", 12); r.despVal[i]   = 0; }

    int master = pvm_parent();
    pvm_initsend(PvmDataDefault);
    for (int i = 0; i < 3; i++) { pvm_pkstr(r.perjId[i]); pvm_pkint(&r.perjDelta[i], 1, 1); }
    for (int i = 0; i < 3; i++) { pvm_pkstr(r.despId[i]); pvm_pkint(&r.despVal[i],   1, 1); }
    pvm_pkfloat(&r.promAprovechamiento, 1, 1);
    pvm_pkint(&r.totalRetornos, 1, 1);
    pvm_send(master, MSG_RESULTADO_RR);
}

// ─── BUCLE PRINCIPAL SLAVE ────────────────────────────────────────────────────
void ejecutarSlave(void)
{
    sleep(1);
    static ProcesoSerial buf[TOTAL_PROCESOS];

    while (1) {
        // Esperar cualquier mensaje del master
        int bufid = pvm_recv(-1, -1);
        int tag   = pvm_bufinfo(bufid, NULL, NULL, NULL), bytes, msgtag, tid;
        pvm_bufinfo(bufid, &bytes, &msgtag, &tid);

        if (msgtag == MSG_FIN) break;

        if (msgtag == MSG_DATOS_PROCESOS) {
            // Re-leer: el recv ya lo consumió, necesitamos desempaquetar
            // Como ya se llamó pvm_recv, el buffer activo tiene los datos
            int n = 0;
            pvm_upkint(&n, 1, 1);
            for (int i = 0; i < n; i++) {
                pvm_upkstr(buf[i].id);
                pvm_upkint(&buf[i].estado,          1, 1);
                pvm_upkint(&buf[i].ciclosRestantes, 1, 1);
                pvm_upkint(&buf[i].tiempoEspera,    1, 1);
                pvm_upkint(&buf[i].dispositivoES,   1, 1);
                pvm_upkint(&buf[i].desperdicio,     1, 1);
                pvm_upkint(&buf[i].aprovechamiento, 1, 1);
                pvm_upkint(&buf[i].rafagaActual,    1, 1);
                pvm_upkint(&buf[i].vecesEnCPU,      1, 1);
            }
            procesarStats(buf, n);
        }
        else if (msgtag == MSG_DATOS_RR) {
            // Primero llega el quantum (mensaje separado), luego los datos
            // El master envía: MSG_DATOS_RR con quantum, luego MSG_DATOS_RR con procesos
            int quantum = 20;
            pvm_upkint(&quantum, 1, 1);
            // Recibir el segundo mensaje con los procesos
            int n = recibirSubconjunto(buf, MSG_DATOS_RR);
            procesarRR(buf, n, quantum);
        }
    }

    pvm_exit();
}

// ─── MAIN DEL SLAVE (ejecutable independiente) ───────────────────────────────
int main(void)
{   

    ejecutarSlave();
    return 0;
}