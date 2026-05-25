#include "modelo.h"

// ─────────────────────────────────────────────────────────────────────────────
// GLOBALES
// ─────────────────────────────────────────────────────────────────────────────

TablaProcesos tablaSistema;
MemoriaBuddy  memoriaBuddy;

BancoPalabras   bancoPalabras;
MemoriaPrincipal memoriaPrincipal;

static int tiemposUsados[801];

// UTILIDADES INTERNAS────────────────────────────────────────────────

static int tiempoLlegadaUnico(void)
{
    int t;
    do { t = rand() % 801; } while (tiemposUsados[t]);
    tiemposUsados[t] = 1;
    return t;
}

static int potencia2Suficiente(int kb)
{
    int p = 4;  // base minima 4 KB
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
            usadas[pos]  = 1;
            colocados++;
        }
    }
    p->indiceCrecimiento = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// BCP
// ─────────────────────────────────────────────────────────────────────────────

static void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));

    char letra = 'A' + (index % 26);
    snprintf(p->id,     sizeof(p->id),     "%c-%d", letra, index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);

    p->tiempoLlegada   = tiempoLlegadaUnico();
    p->ciclosTotales   = rand() % 85001;       // 0 - 85000
    p->ciclosRestantes = p->ciclosTotales;
    p->rafagaActual    = rand() % 61 + 10;     // 10 - 70
    p->cambiosContexto = rand() % 21 + 10;     // 10 - 30
    p->tipoProceso     = rand() % 2;           // 0=CPU 1=ES
    p->estado          = 0;
    p->dispositivoES   = -1;
    p->variable1       = -1;
    p->variable2       = -1;
    p->memoriaUsadaKB  = rand() % 5 + 2;      // 2 - 6 KB

    generarCrecimientoMem(p);
}

// ─────────────────────────────────────────────────────────────────────────────
// TABLA DEL SISTEMA
// ─────────────────────────────────────────────────────────────────────────────

void inicializarTablaSistema(void)
{
    memset(&tablaSistema, 0, sizeof(TablaProcesos));
    memset(tiemposUsados, 0, sizeof(tiemposUsados));

    tablaSistema.totalProcesos       = 250;
    tablaSistema.procesosEnCiclo     = 150;
    tablaSistema.procesosEnSolicitud = 100;

    for (int i = 0; i < 250; i++)
        inicializarProceso(&tablaSistema.tablaBCPs[i], i);

    printf("  [INIT] 250 procesos creados\n");
}

/* Mezcla un arreglo de indices — usado solo en poblarListas */
static void mezclarIndices(int *indices, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j        = rand() % (i + 1);
        int tmp      = indices[i];
        indices[i]   = indices[j];
        indices[j]   = tmp;
    }
}

/* Ordena SOLO los procesos de solicitudes por tiempoLlegada */
static void ordenarSolicitudesPorLlegada(Lista *solicitudes)
{
    if (!solicitudes->cabeza) return;

    // Burbuja sobre los nodos de la lista
    int cambiado = 1;
    while (cambiado) {
        cambiado = 0;
        for (Nodo *n = solicitudes->cabeza; n && n->siguiente; n = n->siguiente) {
            Proceso *a = n->proceso;
            Proceso *b = n->siguiente->proceso;
            if (a->tiempoLlegada > b->tiempoLlegada) {
                n->proceso          = b;
                n->siguiente->proceso = a;
                cambiado = 1;
            }
        }
    }
}

void poblarListas(Lista *enEjecucion, Lista *solicitudes)
{
    // 1. Crear arreglo con los 250 indices y mezclarlo aleatoriamente
    int indices[250];
    for (int i = 0; i < 250; i++) indices[i] = i;
    mezclarIndices(indices, 250);

    // 2. Primeros 150 indices al azar -> enEjecucion
    for (int i = 0; i < 150; i++)
        insertarEnLista(enEjecucion, &tablaSistema.tablaBCPs[indices[i]]);

    // 3. Ultimos 100 indices al azar -> solicitudes (sin orden aun)
    for (int i = 150; i < 250; i++)
        insertarEnLista(solicitudes, &tablaSistema.tablaBCPs[indices[i]]);

    // 4. Ordenar solicitudes por tiempoLlegada para el ingreso dinamico
    ordenarSolicitudesPorLlegada(solicitudes);

    printf("  [LISTAS] enEjecucion=%d (aleatorio)  solicitudes=%d (ord. llegada)\n",
           enEjecucion->tamanio, solicitudes->tamanio);
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
// LISTA DOBLEMENTE ENLAZADA
// ─────────────────────────────────────────────────────────────────────────────

void inicializarLista(Lista *l)
{
    l->cabeza  = NULL;
    l->cola    = NULL;
    l->tamanio = 0;
}

void insertarEnLista(Lista *l, Proceso *p)
{
    Nodo *n      = malloc(sizeof(Nodo));
    n->proceso   = p;
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
    NodoCola *n  = malloc(sizeof(NodoCola));
    n->proceso   = p;
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






// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA E/S
// ─────────────────────────────────────────────────────────────────────────────

void inicializarSistemaES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
    printf("  [ES] 4 dispositivos inicializados (disco/pantalla/teclado/impresora)\n");
}

void asignarES(Proceso *p, SistemaES *es)
{
    int mults[4] = { 2, 4, 8, 12 };
    int tipo     = rand() % 4;
    int base     = rand() % 100 + 1;

    p->tiempoES      = base * mults[tipo];
    p->dispositivoES = tipo;
    p->bloqueado     = 1;
    p->estado        = 2;

    switch (tipo) {
        case 0: encolar(&es->disco,     p); break;
        case 1: encolar(&es->pantalla,  p); break;
        case 2: encolar(&es->teclado,   p); break;
        case 3: encolar(&es->impresora, p); break;
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// INGRESO DINAMICO DESDE SOLICITUDES
// ─────────────────────────────────────────────────────────────────────────────

void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj)
{
    for (Nodo *n = solicitudes->cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
            encolar(colaListos, p);
            p->yaIngresado = 1;
            tablaSistema.procesosIngresadosDinam++;
            printf("  [NEW] %s ingresa (llegada=%d)\n", p->id, p->tiempoLlegada);
        }
    }
}

void actualizarEspera(Cola *colaListos)
{
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
        if (n->proceso->estado == 0)
            n->proceso->tiempoEspera++;
}





// ─────────────────────────────────────────────────────────────────────────────
// BUDDY SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

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

    printf("  [BUDDY] 1024 KB totales inicializados\n");
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
        memoriaBuddy.bloques[nuevo].socio        = &memoriaBuddy.bloques[idx];
        memoriaBuddy.bloques[idx].socio          = &memoriaBuddy.bloques[nuevo];

        memoriaBuddy.numBloques++;
    }
    return idx;
}

int asignarMemoriaBuddy(Proceso *p, int memoriaKB)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);

    int target = potencia2Suficiente(memoriaKB);

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
// BANCO DE PALABRAS
// ─────────────────────────────────────────────────────────────────────────────

void cargarPalabras(const char *rutaArchivo)
{
    memset(&bancoPalabras, 0, sizeof(BancoPalabras));

    FILE *f = fopen(rutaArchivo, "r");
    if (!f) {
        // Si no existe el archivo genera palabras de relleno para no crashear
        printf("  [MEM] AVISO: no se encontro %s, usando palabras generadas\n",
               rutaArchivo);
        for (int i = 0; i < MAX_PALABRAS; i++)
            snprintf(bancoPalabras.palabras[i], MAX_LEN_PALABRA, "palabra%d", i);
        bancoPalabras.totalPalabras = MAX_PALABRAS;
        bancoPalabras.cursor        = 0;
        return;
    }

    char buf[MAX_LEN_PALABRA];
    while (bancoPalabras.totalPalabras < MAX_PALABRAS &&
           fscanf(f, "%63s", buf) == 1) {
        strncpy(bancoPalabras.palabras[bancoPalabras.totalPalabras],
                buf, MAX_LEN_PALABRA - 1);
        bancoPalabras.totalPalabras++;
    }
    fclose(f);
    bancoPalabras.cursor = 0;

    printf("  [MEM] %d palabras cargadas desde %s\n",
           bancoPalabras.totalPalabras, rutaArchivo);
}

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA PRINCIPAL
// ─────────────────────────────────────────────────────────────────────────────

void inicializarMemoriaPrincipal(void)
{
    memset(&memoriaPrincipal, 0, sizeof(MemoriaPrincipal));
    for (int i = 0; i < 150; i++) {
        memoriaPrincipal.slots[i].ocupado        = 0;
        memoriaPrincipal.slots[i].indiceProceso  = -1;
        memoriaPrincipal.slots[i].numPalabras    = 0;
        memoriaPrincipal.slots[i].capacidadPalabras = 0;
    }
    printf("  [MEM] MemoriaPrincipal inicializada (150 slots)\n");
}

// Busca un slot libre y lo asigna al proceso.
// Trae palabras iniciales proporcionales a su memoriaUsadaKB.
// Retorna el indice del slot o -1 si no hay espacio.
int asignarSlotMemoria(Proceso *p)
{
    // Buscar slot libre
    int slot = -1;
    for (int i = 0; i < 150; i++) {
        if (!memoriaPrincipal.slots[i].ocupado) { slot = i; break; }
    }
    if (slot == -1) {
        printf("  [MEM] Sin slots libres para %s\n", p->id);
        return -1;
    }

    SlotMemoria *s       = &memoriaPrincipal.slots[slot];
    s->ocupado           = 1;
    s->indiceProceso     = (int)(p - tablaSistema.tablaBCPs);
    s->numPalabras       = 0;
    // capacidad: cada KB alcanza para ~10 palabras (estimado)
    s->capacidadPalabras = p->bloqueMemoriaKB * 10;
    if (s->capacidadPalabras > MAX_PALABRAS_POR_SLOT)
        s->capacidadPalabras = MAX_PALABRAS_POR_SLOT;

    // Traer palabras iniciales segun memoriaUsadaKB
    int palabrasIniciales = p->memoriaUsadaKB * 10;
    agregarPalabrasAlSlot(p, palabrasIniciales);

    memoriaPrincipal.numSlotsOcupados++;
    memoriaPrincipal.procesosEnEjecucion++;

    printf("  [MEM] Slot %d asignado a %s (%d palabras iniciales)\n",
           slot, p->id, palabrasIniciales);
    return slot;
}

// Agrega 'cantidad' palabras del banco al slot del proceso.
// Si el banco se acaba vuelve al inicio (circular).
void agregarPalabrasAlSlot(Proceso *p, int cantidad)
{
    // Encontrar el slot del proceso
    SlotMemoria *s = NULL;
    for (int i = 0; i < 150; i++) {
        if (memoriaPrincipal.slots[i].ocupado &&
            memoriaPrincipal.slots[i].indiceProceso ==
                (int)(p - tablaSistema.tablaBCPs)) {
            s = &memoriaPrincipal.slots[i];
            break;
        }
    }
    if (!s) return;

    for (int i = 0; i < cantidad; i++) {
        if (s->numPalabras >= s->capacidadPalabras) break; // slot lleno
        if (bancoPalabras.totalPalabras == 0)       break; // banco vacio

        // Copia la palabra del banco al slot
        strncpy(s->palabras[s->numPalabras],
                bancoPalabras.palabras[bancoPalabras.cursor],
                MAX_LEN_PALABRA - 1);
        s->numPalabras++;

        // Cursor circular sobre el banco
        bancoPalabras.cursor =
            (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
    }
}

// Llamar cada vez que el proceso entra a CPU.
// Consulta crecimientoMem[indiceCrecimiento], si es > 0 pide mas Buddy
// y trae las palabras correspondientes.
void crecerMemoriaProceso(Proceso *p)
{
    int extra = p->crecimientoMem[p->indiceCrecimiento % 20];
    p->indiceCrecimiento++;

    if (extra <= 0) return; // este turno no crece

    // Pedir mas memoria al Buddy
    int idx = asignarMemoriaBuddy(p, extra);
    if (idx < 0) {
        printf("  [MEM] %s quiso crecer %d KB pero Buddy sin espacio\n",
               p->id, extra);
        return;
    }

    // Traer palabras proporcionales al crecimiento
    int palabrasExtra = extra * 10;
    agregarPalabrasAlSlot(p, palabrasExtra);

    printf("  [MEM] %s crece +%d KB → +%d palabras (slot ahora: %d palabras)\n",
           p->id, extra, palabrasExtra,
           memoriaPrincipal.slots[0].numPalabras); // solo referencial
}

// Libera el slot del proceso cuando termina.
void liberarSlotMemoria(Proceso *p)
{
    for (int i = 0; i < 150; i++) {
        SlotMemoria *s = &memoriaPrincipal.slots[i];
        if (s->ocupado &&
            s->indiceProceso == (int)(p - tablaSistema.tablaBCPs)) {

            // Actualizar estadisticas antes de limpiar
            memoriaPrincipal.tiempoTotalEjecucion += p->tiempoEjecucion;
            memoriaPrincipal.procesosTerminados++;
            memoriaPrincipal.procesosEnEjecucion--;
            memoriaPrincipal.numSlotsOcupados--;

            // Limpiar el slot
            memset(s, 0, sizeof(SlotMemoria));
            s->ocupado       = 0;
            s->indiceProceso = -1;

            printf("  [MEM] Slot liberado de %s\n", p->id);
            return;
        }
    }
}

// Muestra estadisticas de rendimiento de la MemoriaPrincipal + Buddy
void mostrarEstadisticasMemoria(void)
{
    int term = memoriaPrincipal.procesosTerminados;

    printf("\n  === ESTADISTICAS MEMORIA ===\n");
    printf("  Desperdicio interno (Buddy)  : %d KB\n",
           memoriaBuddy.desperdicioInternoTotal);
    printf("  Memoria libre (Buddy)        : %d KB\n",
           memoriaBuddy.memoriaLibreKB);
    printf("  Memoria usada (Buddy)        : %d KB\n",
           memoriaBuddy.memoriaUsadaKB);
    printf("  Slots ocupados               : %d / 150\n",
           memoriaPrincipal.numSlotsOcupados);
    printf("  Procesos en ejecucion        : %d\n",
           memoriaPrincipal.procesosEnEjecucion);
    printf("  Procesos terminados          : %d\n", term);
    printf("  Prom. tiempo ejec. proceso   : %d ciclos\n",
           term ? memoriaPrincipal.tiempoTotalEjecucion / term : 0);
    printf("  Palabras en banco            : %d\n",
           bancoPalabras.totalPalabras);
}