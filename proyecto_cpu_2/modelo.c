#include <time.h>
#include "modelo.h"

// ── Globales ──────────────────────────────────────────────────────────────────
TablaProcesos tablaSistema;
LibroPalabras libroPalabras;
MemoriaBuddy  memoriaBuddy;

static int tiemposUsados[TIEMPO_LLEGADA_MAX + 1];

// ─────────────────────────────────────────────────────────────────────────────
// UTILIDADES INTERNAS
// ─────────────────────────────────────────────────────────────────────────────

static int tiempoLlegadaUnico(void)
{
    int t;
    do { t = rand() % (TIEMPO_LLEGADA_MAX + 1); } while (tiemposUsados[t]);
    tiemposUsados[t] = 1;
    return t;
}

static int potencia2Suficiente(int kb)
{
    int p = BUDDY_BASE_KB;
    while (p < kb) p <<= 1;
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// BCP
// ─────────────────────────────────────────────────────────────────────────────

void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));

    char letra = 'A' + (index % 26);
    snprintf(p->id,     sizeof(p->id),     "%c-%d", letra, index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);

    p->tiempoLlegada   = tiempoLlegadaUnico();
    p->ciclosTotales   = rand() % (CICLOS_MAX - CICLOS_MIN + 1) + CICLOS_MIN;
    p->ciclosRestantes = p->ciclosTotales;
    p->estado          = 0;
    p->dispositivoES   = -1;
    p->variable1       = -1;
    p->variable2       = -1;
    p->tipoProceso     = rand() % 2;
    p->memoriaUsadaKB  = rand() % 5 + 2;   // 2-6 KB -> buddy asigna 4 u 8 KB

    int m = MARCOS_MIN + (rand() % ((MARCOS_MAX - MARCOS_MIN) / 2 + 1)) * 2;
    inicializarNRU(p, m);
    generarCrecimientoMem(p);
}

void liberarProceso(Proceso *p)
{
    free(p->marcos);      p->marcos     = NULL;
    free(p->paginasSwap); p->paginasSwap = NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// LISTA DOBLE
// ─────────────────────────────────────────────────────────────────────────────

void inicializarLista(Lista *l)
{
    l->cabeza  = NULL;
    l->cola    = NULL;
    l->tamanio = 0;
}

void insertarEnLista(Lista *l, Proceso *p)
{
    Nodo *n    = malloc(sizeof(Nodo));
    n->proceso  = p;
    n->siguiente = NULL;
    n->anterior  = l->cola;

    if (l->cola) l->cola->siguiente = n;
    else         l->cabeza = n;

    l->cola = n;
    l->tamanio++;
}

int estaVaciaLista(Lista *l) { return l->cabeza == NULL; }

// ─────────────────────────────────────────────────────────────────────────────
// COLA FIFO
// ─────────────────────────────────────────────────────────────────────────────

void inicializarCola(Cola *c)
{
    c->frente  = NULL;
    c->final   = NULL;
    c->tamanio = 0;
}

void encolar(Cola *c, Proceso *p)
{
    NodoCola *n = malloc(sizeof(NodoCola));
    n->proceso   = p;
    n->siguiente = NULL;

    if (!c->final) c->frente = c->final = n;
    else { c->final->siguiente = n; c->final = n; }
    c->tamanio++;
}

void encolarAlFrente(Cola *c, Proceso *p)
{
    NodoCola *n = malloc(sizeof(NodoCola));
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

// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA E/S
// ─────────────────────────────────────────────────────────────────────────────

void inicializarSistemaES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

int contarES(SistemaES *es)
{
    return es->disco.tamanio + es->pantalla.tamanio +
           es->teclado.tamanio + es->impresora.tamanio;
}

void asignarES(Proceso *p, SistemaES *es)
{
    int tipo      = rand() % 4;
    int base      = rand() % 100 + 1;
    int mults[4]  = { MULT_DISCO, MULT_PANTALLA, MULT_TECLADO, MULT_IMPRESORA };

    p->tiempoES    = base * mults[tipo];
    p->dispositivoES = tipo;
    p->bloqueado   = 1;
    p->estado      = 2;

    switch (tipo) {
        case 0: encolar(&es->disco,     p); break;
        case 1: encolar(&es->pantalla,  p); break;
        case 2: encolar(&es->teclado,   p); break;
        case 3: encolar(&es->impresora, p); break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TABLA DE PROCESOS
// ─────────────────────────────────────────────────────────────────────────────

void inicializarTablaSistema(void)
{
    memset(&tablaSistema, 0, sizeof(TablaProcesos));
    memset(tiemposUsados, 0, sizeof(tiemposUsados));

    tablaSistema.totalProcesos      = TOTAL_PROCESOS;
    tablaSistema.procesosEnCiclo    = EN_EJECUCION;
    tablaSistema.procesosEnSolicitud = EN_SOLICITUDES;

    for (int i = 0; i < TOTAL_PROCESOS; i++)
        inicializarProceso(&tablaSistema.tablaBCPs[i], i);
}

static void ordenarPorLlegada(void)
{
    for (int i = 0; i < TOTAL_PROCESOS - 1; i++)
        for (int j = i + 1; j < TOTAL_PROCESOS; j++)
            if (tablaSistema.tablaBCPs[i].tiempoLlegada >
                tablaSistema.tablaBCPs[j].tiempoLlegada) {
                Proceso tmp = tablaSistema.tablaBCPs[i];
                tablaSistema.tablaBCPs[i] = tablaSistema.tablaBCPs[j];
                tablaSistema.tablaBCPs[j] = tmp;
            }
}

void poblarListas(Lista *enEjecucion, Lista *solicitudes)
{
    ordenarPorLlegada();

    for (int i = 0; i < EN_EJECUCION; i++) {
        Proceso *p = &tablaSistema.tablaBCPs[i];
        p->estado = 0;
        insertarEnLista(enEjecucion, p);
        int idx = asignarMemoriaBuddy(p, p->memoriaUsadaKB);
        if (idx < 0)
            printf("  [BUDDY] Sin espacio para %s (%d KB)\n", p->id, p->memoriaUsadaKB);
    }

    for (int i = EN_EJECUCION; i < TOTAL_PROCESOS; i++) {
        tablaSistema.tablaBCPs[i].estado = 0;
        insertarEnLista(solicitudes, &tablaSistema.tablaBCPs[i]);
    }
}

void actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                  Cola *colaListos, SistemaES *es, int reloj)
{
    int sumEspera = 0, sumCiclos = 0, cant = 0;
    int terminados = 0, ejecutando = 0, bloq = 0, enES = 0;

    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        sumEspera += p->tiempoEspera;
        sumCiclos += p->ciclosRestantes;
        cant++;
        if (p->estado == 3) terminados++;
        if (p->estado == 1) ejecutando++;
        if (p->estado == 4) bloq++;
        if (p->estado == 2) enES++;
    }

    tablaSistema.procesosTerminados    = terminados;
    tablaSistema.procesosEjecutando    = ejecutando;
    tablaSistema.procesosBloqueados    = bloq;
    tablaSistema.procesosEnES          = enES;
    tablaSistema.procesosEnColaListos  = colaListos->tamanio;
    tablaSistema.procesosEnSolicitud   = solicitudes->tamanio;
    tablaSistema.cicloActual           = reloj;
    tablaSistema.sumaEspera            = sumEspera;
    tablaSistema.sumaCiclosRestantes   = sumCiclos;
    tablaSistema.promedioEspera        = cant ? sumEspera / cant : 0;
    tablaSistema.promedioCiclos        = cant ? sumCiclos / cant : 0;
    tablaSistema.memoriaLibreKB        = memoriaBuddy.memoriaLibreKB;
    tablaSistema.desperdicioTotal      = memoriaBuddy.desperdicioInternoTotal;
    (void)es;
}

// ─────────────────────────────────────────────────────────────────────────────
// LIBRO DE PALABRAS
// ─────────────────────────────────────────────────────────────────────────────

int cargarLibro(const char *ruta)
{
    memset(&libroPalabras, 0, sizeof(LibroPalabras));

    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "  [WARN] No se encontro '%s'. Usando palabras generadas.\n", ruta);
        for (int i = 0; i < MAX_PALABRAS; i++)
            snprintf(libroPalabras.palabras[i], MAX_LEN_PALABRA, "pal%d", i);
        libroPalabras.totalPalabras = MAX_PALABRAS;
        return 0;
    }

    char buf[MAX_LEN_PALABRA];
    int  cnt = 0;
    while (cnt < MAX_PALABRAS && fscanf(f, "%63s", buf) == 1)
        strncpy(libroPalabras.palabras[cnt++], buf, MAX_LEN_PALABRA - 1);

    libroPalabras.totalPalabras = cnt;
    fclose(f);
    printf("  [LIBRO] %d palabras cargadas\n", cnt);
    return cnt;
}

void obtenerPalabras(int inicio, int cantidad,
                     char destino[][MAX_LEN_PALABRA], int *obtenidas)
{
    *obtenidas = 0;
    for (int i = 0; i < cantidad && (inicio + i) < libroPalabras.totalPalabras; i++) {
        strncpy(destino[i], libroPalabras.palabras[inicio + i], MAX_LEN_PALABRA - 1);
        (*obtenidas)++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BUDDY SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

void inicializarBuddy(void)
{
    memset(&memoriaBuddy, 0, sizeof(MemoriaBuddy));
    pthread_mutex_init(&memoriaBuddy.mutex, NULL);

    memoriaBuddy.bloques[0].tamanioKB    = MEMORIA_TOTAL_KB;
    memoriaBuddy.bloques[0].baseDir      = 0;
    memoriaBuddy.bloques[0].libre        = 1;
    memoriaBuddy.bloques[0].indexProceso = -1;
    memoriaBuddy.bloques[0].socio        = NULL;
    memoriaBuddy.numBloques              = 1;
    memoriaBuddy.memoriaLibreKB          = MEMORIA_TOTAL_KB;

    printf("  [BUDDY] %d KB totales\n", MEMORIA_TOTAL_KB);
}

static int dividirHasta(int idx, int targetKB)
{
    while (memoriaBuddy.bloques[idx].tamanioKB > targetKB) {
        int mitad = memoriaBuddy.bloques[idx].tamanioKB / 2;
        int nuevo = memoriaBuddy.numBloques;

        memoriaBuddy.bloques[idx].tamanioKB = mitad;

        memoriaBuddy.bloques[nuevo].tamanioKB    = mitad;
        memoriaBuddy.bloques[nuevo].baseDir      = memoriaBuddy.bloques[idx].baseDir + mitad;
        memoriaBuddy.bloques[nuevo].libre        = 1;
        memoriaBuddy.bloques[nuevo].indexProceso = -1;

        memoriaBuddy.bloques[idx].socio  = &memoriaBuddy.bloques[nuevo];
        memoriaBuddy.bloques[nuevo].socio = &memoriaBuddy.bloques[idx];

        memoriaBuddy.numBloques++;
    }
    return idx;
}

int asignarMemoriaBuddy(Proceso *p, int memoriaKB)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);

    int target = potencia2Suficiente(memoriaKB);
    if (target > MEMORIA_TOTAL_KB) {
        pthread_mutex_unlock(&memoriaBuddy.mutex);
        return -1;
    }

    int mejor = -1;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        if (memoriaBuddy.bloques[i].libre &&
            memoriaBuddy.bloques[i].tamanioKB >= target) {
            if (mejor == -1 ||
                memoriaBuddy.bloques[i].tamanioKB < memoriaBuddy.bloques[mejor].tamanioKB)
                mejor = i;
        }
    }

    if (mejor == -1) {
        pthread_mutex_unlock(&memoriaBuddy.mutex);
        return -1;
    }

    int idx = dividirHasta(mejor, target);

    memoriaBuddy.bloques[idx].libre        = 0;
    memoriaBuddy.bloques[idx].indexProceso = (int)(p - tablaSistema.tablaBCPs);
    memoriaBuddy.memoriaLibreKB           -= target;
    memoriaBuddy.memoriaUsadaKB           += target;
    memoriaBuddy.desperdicioInternoTotal  += target - memoriaKB;

    p->bloqueMemoriaKB    = target;        // bloque buddy asignado (potencia 2)
    p->memoriaUsadaKB     = memoriaKB;     // uso real del proceso
    p->desperdicioInterno = target - memoriaKB;

    pthread_mutex_unlock(&memoriaBuddy.mutex);
    return idx;
}

void liberarMemoriaBuddy(Proceso *p)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);

    int idx = -1;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        if (!memoriaBuddy.bloques[i].libre &&
            memoriaBuddy.bloques[i].indexProceso == (int)(p - tablaSistema.tablaBCPs)) {
            idx = i;
            break;
        }
    }

    if (idx == -1) { pthread_mutex_unlock(&memoriaBuddy.mutex); return; }

    memoriaBuddy.bloques[idx].libre        = 1;
    memoriaBuddy.bloques[idx].indexProceso = -1;
    memoriaBuddy.memoriaLibreKB           += memoriaBuddy.bloques[idx].tamanioKB;
    memoriaBuddy.memoriaUsadaKB           -= memoriaBuddy.bloques[idx].tamanioKB;
    memoriaBuddy.desperdicioInternoTotal  -= p->desperdicioInterno;

    // Fusion con socio
    BloqueBS *actual = &memoriaBuddy.bloques[idx];
    while (actual->socio && actual->socio->libre &&
           actual->socio->tamanioKB == actual->tamanioKB) {
        BloqueBS *socio = actual->socio;
        if (actual->baseDir > socio->baseDir) {
            socio->tamanioKB *= 2;
            socio->socio      = actual->socio->socio;
            actual->tamanioKB = 0;
            actual = socio;
        } else {
            actual->tamanioKB *= 2;
            socio->tamanioKB   = 0;
            actual->socio      = socio->socio;
        }
    }

    p->bloqueMemoriaKB    = 0;
    p->desperdicioInterno = 0;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
}

// ─────────────────────────────────────────────────────────────────────────────
// NRU
// ─────────────────────────────────────────────────────────────────────────────

void inicializarNRU(Proceso *p, int numMarcos)
{
    p->numMarcos  = numMarcos;
    p->numPaginas = numMarcos * 4;

    p->marcos = calloc(numMarcos, sizeof(MarcoNRU));
    for (int i = 0; i < numMarcos; i++) {
        p->marcos[i].numeroPagina = -1;
        p->marcos[i].valido       = 0;
    }

    p->paginasSwap = calloc(p->numPaginas, sizeof(int));
    p->numSwap     = 0;
}

void redimensionarNRU(Proceso *p, int nuevosMarcos)
{
    if (nuevosMarcos < MARCOS_MIN) nuevosMarcos = MARCOS_MIN;
    if (nuevosMarcos > MARCOS_MAX) nuevosMarcos = MARCOS_MAX;
    if (nuevosMarcos % 2 != 0)     nuevosMarcos++;

    MarcoNRU *nuevos = calloc(nuevosMarcos, sizeof(MarcoNRU));

    int copiar = (nuevosMarcos < p->numMarcos) ? nuevosMarcos : p->numMarcos;
    for (int i = 0; i < copiar; i++) nuevos[i] = p->marcos[i];
    for (int i = copiar; i < nuevosMarcos; i++) {
        nuevos[i].numeroPagina = -1;
        nuevos[i].valido       = 0;
    }

    free(p->marcos);
    p->marcos    = nuevos;
    p->numMarcos = nuevosMarcos;
}

static int elegirVictima(Proceso *p)
{
    int mejorIdx = -1, mejorClase = 4;
    for (int i = 0; i < p->numMarcos; i++) {
        if (!p->marcos[i].valido) return i;
        int cl = (p->marcos[i].bitR << 1) | p->marcos[i].bitM;
        if (cl < mejorClase) { mejorClase = cl; mejorIdx = i; }
    }
    return mejorIdx;
}

void accederPaginaNRU(Proceso *p, int pagVirtual)
{
    for (int i = 0; i < p->numMarcos; i++) {
        if (p->marcos[i].valido && p->marcos[i].numeroPagina == pagVirtual) {
            p->marcos[i].bitR = 1;
            if (rand() % 3 == 0) p->marcos[i].bitM = 1;
            return;
        }
    }

    p->fallosPagina++;
    int v = elegirVictima(p);

    if (p->marcos[v].valido) {
        p->reemplazosNRU++;
        if (p->numSwap < p->numPaginas)
            p->paginasSwap[p->numSwap++] = p->marcos[v].numeroPagina;
    }

    int ini = pagVirtual * PALABRAS_POR_PAG, ob = 0;
    obtenerPalabras(ini, PALABRAS_POR_PAG, p->marcos[v].palabras, &ob);

    p->marcos[v].numeroPagina = pagVirtual;
    p->marcos[v].bitR         = 1;
    p->marcos[v].bitM         = (rand() % 3 == 0) ? 1 : 0;
    p->marcos[v].valido       = 1;
}

void limpiarBitsR(Proceso *p)
{
    for (int i = 0; i < p->numMarcos; i++)
        p->marcos[i].bitR = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// CRECIMIENTO DE MEMORIA
// ─────────────────────────────────────────────────────────────────────────────

void generarCrecimientoMem(Proceso *p)
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

int siguienteCrecimiento(Proceso *p)
{
    int v = p->crecimientoMem[p->indiceCrecimiento];
    p->indiceCrecimiento = (p->indiceCrecimiento + 1) % 20;
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// REDIMENSION MASIVA
// ─────────────────────────────────────────────────────────────────────────────

void redimensionarMitadProcesos(Lista *enEjecucion)
{
    int i = 0;
    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente, i++) {
        Proceso *p = n->proceso;
        if (p->estado == 3) continue;
        int nuevos = (i % 2 == 0) ? p->numMarcos / 2 : p->numMarcos * 2;
        redimensionarNRU(p, nuevos);
    }
    printf("  [MEM] Redimension aplicada\n");
}