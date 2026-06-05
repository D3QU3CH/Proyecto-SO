#include "modelo.h"

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
static int  totalFrases = 0;
static int  cursorFrase = 0;

void cargarFrases(const char *ruta)
{
    totalFrases = 0;
    FILE *f = fopen(ruta, "r");
    if (!f) {
        for (int i = 0; i < 20; i++)
            snprintf(frases[i], MAX_LEN_FRASE, "el proceso ejecuta instruccion numero %d", i+1);
        totalFrases = 20;
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
        snprintf(frases[0], MAX_LEN_FRASE, "frase de prueba del sistema");
        totalFrases = 1;
    }
}

static int palabraEnRAM(Proceso *p, const char *palabra)
{
    for (int pg = 0; pg < p->numPaginas; pg++) {
        int marco = p->paginasEnRAM[pg];
        if (marco < 0) continue;
        Pagina *pag = &memoriaPrincipal.marcos[marco];
        for (int w = 0; w < pag->numPalabras; w++) {
            if (strcmp(pag->palabras[w], palabra) == 0) {
                pag->bitR = 1;
                p->bitReferencia[pg] = 1;
                return 1;
            }
        }
    }
    return 0;
}

void procesarFraseES(Proceso *p, int cicloActual)
{
    if (totalFrases == 0) return;
    const char *frase = frases[cursorFrase % totalFrases];
    cursorFrase++;
    char buf[MAX_LEN_FRASE];
    strncpy(buf, frase, MAX_LEN_FRASE - 1);
    char *tok = strtok(buf, " ,.-;:?!");
    while (tok) {
        if (!palabraEnRAM(p, tok)) {
            int ip = (int)(p - tablaSistema.tablaBCPs);
            int swapIdx = -1;
            for (int s = 0; s < areaSwap.numPaginas; s++) {
                if (areaSwap.paginas[s].indiceProceso == ip) {
                    for (int w = 0; w < areaSwap.paginas[s].numPalabras; w++) {
                        if (strcmp(areaSwap.paginas[s].palabras[w], tok) == 0) {
                            swapIdx = s; break;
                        }
                    }
                }
                if (swapIdx >= 0) break;
            }
            if (swapIdx >= 0) {
                int pgIdx = areaSwap.paginas[swapIdx].indicePagina;
                manejarFalloPagina(p, pgIdx, cicloActual);
                p->fallosPagina++;
                tablaSistema.totalFallosPagina++;
            }
        }
        tok = strtok(NULL, " ,.-;:?!");
    }
}

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
    p->ciclosTotales   = rand() % 85001;
    p->ciclosRestantes = p->ciclosTotales;
    p->rafagaActual    = rand() % 61 + 10;
    p->cambiosContexto = rand() % 21 + 10;
    p->tipoProceso     = rand() % 2;
    p->estado          = ESTADO_LISTO;
    p->dispositivoES   = -1;
    p->variable1       = -1;
    p->variable2       = -1;
    p->memoriaUsadaKB  = rand() % 5 + 2;
    p->numMarcos       = rand() % (MARCOS_MAX - MARCOS_MIN + 1) + MARCOS_MIN;
    if (p->numMarcos % 2 != 0) p->numMarcos++;
    if (p->numMarcos > MARCOS_MAX) p->numMarcos = MARCOS_MAX;
    for (int i = 0; i < MAX_PAGINAS_PROCESO; i++)
        p->paginasEnRAM[i] = -1;
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

static void mezclarIndices(int *indices, int n)
{
    for (int i = n-1; i > 0; i--) {
        int j = rand() % (i+1);
        int tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
    }
}

static void ordenarSolicitudesPorLlegada(Lista *solicitudes)
{
    if (!solicitudes->cabeza) return;
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

// ─── LISTA ────────────────────────────────────────────────────────────────────
void inicializarLista(Lista *l) { l->cabeza = l->cola = NULL; l->tamanio = 0; }

void insertarEnLista(Lista *l, Proceso *p)
{
    Nodo *n = malloc(sizeof(Nodo));
    n->proceso = p; n->siguiente = NULL; n->anterior = l->cola;
    if (l->cola) l->cola->siguiente = n; else l->cabeza = n;
    l->cola = n; l->tamanio++;
}

void eliminarDeLista(Lista *l, Proceso *p)
{
    for (Nodo *n = l->cabeza; n; n = n->siguiente) {
        if (n->proceso == p) {
            if (n->anterior) n->anterior->siguiente = n->siguiente;
            else             l->cabeza = n->siguiente;
            if (n->siguiente) n->siguiente->anterior = n->anterior;
            else              l->cola = n->anterior;
            free(n); l->tamanio--;
            return;
        }
    }
}

int estaVaciaLista(Lista *l) { return l->cabeza == NULL; }

// ─── COLA ─────────────────────────────────────────────────────────────────────
void inicializarCola(Cola *c) { c->frente = c->final = NULL; c->tamanio = 0; }

void encolar(Cola *c, Proceso *p)
{
    NodoCola *n = malloc(sizeof(NodoCola));
    n->proceso = p; n->siguiente = NULL;
    if (!c->final) c->frente = c->final = n;
    else { c->final->siguiente = n; c->final = n; }
    c->tamanio++;
}

void encolarAlFrente(Cola *c, Proceso *p)
{
    NodoCola *n = malloc(sizeof(NodoCola));
    n->proceso = p; n->siguiente = c->frente;
    c->frente = n;
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
    free(tmp); c->tamanio--;
    return p;
}

int estaVaciaCola(Cola *c) { return c->frente == NULL; }

// Mueve un proceso al frente de la cola sin duplicarlo
void moverAlFrenteCola(Cola *c, Proceso *p)
{
    if (!c->frente) return;
    if (c->frente->proceso == p) return; // ya esta al frente

    NodoCola *prev = NULL;
    NodoCola *curr = c->frente;
    while (curr && curr->proceso != p) {
        prev = curr;
        curr = curr->siguiente;
    }
    if (!curr) return; // no esta en la cola

    // Desconectar el nodo
    if (prev) prev->siguiente = curr->siguiente;
    if (curr == c->final) c->final = prev;

    // Poner al frente
    curr->siguiente = c->frente;
    c->frente = curr;
}

// ─── SISTEMA E/S ──────────────────────────────────────────────────────────────
void inicializarSistemaES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

// Asigna proceso a cola E/S con multiplicador x2 x4 x8 x12
// Si el proceso es apropiativo va al frente de la cola E/S
void asignarES(Proceso *p, SistemaES *es)
{
    int mults[4] = { 2, 4, 8, 12 };
    int tipo = rand() % 4;
    int base = rand() % 100 + 1;

    p->tiempoES      = base * mults[tipo];
    p->dispositivoES = tipo;
    p->estado        = ESTADO_ESPERA_ES;

    procesarFraseES(p, tablaSistema.cicloActual);

    Cola *colaDestino = NULL;
    switch (tipo) {
        case 0: colaDestino = &es->disco;     break;
        case 1: colaDestino = &es->pantalla;  break;
        case 2: colaDestino = &es->teclado;   break;
        case 3: colaDestino = &es->impresora; break;
    }
    if (!colaDestino) return;

    // Apropiativo siempre al frente de la cola E/S
    if (p->esApropiativo)
        encolarAlFrente(colaDestino, p);
    else
        encolar(colaDestino, p);
}

// ─── INGRESO DINAMICO ─────────────────────────────────────────────────────────
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj)
{
    Nodo *n = solicitudes->cabeza;
    while (n) {
        Nodo *sig = n->siguiente;
        Proceso *p = n->proceso;
        if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
            if (p->esApropiativo)
                encolarAlFrente(colaListos, p);
            else
                encolar(colaListos, p);
            p->estado = ESTADO_LISTO;
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

// ─── BUDDY SYSTEM ─────────────────────────────────────────────────────────────
void inicializarBuddy(void)
{
    memset(&memoriaBuddy, 0, sizeof(MemoriaBuddy));
    pthread_mutex_init(&memoriaBuddy.mutex, NULL);
    memoriaBuddy.bloques[0].tamanioKB    = 1024;
    memoriaBuddy.bloques[0].baseDir      = 0;
    memoriaBuddy.bloques[0].libre        = 1;
    memoriaBuddy.bloques[0].indexProceso = -1;
    memoriaBuddy.bloques[0].socio        = NULL;
    memoriaBuddy.numBloques              = 1;
    memoriaBuddy.memoriaLibreKB          = 1024;
}

static int dividirHasta(int idx, int targetKB)
{
    while (memoriaBuddy.bloques[idx].tamanioKB > targetKB) {
        int mitad = memoriaBuddy.bloques[idx].tamanioKB / 2;
        int nuevo = memoriaBuddy.numBloques;
        memoriaBuddy.bloques[idx].tamanioKB          = mitad;
        memoriaBuddy.bloques[nuevo].tamanioKB        = mitad;
        memoriaBuddy.bloques[nuevo].baseDir          = memoriaBuddy.bloques[idx].baseDir + mitad;
        memoriaBuddy.bloques[nuevo].libre            = 1;
        memoriaBuddy.bloques[nuevo].indexProceso     = -1;
        memoriaBuddy.bloques[nuevo].socio            = &memoriaBuddy.bloques[idx];
        memoriaBuddy.bloques[idx].socio              = &memoriaBuddy.bloques[nuevo];
        memoriaBuddy.numBloques++;
    }
    return idx;
}

int asignarMemoriaBuddy(Proceso *p, int memoriaKB)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int target = potencia2Suficiente(memoriaKB);
    int mejor  = -1;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        if (memoriaBuddy.bloques[i].libre && memoriaBuddy.bloques[i].tamanioKB >= target) {
            if (mejor == -1 || memoriaBuddy.bloques[i].tamanioKB < memoriaBuddy.bloques[mejor].tamanioKB)
                mejor = i;
        }
    }
    if (mejor == -1) { pthread_mutex_unlock(&memoriaBuddy.mutex); return -1; }
    int idx = dividirHasta(mejor, target);
    memoriaBuddy.bloques[idx].libre        = 0;
    memoriaBuddy.bloques[idx].indexProceso = (int)(p - tablaSistema.tablaBCPs);
    memoriaBuddy.memoriaLibreKB           -= target;
    memoriaBuddy.memoriaUsadaKB           += target;
    memoriaBuddy.desperdicioInternoTotal  += target - memoriaKB;
    p->bloqueMemoriaKB    = target;
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
            idx = i; break;
        }
    }
    if (idx == -1) { pthread_mutex_unlock(&memoriaBuddy.mutex); return; }
    memoriaBuddy.bloques[idx].libre        = 1;
    memoriaBuddy.bloques[idx].indexProceso = -1;
    memoriaBuddy.memoriaLibreKB           += memoriaBuddy.bloques[idx].tamanioKB;
    memoriaBuddy.memoriaUsadaKB           -= memoriaBuddy.bloques[idx].tamanioKB;
    memoriaBuddy.desperdicioInternoTotal  -= p->desperdicioInterno;
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
    p->bloqueMemoriaKB = 0; p->desperdicioInterno = 0;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
}

// ─── BANCO DE PALABRAS ────────────────────────────────────────────────────────
void cargarPalabras(const char *rutaArchivo)
{
    memset(&bancoPalabras, 0, sizeof(BancoPalabras));
    FILE *f = fopen(rutaArchivo, "r");
    if (!f) {
        for (int i = 0; i < MAX_PALABRAS; i++)
            snprintf(bancoPalabras.palabras[i], MAX_LEN_PALABRA, "palabra%d", i);
        bancoPalabras.totalPalabras = MAX_PALABRAS;
        bancoPalabras.cursor = 0;
        return;
    }
    char buf[MAX_LEN_PALABRA];
    while (bancoPalabras.totalPalabras < MAX_PALABRAS && fscanf(f, "%63s", buf) == 1)
        strncpy(bancoPalabras.palabras[bancoPalabras.totalPalabras++], buf, MAX_LEN_PALABRA-1);
    fclose(f);
    bancoPalabras.cursor = 0;
}

// ─── MEMORIA LEGACY (slots) ───────────────────────────────────────────────────
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
    for (int i = 0; i < PROCESOS_EN_CICLO; i++) {
        if (!memoriaPrincipalLegacy.slots[i].ocupado) { slot = i; break; }
    }
    if (slot == -1) return -1;
    SlotMemoria *s   = &memoriaPrincipalLegacy.slots[slot];
    s->ocupado       = 1;
    s->indiceProceso = (int)(p - tablaSistema.tablaBCPs);
    s->numPalabras   = 0;
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
            memoriaPrincipalLegacy.slots[i].indiceProceso == (int)(p - tablaSistema.tablaBCPs)) {
            s = &memoriaPrincipalLegacy.slots[i]; break;
        }
    }
    if (!s) return;
    for (int i = 0; i < cantidad; i++) {
        if (s->numPalabras >= s->capacidadPalabras || bancoPalabras.totalPalabras == 0) break;
        strncpy(s->palabras[s->numPalabras], bancoPalabras.palabras[bancoPalabras.cursor], MAX_LEN_PALABRA-1);
        s->numPalabras++;
        bancoPalabras.cursor = (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
    }
}

void liberarSlotMemoria(Proceso *p)
{
    for (int i = 0; i < PROCESOS_EN_CICLO; i++) {
        SlotMemoria *s = &memoriaPrincipalLegacy.slots[i];
        if (s->ocupado && s->indiceProceso == (int)(p - tablaSistema.tablaBCPs)) {
            memoriaPrincipalLegacy.tiempoTotalEjecucion += p->tiempoEjecucion;
            memoriaPrincipalLegacy.procesosTerminados++;
            memoriaPrincipalLegacy.procesosEnEjecucion--;
            memoriaPrincipalLegacy.numSlotsOcupados--;
            memset(s, 0, sizeof(SlotMemoria));
            s->ocupado = 0; s->indiceProceso = -1;
            return;
        }
    }
}

void crecerMemoriaProceso(Proceso *p)
{
    if (p->bloqueMemoriaKB == 0) return;
    int extra = p->crecimientoMem[p->indiceCrecimiento % 20];
    p->indiceCrecimiento++;
    if (extra <= 0) return;
    int idx = asignarMemoriaBuddy(p, extra);
    if (idx < 0) return;
    agregarPalabrasAlSlot(p, extra * 10);
}

// ─── PAGINACION + NRU ─────────────────────────────────────────────────────────
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
    p->numPaginas = (totalPalabras + PALABRAS_POR_PAGINA - 1) / PALABRAS_POR_PAGINA;
    if (p->numPaginas > MAX_PAGINAS_PROCESO) p->numPaginas = MAX_PAGINAS_PROCESO;
    int ip = (int)(p - tablaSistema.tablaBCPs);
    int marcosAsignados = 0;
    for (int pg = 0; pg < p->numPaginas; pg++) {
        p->paginasEnRAM[pg] = -1; p->bitReferencia[pg] = 0; p->bitModificado[pg] = 0;
    }
    for (int pg = 0; pg < p->numPaginas && marcosAsignados < p->numMarcos; pg++) {
        int marco = -1;
        for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
            if (memoriaPrincipal.marcos[m].indiceProceso == -1) { marco = m; break; }
        }
        if (marco == -1) break;
        Pagina *pag = &memoriaPrincipal.marcos[marco];
        pag->indiceProceso = ip; pag->indicePagina = pg;
        pag->bitR = 0; pag->bitM = 0; pag->numPalabras = 0;
        pag->tiempoEntrada = tablaSistema.cicloActual;
        for (int w = 0; w < PALABRAS_POR_PAGINA && bancoPalabras.totalPalabras > 0; w++) {
            strncpy(pag->palabras[w], bancoPalabras.palabras[bancoPalabras.cursor], MAX_LEN_PALABRA-1);
            pag->numPalabras++;
            bancoPalabras.cursor = (bancoPalabras.cursor+1) % bancoPalabras.totalPalabras;
        }
        p->paginasEnRAM[pg] = marco;
        memoriaPrincipal.numMarcosOcupados++;
        marcosAsignados++;
    }
    for (int pg = marcosAsignados; pg < p->numPaginas; pg++) {
        if (areaSwap.numPaginas >= MAX_PAGINAS_SWAP) break;
        Pagina *sp = &areaSwap.paginas[areaSwap.numPaginas];
        sp->indiceProceso = ip; sp->indicePagina = pg;
        sp->bitR = 0; sp->bitM = 0; sp->numPalabras = 0;
        for (int w = 0; w < PALABRAS_POR_PAGINA && bancoPalabras.totalPalabras > 0; w++) {
            strncpy(sp->palabras[w], bancoPalabras.palabras[bancoPalabras.cursor], MAX_LEN_PALABRA-1);
            sp->numPalabras++;
            bancoPalabras.cursor = (bancoPalabras.cursor+1) % bancoPalabras.totalPalabras;
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
            memoriaPrincipal.numMarcosOcupados--;
        }
    }
    int j = 0;
    for (int i = 0; i < areaSwap.numPaginas; i++)
        if (areaSwap.paginas[i].indiceProceso != ip)
            areaSwap.paginas[j++] = areaSwap.paginas[i];
    areaSwap.numPaginas = j;
    for (int i = 0; i < MAX_PAGINAS_PROCESO; i++) p->paginasEnRAM[i] = -1;
}

// NRU: clase = bitR*2 + bitM, victima = menor clase y mas antigua
static int seleccionarVictimaNRU(void)
{
    int victima = -1, claseVictima = 4;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == -1) continue;
        int clase = memoriaPrincipal.marcos[m].bitR * 2 + memoriaPrincipal.marcos[m].bitM;
        if (clase < claseVictima ||
           (clase == claseVictima && victima >= 0 &&
            memoriaPrincipal.marcos[m].tiempoEntrada < memoriaPrincipal.marcos[victima].tiempoEntrada)) {
            claseVictima = clase; victima = m;
        }
    }
    return victima;
}

int manejarFalloPagina(Proceso *p, int indicePagina, int cicloActual)
{
    int marco = -1;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == -1) { marco = m; break; }
    }
    int marcoVictima = -1;
    if (marco == -1) {
        marcoVictima = seleccionarVictimaNRU();
        if (marcoVictima == -1) return -1;
        Pagina *victima = &memoriaPrincipal.marcos[marcoVictima];
        if (areaSwap.numPaginas < MAX_PAGINAS_SWAP) {
            areaSwap.paginas[areaSwap.numPaginas] = *victima;
            areaSwap.numPaginas++;
        }
        int ipV = victima->indiceProceso, pgV = victima->indicePagina;
        if (ipV >= 0 && ipV < TOTAL_PROCESOS && pgV >= 0)
            tablaSistema.tablaBCPs[ipV].paginasEnRAM[pgV] = -1;
        marco = marcoVictima;
    }
    int swapIdx = -1;
    int ip = (int)(p - tablaSistema.tablaBCPs);
    for (int s = 0; s < areaSwap.numPaginas; s++) {
        if (areaSwap.paginas[s].indiceProceso == ip &&
            areaSwap.paginas[s].indicePagina  == indicePagina) { swapIdx = s; break; }
    }
    Pagina *destino = &memoriaPrincipal.marcos[marco];
    if (swapIdx >= 0) {
        *destino = areaSwap.paginas[swapIdx];
        for (int s = swapIdx; s < areaSwap.numPaginas-1; s++)
            areaSwap.paginas[s] = areaSwap.paginas[s+1];
        areaSwap.numPaginas--;
    } else {
        destino->indiceProceso = ip; destino->indicePagina = indicePagina;
        destino->numPalabras = 0;
        for (int w = 0; w < PALABRAS_POR_PAGINA && bancoPalabras.totalPalabras > 0; w++) {
            strncpy(destino->palabras[w], bancoPalabras.palabras[bancoPalabras.cursor], MAX_LEN_PALABRA-1);
            destino->numPalabras++;
            bancoPalabras.cursor = (bancoPalabras.cursor+1) % bancoPalabras.totalPalabras;
        }
    }
    destino->bitR = 1; destino->bitM = 0; destino->tiempoEntrada = cicloActual;
    if (marcoVictima == -1) memoriaPrincipal.numMarcosOcupados++;
    p->paginasEnRAM[indicePagina] = marco;
    return marco;
}

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
            int nuevos = p->numMarcos / 2;
            if (nuevos < MARCOS_MIN) nuevos = MARCOS_MIN;
            int liberados = 0;
            for (int pg = p->numPaginas-1; pg >= 0 && liberados < (p->numMarcos - nuevos); pg--) {
                int m = p->paginasEnRAM[pg];
                if (m < 0) continue;
                if (areaSwap.numPaginas < MAX_PAGINAS_SWAP) {
                    areaSwap.paginas[areaSwap.numPaginas] = memoriaPrincipal.marcos[m];
                    areaSwap.numPaginas++;
                }
                memoriaPrincipal.marcos[m].indiceProceso = -1;
                memoriaPrincipal.marcos[m].indicePagina  = -1;
                memoriaPrincipal.marcos[m].numPalabras   = 0;
                memoriaPrincipal.numMarcosOcupados--;
                p->paginasEnRAM[pg] = -1;
                liberados++;
            }
            p->numMarcos = nuevos;
        } else {
            int nuevos = p->numMarcos * 2;
            if (nuevos > MARCOS_MAX) nuevos = MARCOS_MAX;
            int extra = nuevos - p->numMarcos;
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

// ─── ESTADISTICAS ─────────────────────────────────────────────────────────────
void calcularDesperdicioExterno(void)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int libres[512], nLibres = 0;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        BloqueBS *b = &memoriaBuddy.bloques[i];
        if (b->libre && b->tamanioKB > 0) libres[nLibres++] = b->tamanioKB;
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
    if (term == 0 || cicloActual == 0) {
        memoriaPrincipalLegacy.promedioFinalizadosPorCiclo = 0.0f; return;
    }
    memoriaPrincipalLegacy.promedioFinalizadosPorCiclo = (float)term / (float)cicloActual;
}

void mostrarEstadisticasMemoria(void)
{
    int term = memoriaPrincipalLegacy.procesosTerminados;
    printf("\n--- ESTADISTICAS MEMORIA ---\n");
    printf("Desperdicio interno Buddy : %d KB\n", memoriaBuddy.desperdicioInternoTotal);
    printf("Desperdicio externo       : %d KB\n", memoriaPrincipalLegacy.desperdicioExterno);
    printf("Memoria libre Buddy       : %d KB\n", memoriaBuddy.memoriaLibreKB);
    printf("Marcos RAM ocupados       : %d\n",    memoriaPrincipal.numMarcosOcupados);
    printf("Paginas en SWAP           : %d\n",    areaSwap.numPaginas);
    printf("Fallos de pagina NRU      : %d\n",    tablaSistema.totalFallosPagina);
    printf("Procesos en ejecucion     : %d\n",    memoriaPrincipalLegacy.procesosEnEjecucion);
    printf("Procesos terminados       : %d\n",    term);
    printf("Prom terminados/ciclo     : %.4f\n",  memoriaPrincipalLegacy.promedioFinalizadosPorCiclo);
    printf("Prom tiempo ejec proceso  : %d ciclos\n",
           term ? memoriaPrincipalLegacy.tiempoTotalEjecucion / term : 0);
}

// ─── CPU ──────────────────────────────────────────────────────────────────────
void procesarEntradaCPU(Proceso *p, int reloj)
{
    p->cambiosContexto = rand() % 21 + 10;
    crecerMemoriaProceso(p);
    p->estado = ESTADO_EJECUTANDO;
    p->vecesEnCPU++;
    p->iteraciones++;
    if (p->vecesEnCPU == 1)
        p->tiempoRespuesta = reloj - p->tiempoLlegada;
    int estaInstancia = rand() % 61 + 10;
    if (estaInstancia > p->ciclosRestantes) estaInstancia = p->ciclosRestantes;
    p->rafagaActual     = estaInstancia;
    p->ciclosRestantes -= estaInstancia;
    p->tiempoEjecucion += estaInstancia;
    p->restanteQuantum  = tablaSistema.quantumActual;
}

void procesarTerminacion(Proceso *p)
{
    p->estado = ESTADO_TERMINADO;
    liberarMemoriaBuddy(p);
    liberarSlotMemoria(p);
    liberarPaginasProceso(p);
}

// ─── CAMBIO AUTOMATICO DE ALGORITMO ──────────────────────────────────────────
// Variables tabla: procesosEnColaListos, procesosEnES, cicloActual
// Variables BCP:   tiempoEspera, ciclosRestantes, vecesEnCPU, rafagaActual, tipoProceso
int evaluarCambioAlgoritmo(Cola *colaListos, SistemaES *es)
{
    int totalEnSistema = tablaSistema.procesosEnColaListos + tablaSistema.procesosEnES + 1;
    if (totalEnSistema <= 0) return tablaSistema.algoritmoActual;
    float propListos = (float)tablaSistema.procesosEnColaListos / (float)totalEnSistema;
    int enES = tablaSistema.procesosEnES;
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
        if (propListos > 0.5f && promEspera > 200.0f && enES < 5) {
            printf("[ALG] Cambio automatico FCFS->RR (espera alta: %.0f, cola: %.0f%%)\n",
                   promEspera, propListos * 100);
            return ALG_RR;
        }
    } else {
        if (propListos < 0.3f && promVeces > 5.0f && propES < 0.3f) {
            printf("[ALG] Cambio automatico RR->FCFS (sistema estable)\n");
            return ALG_FCFS;
        }
    }
    (void)es;
    return tablaSistema.algoritmoActual;
}