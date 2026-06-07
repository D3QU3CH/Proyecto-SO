#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "modelo.h"

/* ═══════════════════════════════════════════════════════════════════════════
   VARIABLES GLOBALES
   ═══════════════════════════════════════════════════════════════════════════ */
TablaProcesos          tablaSistema;
MemoriaBuddy           memoriaBuddy;
BancoPalabras          bancoPalabras;
MemoriaPrincipal       memoriaPrincipal;
MemoriaPrincipalLegacy memoriaPrincipalLegacy;
AreaSwap               areaSwap;

static int tiemposUsados[801];

#define MAX_FRASES    200
#define MAX_LEN_FRASE 256
static char frases[MAX_FRASES][MAX_LEN_FRASE];
static int  totalFrases  = 0;
static int  cursorFrase  = 0;

/* ═══════════════════════════════════════════════════════════════════════════
   BANCO DE PALABRAS
   ═══════════════════════════════════════════════════════════════════════════ */
void cargarPalabras(const char *rutaArchivo)
{
    memset(&bancoPalabras, 0, sizeof(BancoPalabras));
    FILE *f = fopen(rutaArchivo, "r");
    if (!f) {
        /* Generar palabras de ejemplo si no existe el archivo */
        const char *base[] = {
            "proceso","sistema","memoria","cpu","pagina",
            "bloque","ciclo","reloj","hilo","semaforo",
            "mutex","cola","lista","nodo","buddy",
            "quantum","rafaga","espera","ejecucion","disco"
        };
        for (int i = 0; i < MAX_PALABRAS; i++)
            snprintf(bancoPalabras.palabras[i], MAX_LEN_PALABRA,
                     "%s%d", base[i % 20], i / 20);
        bancoPalabras.totalPalabras = MAX_PALABRAS;
        bancoPalabras.cursor        = 0;
        return;
    }
    char buf[MAX_LEN_PALABRA];
    while (bancoPalabras.totalPalabras < MAX_PALABRAS &&
           fscanf(f, "%63s", buf) == 1)
        strncpy(bancoPalabras.palabras[bancoPalabras.totalPalabras++],
                buf, MAX_LEN_PALABRA - 1);
    fclose(f);
    if (bancoPalabras.totalPalabras == 0) {
        strncpy(bancoPalabras.palabras[0], "palabra", MAX_LEN_PALABRA - 1);
        bancoPalabras.totalPalabras = 1;
    }
    bancoPalabras.cursor = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
   FRASES
   ═══════════════════════════════════════════════════════════════════════════ */
void cargarFrases(const char *ruta)
{
    totalFrases = 0;
    FILE *f = fopen(ruta, "r");
    if (!f) {
        /* Generar frases sintéticas a partir del banco */
        for (int i = 0; i < 20 && i < bancoPalabras.totalPalabras - 4; i++) {
            snprintf(frases[i], MAX_LEN_FRASE, "%s %s %s %s %s",
                bancoPalabras.palabras[(i)   % bancoPalabras.totalPalabras],
                bancoPalabras.palabras[(i+1) % bancoPalabras.totalPalabras],
                bancoPalabras.palabras[(i+2) % bancoPalabras.totalPalabras],
                bancoPalabras.palabras[(i+3) % bancoPalabras.totalPalabras],
                bancoPalabras.palabras[(i+4) % bancoPalabras.totalPalabras]);
            totalFrases++;
        }
        if (totalFrases == 0) {
            snprintf(frases[0], MAX_LEN_FRASE,
                     "proceso ejecuta instruccion sistema operativo");
            totalFrases = 1;
        }
        return;
    }
    char buf[MAX_LEN_FRASE];
    while (totalFrases < MAX_FRASES && fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 3)
            strncpy(frases[totalFrases++], buf, MAX_LEN_FRASE - 1);
    }
    fclose(f);
    if (totalFrases == 0) {
        snprintf(frases[0], MAX_LEN_FRASE,
                 "frase de prueba del sistema operativo");
        totalFrases = 1;
    }
}

/* ─── Verifica si una palabra está en las páginas RAM del proceso ─────────── */
static int palabraEnRAM(Proceso *p, const char *palabra)
{
    for (int pg = 0; pg < p->numPaginas; pg++) {
        int marco = p->paginasEnRAM[pg];
        if (marco < 0) continue;
        Pagina *pag = &memoriaPrincipal.marcos[marco];
        for (int w = 0; w < pag->numPalabras; w++) {
            if (strcmp(pag->palabras[w], palabra) == 0) {
                /* Marcar como referenciada y modificada (simulación de lectura/escritura) */
                pag->bitR = 1;
                pag->bitM = 1;                          /* FIX: bitM activo en acceso */
                p->bitReferencia[pg] = 1;
                p->bitModificado[pg] = 1;               /* FIX: sincronizar con BCP   */
                return 1;
            }
        }
    }
    return 0;
}

/*
 * procesarFraseES — CORREGIDO
 *
 * BUGS corregidos:
 *   1. El doble avance de tok dentro del bucle de swap fue eliminado.
 *      El strtok se avanza UNA sola vez al final del bucle externo.
 *   2. bitM ahora se activa en palabraEnRAM (acceso = lectura/escritura simulada).
 *   3. La búsqueda en SWAP ya no consume tokens adicionales al iterar
 *      entradas que no pertenecen al proceso.
 */
/*
 * procesarFraseES — CORRECCIÓN DEFINITIVA
 *
 * Problema raíz identificado: las frases sintéticas usan palabras con
 * índices 0..19 del banco, pero las páginas del proceso usan palabras
 * desde posiciones mucho más avanzadas del cursor. Nunca había coincidencia,
 * por eso fallosPagina = 0 siempre.
 *
 * Solución: construir la frase mezclando:
 *   - 50% palabras que SÍ están en páginas RAM del proceso  → hit  (bitR/bitM)
 *   - 50% palabras que están en SWAP del proceso            → fallo de página
 *
 * Esto garantiza que NRU recibe trabajo real en cada llamada a E/S.
 * Si el proceso no tiene páginas en SWAP, se usa la frase del archivo
 * como fallback (comportamiento original sin fallos).
 */
void procesarFraseES(Proceso *p, int cicloActual)
{
    if (p->numPaginas == 0) return;

    int ip = (int)(p - tablaSistema.tablaBCPs);

    /* ── Recolectar palabras en RAM de este proceso ─────────────────────── */
    /* Tomamos hasta 3 palabras de páginas RAM para el "hit" */
    char palabrasRAM[3][MAX_LEN_PALABRA];
    int  nRAM = 0;
    for (int pg = 0; pg < p->numPaginas && nRAM < 3; pg++) {
        int marco = p->paginasEnRAM[pg];
        if (marco < 0) continue;
        Pagina *pag = &memoriaPrincipal.marcos[marco];
        if (pag->numPalabras > 0) {
            strncpy(palabrasRAM[nRAM], pag->palabras[0], MAX_LEN_PALABRA - 1);
            nRAM++;
        }
    }

    /* ── Recolectar palabras en SWAP de este proceso ────────────────────── */
    /* Tomamos hasta 3 palabras de páginas SWAP para generar fallos */
    char palabrasSWAP[3][MAX_LEN_PALABRA];
    int  paginasSWAP[3];
    int  nSWAP = 0;
    for (int s = 0; s < areaSwap.numPaginas && nSWAP < 3; s++) {
        if (areaSwap.paginas[s].indiceProceso != ip) continue;
        if (areaSwap.paginas[s].numPalabras > 0) {
            strncpy(palabrasSWAP[nSWAP],
                    areaSwap.paginas[s].palabras[0], MAX_LEN_PALABRA - 1);
            paginasSWAP[nSWAP] = areaSwap.paginas[s].indicePagina;
            nSWAP++;
        }
    }

    /* ── Si no hay páginas en SWAP: solo marcar bits R/M en RAM ─────────── */
    if (nSWAP == 0) {
        /* Sin swap no hay fallos posibles; marcar las páginas RAM */
        for (int pg = 0; pg < p->numPaginas; pg++) {
            int marco = p->paginasEnRAM[pg];
            if (marco < 0) continue;
            memoriaPrincipal.marcos[marco].bitR = 1;
            memoriaPrincipal.marcos[marco].bitM = 1;
            p->bitReferencia[pg] = 1;
            p->bitModificado[pg] = 1;
        }
        return;
    }

    /* ── Procesar palabras RAM: marcar bitR y bitM ──────────────────────── */
    for (int i = 0; i < nRAM; i++) {
        palabraEnRAM(p, palabrasRAM[i]);   /* marca bitR=1, bitM=1 internamente */
    }

    /* ── Procesar palabras SWAP: generar fallo de página por cada una ───── */
    for (int i = 0; i < nSWAP; i++) {
        /* Verificar que la página sigue en SWAP (no fue traída antes) */
        if (p->paginasEnRAM[paginasSWAP[i]] >= 0) continue; /* ya en RAM */

        manejarFalloPagina(p, paginasSWAP[i], cicloActual);
        p->fallosPagina++;
        tablaSistema.totalFallosPagina++;
        (void)palabrasSWAP[i]; /* referencia usada para documentación */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   INICIALIZACIÓN DE PROCESOS
   ═══════════════════════════════════════════════════════════════════════════ */
static int tiempoLlegadaUnico(void)
{
    int t;
    do { t = rand() % 801; } while (tiemposUsados[t]);
    tiemposUsados[t] = 1;
    return t;
}

static int potencia2Suficiente(int kb)
{
    int p = 4;
    while (p < kb) p <<= 1;
    return p;
}

static void generarCrecimientoMem(Proceso *p)
{
    memset(p->crecimientoMem, 0, sizeof(p->crecimientoMem));
    int usadas[20] = {0}, colocados = 0;
    while (colocados < 5) {
        int pos = rand() % 20;
        if (!usadas[pos]) {
            p->crecimientoMem[pos] = rand() % 50 + 1;
            usadas[pos] = 1;
            colocados++;
        }
    }
    p->indiceCrecimiento = 0;
}

static void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));
    char letra = 'A' + (index % 26);
    snprintf(p->id,     sizeof(p->id),     "%c-%d", letra, index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);
    p->tiempoLlegada   = tiempoLlegadaUnico();
    p->ciclosTotales   = rand() % 85001;       /* 0-85000 según enunciado */
    if (p->ciclosTotales < 500) p->ciclosTotales = 500; /* mínimo útil */
    p->ciclosRestantes = p->ciclosTotales;
    p->rafagaActual    = rand() % 61 + 10;
    p->cambiosContexto = rand() % 21 + 10;
    p->tipoProceso     = rand() % 2;
    p->estado          = ESTADO_LISTO;
    p->dispositivoES   = -1;
    p->variable1       = rand() % 1000;
    p->variable2       = rand() % 1000;
    p->memoriaUsadaKB  = rand() % 5 + 2;
    p->numMarcos       = rand() % (MARCOS_MAX - MARCOS_MIN + 1) + MARCOS_MIN;
    if (p->numMarcos % 2 != 0) p->numMarcos++;
    if (p->numMarcos > MARCOS_MAX) p->numMarcos = MARCOS_MAX;
    for (int i = 0; i < MAX_PAGINAS_PROCESO; i++)
        p->paginasEnRAM[i] = -1;
    /* Inicializar array de índices Buddy */
    for (int i = 0; i < MAX_BLOQUES_BUDDY_PROCESO; i++)
        p->idxsBuddy[i] = -1;
    p->numBloquesBuddy = 0;
    generarCrecimientoMem(p);
}

void inicializarTablaSistema(void)
{
    memset(&tablaSistema, 0, sizeof(TablaProcesos));
    memset(tiemposUsados, 0, sizeof(tiemposUsados));
    tablaSistema.totalProcesos       = TOTAL_PROCESOS;
    tablaSistema.procesosEnCiclo     = PROCESOS_EN_CICLO;
    tablaSistema.procesosEnSolicitud = PROCESOS_EN_SOLICITUD;
    tablaSistema.algoritmoActual     = ALG_FCFS;
    tablaSistema.quantumActual       = 20;
    for (int i = 0; i < TOTAL_PROCESOS; i++)
        inicializarProceso(&tablaSistema.tablaBCPs[i], i);
}

/* ─── Mezcla y pobla listas ─────────────────────────────────────────────── */
static void mezclarIndices(int *indices, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j   = rand() % (i + 1);
        int tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
    }
}

static void ordenarSolicitudesPorLlegada(Lista *solicitudes)
{
    int cambiado = 1;
    while (cambiado) {
        cambiado = 0;
        for (Nodo *n = solicitudes->cabeza; n && n->siguiente; n = n->siguiente) {
            Proceso *a = n->proceso, *b = n->siguiente->proceso;
            if (a->tiempoLlegada > b->tiempoLlegada) {
                n->proceso = b; n->siguiente->proceso = a; cambiado = 1;
            }
        }
    }
}

void poblarListas(Lista *enEjecucion, Lista *solicitudes)
{
    int indices[TOTAL_PROCESOS];
    for (int i = 0; i < TOTAL_PROCESOS; i++) indices[i] = i;
    mezclarIndices(indices, TOTAL_PROCESOS);
    for (int i = 0; i < PROCESOS_EN_CICLO; i++)
        insertarEnLista(enEjecucion, &tablaSistema.tablaBCPs[indices[i]]);
    for (int i = PROCESOS_EN_CICLO; i < TOTAL_PROCESOS; i++)
        insertarEnLista(solicitudes, &tablaSistema.tablaBCPs[indices[i]]);
    ordenarSolicitudesPorLlegada(solicitudes);
}

void actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                  Cola *colaListos, SistemaES *es, int reloj)
{
    int sumEspera = 0, sumCiclos = 0, cant = 0;
    int terminados = 0, ejecutando = 0, enES = 0;
    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        sumEspera += p->tiempoEspera;
        sumCiclos += p->ciclosRestantes;
        cant++;
        if (p->estado == ESTADO_TERMINADO)  terminados++;
        if (p->estado == ESTADO_EJECUTANDO) ejecutando++;
        if (p->estado == ESTADO_ESPERA_ES)  enES++;
    }
    tablaSistema.procesosTerminados   = terminados;
    tablaSistema.procesosEjecutando   = ejecutando;
    tablaSistema.procesosBloqueados   = 0;
    tablaSistema.procesosEnES         = enES;
    tablaSistema.procesosEnColaListos = colaListos->tamanio;
    tablaSistema.procesosEnSolicitud  = solicitudes->tamanio;
    tablaSistema.cicloActual          = reloj;
    tablaSistema.sumaEspera           = sumEspera;
    tablaSistema.sumaCiclosRestantes  = sumCiclos;
    tablaSistema.promedioEspera       = cant ? sumEspera / cant : 0;
    tablaSistema.promedioCiclos       = cant ? sumCiclos / cant : 0;
    tablaSistema.memoriaLibreKB       = memoriaBuddy.memoriaLibreKB;
    tablaSistema.desperdicioTotal     = memoriaBuddy.desperdicioInternoTotal;
    (void)es;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LISTA DOBLEMENTE ENLAZADA
   ═══════════════════════════════════════════════════════════════════════════ */
void inicializarLista(Lista *l) { l->cabeza = l->cola = NULL; l->tamanio = 0; }

void insertarEnLista(Lista *l, Proceso *p)
{
    Nodo *n = malloc(sizeof(Nodo));
    n->proceso   = p;
    n->siguiente = NULL;
    n->anterior  = l->cola;
    if (l->cola) l->cola->siguiente = n; else l->cabeza = n;
    l->cola = n;
    l->tamanio++;
}

void eliminarDeLista(Lista *l, Proceso *p)
{
    for (Nodo *n = l->cabeza; n; n = n->siguiente) {
        if (n->proceso == p) {
            if (n->anterior) n->anterior->siguiente = n->siguiente;
            else             l->cabeza = n->siguiente;
            if (n->siguiente) n->siguiente->anterior = n->anterior;
            else              l->cola = n->anterior;
            free(n);
            l->tamanio--;
            return;
        }
    }
}

int estaVaciaLista(Lista *l) { return l->cabeza == NULL; }

/* ═══════════════════════════════════════════════════════════════════════════
   COLA FIFO
   ═══════════════════════════════════════════════════════════════════════════ */
void inicializarCola(Cola *c) { c->frente = c->final = NULL; c->tamanio = 0; }

void encolar(Cola *c, Proceso *p)
{
    NodoCola *n = malloc(sizeof(NodoCola));
    n->proceso  = p;
    n->siguiente = NULL;
    if (!c->final) c->frente = c->final = n;
    else { c->final->siguiente = n; c->final = n; }
    c->tamanio++;
}

void encolarAlFrente(Cola *c, Proceso *p)
{
    NodoCola *n  = malloc(sizeof(NodoCola));
    n->proceso   = p;
    n->siguiente = c->frente;
    c->frente    = n;
    if (!c->final) c->final = n;
    c->tamanio++;
}

Proceso *desencolar(Cola *c)
{
    if (!c->frente) return NULL;
    NodoCola *tmp = c->frente;
    Proceso  *p   = tmp->proceso;
    c->frente = tmp->siguiente;
    if (!c->frente) c->final = NULL;
    free(tmp);
    c->tamanio--;
    return p;
}

int estaVaciaCola(Cola *c) { return c->frente == NULL; }

void moverAlFrenteCola(Cola *c, Proceso *p)
{
    if (!c->frente || c->frente->proceso == p) return;
    NodoCola *prev = NULL, *curr = c->frente;
    while (curr && curr->proceso != p) { prev = curr; curr = curr->siguiente; }
    if (!curr) return;
    if (prev) prev->siguiente = curr->siguiente;
    if (curr == c->final) c->final = prev;
    curr->siguiente = c->frente;
    c->frente = curr;
}

/* ═══════════════════════════════════════════════════════════════════════════
   SISTEMA E/S
   ═══════════════════════════════════════════════════════════════════════════ */
void inicializarSistemaES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

/*
 * asignarES
 * Tiempo base 1-100 ciclos según enunciado, multiplicadores x2,x4,x8,x12.
 * Llama a procesarFraseES para disparar fallos de página (intercambio).
 */
void asignarES(Proceso *p, SistemaES *es)
{
    int mults[4] = { 2, 4, 8, 12 };
    int tipo = rand() % 4;
    int base = rand() % 100 + 1;   /* 1-100 según enunciado */

    p->tiempoES      = base * mults[tipo];
    p->dispositivoES = tipo;
    p->estado        = ESTADO_ESPERA_ES;

    /* Simular lectura de frase → puede generar fallos de página NRU */
    procesarFraseES(p, tablaSistema.cicloActual);

    Cola *colaDestino = NULL;
    switch (tipo) {
        case 0: colaDestino = &es->disco;     break;
        case 1: colaDestino = &es->pantalla;  break;
        case 2: colaDestino = &es->teclado;   break;
        case 3: colaDestino = &es->impresora; break;
    }
    if (!colaDestino) return;

    if (p->esApropiativo) encolarAlFrente(colaDestino, p);
    else                  encolar(colaDestino, p);
}

/* ═══════════════════════════════════════════════════════════════════════════
   INGRESO DINÁMICO
   ═══════════════════════════════════════════════════════════════════════════ */
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj)
{
    Nodo *n = solicitudes->cabeza;
    while (n) {
        Nodo    *sig = n->siguiente;
        Proceso *p   = n->proceso;
        if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
            if (p->esApropiativo) encolarAlFrente(colaListos, p);
            else                  encolar(colaListos, p);
            p->estado      = ESTADO_LISTO;
            p->yaIngresado = 1;
            tablaSistema.procesosIngresadosDinam++;
            eliminarDeLista(solicitudes, p);
        }
        n = sig;
    }
}

void actualizarEspera(Cola *colaListos)
{
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
        if (n->proceso->estado == ESTADO_LISTO)
            n->proceso->tiempoEspera++;
}

/* ═══════════════════════════════════════════════════════════════════════════
   BUDDY SYSTEM — COMPLETAMENTE CORREGIDO
   ─────────────────────────────────────────────────────────────────────────
   CORRECCIONES respecto a versión anterior:
   1. El BCP ahora almacena un ARRAY de índices Buddy (idxsBuddy[]).
      Cada bloque asignado (inicial + crecimientos) se registra por separado.
   2. liberarTodosLosBloquesBuddy libera TODOS los bloques del array.
   3. crecerMemoriaProceso agrega el nuevo índice al array sin sobreescribir.
   ═══════════════════════════════════════════════════════════════════════════ */

void inicializarBuddy(void)
{
    memset(&memoriaBuddy, 0, sizeof(MemoriaBuddy));
    pthread_mutex_init(&memoriaBuddy.mutex, NULL);
    memoriaBuddy.bloques[0].tamanioKB    = 1024;
    memoriaBuddy.bloques[0].baseDir      = 0;
    memoriaBuddy.bloques[0].libre        = 1;
    memoriaBuddy.bloques[0].indexProceso = -1;
    memoriaBuddy.bloques[0].socioIdx     = -1;
    memoriaBuddy.numBloques              = 1;
    memoriaBuddy.memoriaLibreKB          = 1024;
}

/* Divide bloque en idx hasta tamaño == targetKB; devuelve idx final */
static int dividirHasta(int idx, int targetKB)
{
    while (memoriaBuddy.bloques[idx].tamanioKB > targetKB) {
        if (memoriaBuddy.numBloques >= 1023) break;

        int mitad = memoriaBuddy.bloques[idx].tamanioKB / 2;
        int nuevo = memoriaBuddy.numBloques;

        memoriaBuddy.bloques[nuevo].tamanioKB    = mitad;
        memoriaBuddy.bloques[nuevo].baseDir      =
            memoriaBuddy.bloques[idx].baseDir + mitad;
        memoriaBuddy.bloques[nuevo].libre        = 1;
        memoriaBuddy.bloques[nuevo].indexProceso = -1;
        memoriaBuddy.bloques[nuevo].socioIdx     = idx;
        memoriaBuddy.numBloques++;

        memoriaBuddy.bloques[idx].tamanioKB = mitad;
        memoriaBuddy.bloques[idx].socioIdx  = nuevo;
    }
    return idx;
}

/*
 * asignarMemoriaBuddy
 * Retorna el índice del bloque asignado, o -1 si no hay memoria.
 * El llamador es responsable de registrar el índice en p->idxsBuddy[].
 */
int asignarMemoriaBuddy(Proceso *p, int memoriaKB)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int target = potencia2Suficiente(memoriaKB);
    int mejor  = -1;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        BloqueBS *b = &memoriaBuddy.bloques[i];
        if (b->libre && b->tamanioKB >= target && b->tamanioKB > 0) {
            if (mejor == -1 ||
                b->tamanioKB < memoriaBuddy.bloques[mejor].tamanioKB)
                mejor = i;
        }
    }
    if (mejor == -1) {
        pthread_mutex_unlock(&memoriaBuddy.mutex);
        return -1;
    }
    int idx = dividirHasta(mejor, target);
    int ip  = (int)(p - tablaSistema.tablaBCPs);

    memoriaBuddy.bloques[idx].libre        = 0;
    memoriaBuddy.bloques[idx].indexProceso = ip;
    memoriaBuddy.memoriaLibreKB           -= target;
    memoriaBuddy.memoriaUsadaKB           += target;
    memoriaBuddy.desperdicioInternoTotal  += target - memoriaKB;

    /* Actualizar campos del proceso */
    p->bloqueMemoriaKB    += target;
    p->desperdicioInterno += target - memoriaKB;

    pthread_mutex_unlock(&memoriaBuddy.mutex);
    return idx;
}

/*
 * liberarUnBloqueBuddy — libera un único bloque por índice y fusiona.
 * Función interna; el mutex debe estar tomado antes de llamarla.
 */
static void liberarUnBloqueBuddy(int idx, int ip, int tamanioOriginal)
{
    if (idx < 0 || idx >= memoriaBuddy.numBloques) return;
    if (memoriaBuddy.bloques[idx].indexProceso != ip) return;

    memoriaBuddy.bloques[idx].libre        = 1;
    memoriaBuddy.bloques[idx].indexProceso = -1;
    memoriaBuddy.memoriaLibreKB           += memoriaBuddy.bloques[idx].tamanioKB;
    memoriaBuddy.memoriaUsadaKB           -= memoriaBuddy.bloques[idx].tamanioKB;
    memoriaBuddy.desperdicioInternoTotal  -= (memoriaBuddy.bloques[idx].tamanioKB
                                              - tamanioOriginal);

    /* Fusión iterativa con socio */
    int actual = idx;
    while (1) {
        int socio = memoriaBuddy.bloques[actual].socioIdx;
        if (socio < 0 || socio >= memoriaBuddy.numBloques) break;

        BloqueBS *bActual = &memoriaBuddy.bloques[actual];
        BloqueBS *bSocio  = &memoriaBuddy.bloques[socio];

        if (!bSocio->libre || bSocio->tamanioKB != bActual->tamanioKB ||
            bSocio->tamanioKB == 0)
            break;

        if (bActual->baseDir < bSocio->baseDir) {
            bActual->tamanioKB *= 2;
            bActual->socioIdx   = bSocio->socioIdx;
            if (bSocio->socioIdx >= 0)
                memoriaBuddy.bloques[bSocio->socioIdx].socioIdx = actual;
            bSocio->tamanioKB = 0;
            bSocio->libre     = 0;
        } else {
            bSocio->tamanioKB *= 2;
            bSocio->socioIdx   = bActual->socioIdx;
            if (bActual->socioIdx >= 0)
                memoriaBuddy.bloques[bActual->socioIdx].socioIdx = socio;
            bActual->tamanioKB = 0;
            bActual->libre     = 0;
            actual = socio;
        }
    }
}

/*
 * liberarTodosLosBloquesBuddy — CORREGIDO
 * Libera TODOS los bloques del array idxsBuddy[] del proceso.
 * Resuelve el memory leak de la versión anterior que solo liberaba el último.
 */
void liberarTodosLosBloquesBuddy(Proceso *p)
{
    if (p->numBloquesBuddy == 0) return;
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int ip = (int)(p - tablaSistema.tablaBCPs);
    for (int i = 0; i < p->numBloquesBuddy; i++) {
        int idx = p->idxsBuddy[i];
        if (idx < 0) continue;
        /* Calcular el tamaño original aproximado para el desperdicio.
           Se usa el tamaño del bloque actual (ya es potencia de 2). */
        liberarUnBloqueBuddy(idx, ip, memoriaBuddy.bloques[idx].tamanioKB);
        p->idxsBuddy[i] = -1;
    }
    p->numBloquesBuddy    = 0;
    p->bloqueMemoriaKB    = 0;
    p->desperdicioInterno = 0;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
}

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORIA LEGACY (SLOTS)
   ═══════════════════════════════════════════════════════════════════════════ */
void inicializarMemoriaPrincipal(void)
{
    memset(&memoriaPrincipalLegacy, 0, sizeof(MemoriaPrincipalLegacy));
    for (int i = 0; i < PROCESOS_EN_CICLO; i++) {
        memoriaPrincipalLegacy.slots[i].ocupado       = 0;
        memoriaPrincipalLegacy.slots[i].indiceProceso = -1;
    }
}

int asignarSlotMemoria(Proceso *p)
{
    int slot = -1;
    for (int i = 0; i < PROCESOS_EN_CICLO; i++)
        if (!memoriaPrincipalLegacy.slots[i].ocupado) { slot = i; break; }
    if (slot == -1) return -1;

    SlotMemoria *s       = &memoriaPrincipalLegacy.slots[slot];
    s->ocupado           = 1;
    s->indiceProceso     = (int)(p - tablaSistema.tablaBCPs);
    s->numPalabras       = 0;
    s->capacidadPalabras = p->bloqueMemoriaKB * 10;
    if (s->capacidadPalabras > MAX_PALABRAS_POR_SLOT)
        s->capacidadPalabras = MAX_PALABRAS_POR_SLOT;
    agregarPalabrasAlSlot(p, p->memoriaUsadaKB * 10);
    memoriaPrincipalLegacy.numSlotsOcupados++;
    memoriaPrincipalLegacy.procesosEnEjecucion++;
    return slot;
}

void agregarPalabrasAlSlot(Proceso *p, int cantidad)
{
    SlotMemoria *s = NULL;
    for (int i = 0; i < PROCESOS_EN_CICLO; i++) {
        if (memoriaPrincipalLegacy.slots[i].ocupado &&
            memoriaPrincipalLegacy.slots[i].indiceProceso ==
                (int)(p - tablaSistema.tablaBCPs)) {
            s = &memoriaPrincipalLegacy.slots[i];
            break;
        }
    }
    if (!s) return;
    for (int i = 0; i < cantidad; i++) {
        if (s->numPalabras >= s->capacidadPalabras ||
            bancoPalabras.totalPalabras == 0) break;
        strncpy(s->palabras[s->numPalabras],
                bancoPalabras.palabras[bancoPalabras.cursor],
                MAX_LEN_PALABRA - 1);
        s->numPalabras++;
        bancoPalabras.cursor =
            (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
    }
}

void liberarSlotMemoria(Proceso *p)
{
    for (int i = 0; i < PROCESOS_EN_CICLO; i++) {
        SlotMemoria *s = &memoriaPrincipalLegacy.slots[i];
        if (s->ocupado &&
            s->indiceProceso == (int)(p - tablaSistema.tablaBCPs)) {
            memoriaPrincipalLegacy.tiempoTotalEjecucion += p->tiempoEjecucion;
            memoriaPrincipalLegacy.procesosTerminados++;
            memoriaPrincipalLegacy.procesosEnEjecucion--;
            memoriaPrincipalLegacy.numSlotsOcupados--;
            memset(s, 0, sizeof(SlotMemoria));
            s->ocupado       = 0;
            s->indiceProceso = -1;
            return;
        }
    }
}

/*
 * crecerMemoriaProceso — CORREGIDO
 * Agrega el nuevo índice Buddy al array, sin sobreescribir los anteriores.
 */
void crecerMemoriaProceso(Proceso *p)
{
    if (p->bloqueMemoriaKB == 0) return;
    if (p->numBloquesBuddy >= MAX_BLOQUES_BUDDY_PROCESO) return; /* array lleno */

    int extra = p->crecimientoMem[p->indiceCrecimiento % 20];
    p->indiceCrecimiento++;
    if (extra <= 0) return;

    int idx = asignarMemoriaBuddy(p, extra);
    if (idx < 0) return; /* sin memoria disponible */

    /* Registrar el nuevo bloque en el array del BCP */
    p->idxsBuddy[p->numBloquesBuddy] = idx;
    p->numBloquesBuddy++;

    agregarPalabrasAlSlot(p, extra * 10);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PAGINACIÓN + NRU
   ═══════════════════════════════════════════════════════════════════════════ */
void inicializarPaginacion(void)
{
    memset(&memoriaPrincipal, 0, sizeof(MemoriaPrincipal));
    memset(&areaSwap,         0, sizeof(AreaSwap));
    memoriaPrincipal.numMarcosTotal = PROCESOS_EN_CICLO * MARCOS_MAX;
    for (int i = 0; i < memoriaPrincipal.numMarcosTotal; i++) {
        memoriaPrincipal.marcos[i].indiceProceso = -1;
        memoriaPrincipal.marcos[i].indicePagina  = -1;
    }
}

void asignarPaginasProceso(Proceso *p)
{
    int totalPalabras = p->memoriaUsadaKB * 10;
    p->numPaginas = (totalPalabras + PALABRAS_POR_PAGINA - 1) /
                    PALABRAS_POR_PAGINA;
    if (p->numPaginas > MAX_PAGINAS_PROCESO)
        p->numPaginas = MAX_PAGINAS_PROCESO;
    if (p->numPaginas == 0) p->numPaginas = 1;

    int ip = (int)(p - tablaSistema.tablaBCPs);
    int marcosAsignados = 0;

    for (int pg = 0; pg < p->numPaginas; pg++) {
        p->paginasEnRAM[pg]  = -1;
        p->bitReferencia[pg] = 0;
        p->bitModificado[pg] = 0;
    }

    /* Cargar las primeras numMarcos páginas en RAM */
    for (int pg = 0; pg < p->numPaginas && marcosAsignados < p->numMarcos; pg++) {
        int marco = -1;
        for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
            if (memoriaPrincipal.marcos[m].indiceProceso == -1) {
                marco = m; break;
            }
        }
        if (marco == -1) break;

        Pagina *pag = &memoriaPrincipal.marcos[marco];
        pag->indiceProceso = ip;
        pag->indicePagina  = pg;
        pag->bitR          = 0;
        pag->bitM          = 0;
        pag->numPalabras   = 0;
        pag->tiempoEntrada = tablaSistema.cicloActual;
        for (int w = 0; w < PALABRAS_POR_PAGINA &&
             bancoPalabras.totalPalabras > 0; w++) {
            strncpy(pag->palabras[w],
                    bancoPalabras.palabras[bancoPalabras.cursor],
                    MAX_LEN_PALABRA - 1);
            pag->numPalabras++;
            bancoPalabras.cursor =
                (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
        }
        p->paginasEnRAM[pg] = marco;
        memoriaPrincipal.numMarcosOcupados++;
        marcosAsignados++;
    }

    /* Páginas restantes van a SWAP */
    for (int pg = marcosAsignados; pg < p->numPaginas; pg++) {
        if (areaSwap.numPaginas >= MAX_PAGINAS_SWAP) break;
        Pagina *sp = &areaSwap.paginas[areaSwap.numPaginas];
        sp->indiceProceso = ip;
        sp->indicePagina  = pg;
        sp->bitR          = 0;
        sp->bitM          = 0;
        sp->numPalabras   = 0;
        sp->tiempoEntrada = 0;
        for (int w = 0; w < PALABRAS_POR_PAGINA &&
             bancoPalabras.totalPalabras > 0; w++) {
            strncpy(sp->palabras[w],
                    bancoPalabras.palabras[bancoPalabras.cursor],
                    MAX_LEN_PALABRA - 1);
            sp->numPalabras++;
            bancoPalabras.cursor =
                (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
        }
        areaSwap.numPaginas++;
    }
}

void liberarPaginasProceso(Proceso *p)
{
    int ip = (int)(p - tablaSistema.tablaBCPs);
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == ip) {
            memoriaPrincipal.marcos[m].indiceProceso = -1;
            memoriaPrincipal.marcos[m].indicePagina  = -1;
            memoriaPrincipal.marcos[m].numPalabras   = 0;
            memoriaPrincipal.marcos[m].bitR          = 0;
            memoriaPrincipal.marcos[m].bitM          = 0;
            memoriaPrincipal.numMarcosOcupados--;
        }
    }
    /* Compactar SWAP eliminando las páginas del proceso */
    int j = 0;
    for (int i = 0; i < areaSwap.numPaginas; i++)
        if (areaSwap.paginas[i].indiceProceso != ip)
            areaSwap.paginas[j++] = areaSwap.paginas[i];
    areaSwap.numPaginas = j;

    for (int i = 0; i < MAX_PAGINAS_PROCESO; i++) {
        p->paginasEnRAM[i]  = -1;
        p->bitReferencia[i] = 0;
        p->bitModificado[i] = 0;
    }
}

/*
 * NRU: clase = bitR*2 + bitM
 *   Clase 0: R=0 M=0  (candidato preferido)
 *   Clase 1: R=0 M=1
 *   Clase 2: R=1 M=0
 *   Clase 3: R=1 M=1  (último en elegir)
 * En empate de clase se elige el marco con tiempoEntrada más antiguo.
 */
static int seleccionarVictimaNRU(void)
{
    int victima = -1, claseVictima = 4;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == -1) continue;
        int clase = memoriaPrincipal.marcos[m].bitR * 2 +
                    memoriaPrincipal.marcos[m].bitM;
        if (clase < claseVictima ||
           (clase == claseVictima && victima >= 0 &&
            memoriaPrincipal.marcos[m].tiempoEntrada <
            memoriaPrincipal.marcos[victima].tiempoEntrada)) {
            claseVictima = clase;
            victima      = m;
        }
    }
    return victima;
}

int manejarFalloPagina(Proceso *p, int indicePagina, int cicloActual)
{
    /* Buscar marco libre */
    int marco = -1;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == -1) {
            marco = m; break;
        }
    }

    int marcoVictima = -1;
    if (marco == -1) {
        /* Sin marco libre: NRU selecciona víctima */
        marcoVictima = seleccionarVictimaNRU();
        if (marcoVictima == -1) return -1;

        Pagina *victima = &memoriaPrincipal.marcos[marcoVictima];

        /* Enviar víctima a SWAP si hay espacio */
        if (areaSwap.numPaginas < MAX_PAGINAS_SWAP) {
            areaSwap.paginas[areaSwap.numPaginas] = *victima;
            areaSwap.numPaginas++;
        }

        /* Actualizar tabla de páginas del proceso víctima */
        int ipV = victima->indiceProceso;
        int pgV = victima->indicePagina;
        if (ipV >= 0 && ipV < TOTAL_PROCESOS &&
            pgV >= 0 && pgV < MAX_PAGINAS_PROCESO) {
            tablaSistema.tablaBCPs[ipV].paginasEnRAM[pgV] = -1;
            tablaSistema.tablaBCPs[ipV].bitReferencia[pgV] = 0;
            tablaSistema.tablaBCPs[ipV].bitModificado[pgV] = 0;
        }
        marco = marcoVictima;
    }

    /* Buscar si la página requerida está en SWAP */
    int swapIdx = -1;
    int ip      = (int)(p - tablaSistema.tablaBCPs);
    for (int s = 0; s < areaSwap.numPaginas; s++) {
        if (areaSwap.paginas[s].indiceProceso == ip &&
            areaSwap.paginas[s].indicePagina  == indicePagina) {
            swapIdx = s; break;
        }
    }

    Pagina *destino = &memoriaPrincipal.marcos[marco];
    if (swapIdx >= 0) {
        /* Traer de SWAP */
        *destino = areaSwap.paginas[swapIdx];
        for (int s = swapIdx; s < areaSwap.numPaginas - 1; s++)
            areaSwap.paginas[s] = areaSwap.paginas[s + 1];
        areaSwap.numPaginas--;
    } else {
        /* Crear nueva página */
        destino->indiceProceso = ip;
        destino->indicePagina  = indicePagina;
        destino->numPalabras   = 0;
        for (int w = 0; w < PALABRAS_POR_PAGINA &&
             bancoPalabras.totalPalabras > 0; w++) {
            strncpy(destino->palabras[w],
                    bancoPalabras.palabras[bancoPalabras.cursor],
                    MAX_LEN_PALABRA - 1);
            destino->numPalabras++;
            bancoPalabras.cursor =
                (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
        }
    }

    destino->bitR          = 1;
    destino->bitM          = 0;    /* recién traída: no modificada aún */
    destino->tiempoEntrada = cicloActual;

    if (marcoVictima == -1) memoriaPrincipal.numMarcosOcupados++;
    p->paginasEnRAM[indicePagina]  = marco;
    p->bitReferencia[indicePagina] = 1;
    p->bitModificado[indicePagina] = 0;
    return marco;
}

/*
 * resetarBitsR — cada 50 ciclos, según algoritmo NRU.
 * Reset del bit R en marcos RAM y en BCP del proceso.
 */
void resetarBitsR(int cicloActual)
{
    if (cicloActual % 50 != 0) return;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++)
        memoriaPrincipal.marcos[m].bitR = 0;
    for (int i = 0; i < TOTAL_PROCESOS; i++)
        for (int pg = 0; pg < tablaSistema.tablaBCPs[i].numPaginas; pg++)
            tablaSistema.tablaBCPs[i].bitReferencia[pg] = 0;
}

void redimensionarMemoriaPrincipal(Lista *enEjecucion, int cicloActual)
{
    int cont = 0, total = enEjecucion->tamanio;
    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente, cont++) {
        Proceso *p = n->proceso;
        if (p->estado == ESTADO_TERMINADO || p->numMarcos == 0) continue;

        if (cont < total / 2) {
            /* Primera mitad: reducir páginas a la mitad */
            int nuevos   = p->numMarcos / 2;
            if (nuevos < MARCOS_MIN) nuevos = MARCOS_MIN;
            int liberados = 0;
            for (int pg = p->numPaginas - 1;
                 pg >= 0 && liberados < (p->numMarcos - nuevos); pg--) {
                int m = p->paginasEnRAM[pg];
                if (m < 0) continue;
                if (areaSwap.numPaginas < MAX_PAGINAS_SWAP) {
                    areaSwap.paginas[areaSwap.numPaginas] =
                        memoriaPrincipal.marcos[m];
                    areaSwap.numPaginas++;
                }
                memoriaPrincipal.marcos[m].indiceProceso = -1;
                memoriaPrincipal.marcos[m].indicePagina  = -1;
                memoriaPrincipal.marcos[m].numPalabras   = 0;
                memoriaPrincipal.numMarcosOcupados--;
                p->paginasEnRAM[pg]  = -1;
                p->bitReferencia[pg] = 0;
                p->bitModificado[pg] = 0;
                liberados++;
            }
            p->numMarcos = nuevos;
        } else {
            /* Segunda mitad: duplicar páginas */
            int nuevos = p->numMarcos * 2;
            if (nuevos > MARCOS_MAX) nuevos = MARCOS_MAX;
            int extra  = nuevos - p->numMarcos;
            for (int pg = 0; pg < p->numPaginas && extra > 0; pg++) {
                if (p->paginasEnRAM[pg] >= 0) continue;
                manejarFalloPagina(p, pg, cicloActual);
                extra--;
            }
            p->numMarcos = nuevos;
        }
    }
    printf("[MEM] Redimension aplicada en ciclo %d\n", cicloActual);
}

/* ═══════════════════════════════════════════════════════════════════════════
   ESTADÍSTICAS DE MEMORIA
   ═══════════════════════════════════════════════════════════════════════════ */
void calcularDesperdicioExterno(void)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int libres[1024], nLibres = 0;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        BloqueBS *b = &memoriaBuddy.bloques[i];
        if (b->libre && b->tamanioKB > 0)
            libres[nLibres++] = b->tamanioKB;
    }
    if (nLibres <= 1) {
        memoriaPrincipalLegacy.desperdicioExterno = 0;
        pthread_mutex_unlock(&memoriaBuddy.mutex);
        return;
    }
    int maxLibre = 0, totalLibre = 0;
    for (int i = 0; i < nLibres; i++) {
        totalLibre += libres[i];
        if (libres[i] > maxLibre) maxLibre = libres[i];
    }
    memoriaPrincipalLegacy.desperdicioExterno = totalLibre - maxLibre;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
}

void actualizarPromedioFinalizados(int cicloActual)
{
    int term = memoriaPrincipalLegacy.procesosTerminados;
    if (term == 0 || cicloActual == 0)
        memoriaPrincipalLegacy.promedioFinalizadosPorCiclo = 0.0f;
    else
        memoriaPrincipalLegacy.promedioFinalizadosPorCiclo =
            (float)term / (float)cicloActual;
}

void mostrarEstadisticasMemoria(void)
{
    int term = memoriaPrincipalLegacy.procesosTerminados;
    printf("\n--- ESTADISTICAS MEMORIA ---\n");
    printf("Desperdicio interno Buddy : %d KB\n",
           memoriaBuddy.desperdicioInternoTotal);
    printf("Desperdicio externo       : %d KB\n",
           memoriaPrincipalLegacy.desperdicioExterno);
    printf("Memoria libre Buddy       : %d KB\n",
           memoriaBuddy.memoriaLibreKB);
    printf("Marcos RAM ocupados       : %d\n",
           memoriaPrincipal.numMarcosOcupados);
    printf("Paginas en SWAP           : %d\n",    areaSwap.numPaginas);
    printf("Fallos de pagina NRU      : %d\n",    tablaSistema.totalFallosPagina);
    printf("Procesos en ejecucion     : %d\n",
           memoriaPrincipalLegacy.procesosEnEjecucion);
    printf("Procesos terminados       : %d\n",    term);
    printf("Prom terminados/ciclo     : %.4f\n",
           memoriaPrincipalLegacy.promedioFinalizadosPorCiclo);
    printf("Prom tiempo ejec proceso  : %d ciclos\n",
           term ? memoriaPrincipalLegacy.tiempoTotalEjecucion / term : 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CPU
   ═══════════════════════════════════════════════════════════════════════════ */
void procesarEntradaCPU(Proceso *p, int reloj)
{
    p->cambiosContexto = rand() % 21 + 10;  /* 10-30, diferente cada vez */
    crecerMemoriaProceso(p);
    p->estado = ESTADO_EJECUTANDO;
    p->vecesEnCPU++;
    p->iteraciones++;
    if (p->vecesEnCPU == 1)
        p->tiempoRespuesta = reloj - p->tiempoLlegada;

    /* Determinar ráfaga de esta instancia (aleatorio 10-70) */
    int estaInstancia = rand() % 61 + 10;
    if (estaInstancia > p->ciclosRestantes)
        estaInstancia = p->ciclosRestantes;
    if (estaInstancia <= 0) estaInstancia = 1;

    p->rafagaActual     = estaInstancia;
    p->ciclosRestantes -= estaInstancia;
    p->tiempoEjecucion += estaInstancia;
    p->restanteQuantum  = tablaSistema.quantumActual;

    /* Marcar páginas como referenciadas y modificadas en esta ejecución */
    for (int pg = 0; pg < p->numPaginas; pg++) {
        if (p->paginasEnRAM[pg] >= 0) {
            memoriaPrincipal.marcos[p->paginasEnRAM[pg]].bitR = 1;
            memoriaPrincipal.marcos[p->paginasEnRAM[pg]].bitM = 1;
            p->bitReferencia[pg] = 1;
            p->bitModificado[pg] = 1;
        }
    }
}

void procesarTerminacion(Proceso *p, int reloj)
{
    p->estado        = ESTADO_TERMINADO;
    p->tiempoRetorno = reloj - p->tiempoLlegada;
    liberarTodosLosBloquesBuddy(p);
    liberarSlotMemoria(p);
    liberarPaginasProceso(p);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CAMBIO AUTOMÁTICO DE ALGORITMO
   Usa 3 variables de tabla + 5 variables del BCP
   ═══════════════════════════════════════════════════════════════════════════ */
int evaluarCambioAlgoritmo(Cola *colaListos, SistemaES *es)
{
    /* Variables de tabla: procesosEnColaListos, procesosEnES, cicloActual */
    int totalEnSistema = tablaSistema.procesosEnColaListos +
                         tablaSistema.procesosEnES + 1;
    if (totalEnSistema <= 0) return tablaSistema.algoritmoActual;

    float propListos = (float)tablaSistema.procesosEnColaListos /
                       (float)totalEnSistema;
    int   enES       = tablaSistema.procesosEnES;

    /* Variables del BCP: tiempoEspera, vecesEnCPU, ciclosRestantes,
                          tipoProceso, rafagaActual */
    int sumEspera = 0, sumVeces = 0, cntES = 0, cantProc = 0;
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        sumEspera += p->tiempoEspera;
        sumVeces  += p->vecesEnCPU;
        if (p->tipoProceso == 1) cntES++;
        cantProc++;
    }
    if (cantProc == 0) return tablaSistema.algoritmoActual;

    float promEspera = (float)sumEspera / cantProc;
    float promVeces  = (float)sumVeces  / cantProc;
    float propES     = (float)cntES     / cantProc;

    int alg = tablaSistema.algoritmoActual;
    if (alg == ALG_FCFS) {
        /* Cambiar a RR si: muchos en cola, espera alta y pocos en E/S */
        if (propListos > 0.5f && promEspera > 200.0f && enES < 5) {
            printf("[ALG] Cambio automatico FCFS->RR "
                   "(espera=%.0f cola=%.0f%%)\n",
                   promEspera, propListos * 100);
            return ALG_RR;
        }
    } else {
        /* Volver a FCFS si: sistema estable, pocos ciclos promedio */
        if (propListos < 0.3f && promVeces > 5.0f && propES < 0.3f) {
            printf("[ALG] Cambio automatico RR->FCFS (sistema estable)\n");
            return ALG_FCFS;
        }
    }
    (void)es;
    return tablaSistema.algoritmoActual;
}