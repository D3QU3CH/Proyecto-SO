#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "modelo.h"

// VARIABLES GLOBALES

TablaProcesos    tablaSistema;
MemoriaBuddy     memoriaBuddy;
BancoPalabras    bancoPalabras;
MemoriaPrincipal memoriaPrincipal;
AreaSwap         areaSwap;
EstadisticasMem  estadMem;

static int tiemposUsados[801];

static char frases[MAX_FRASES][MAX_LEN_FRASE];
static int  totalFrases = 0;
static int  cursorFrase = 0;

// BANCO DE PALABRAS
// Carga palabras del archivo o usa palabras de relleno si no existe

void cargarPalabras(const char *ruta)
{
    memset(&bancoPalabras, 0, sizeof(BancoPalabras));
    FILE *f = fopen(ruta, "r");

    if (!f) {
        // Si no hay archivo, generar palabras sinteticas
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
        return;
    }

    // Leer palabras del archivo
    char buf[MAX_LEN_PALABRA];
    while (bancoPalabras.totalPalabras < MAX_PALABRAS &&
           fscanf(f, "%63s", buf) == 1)
        strncpy(bancoPalabras.palabras[bancoPalabras.totalPalabras++],
                buf, MAX_LEN_PALABRA - 1);
    fclose(f);

    // Si el archivo estaba vacio, al menos una palabra
    if (bancoPalabras.totalPalabras == 0) {
        strncpy(bancoPalabras.palabras[0], "palabra", MAX_LEN_PALABRA - 1);
        bancoPalabras.totalPalabras = 1;
    }
}

// FRASES
// Se arman con palabras del banco para que haya
// probabilidad real de fallo de pagina NRU

void cargarFrases(const char *ruta)
{
    totalFrases = 0;
    FILE *f = fopen(ruta, "r");

    if (!f) {
        // Generar frases sinteticas repartidas por todo el banco
        int total = bancoPalabras.totalPalabras;
        for (int i = 0; i < 100 && total > 4; i++) {
            int b = (i * (total / 100));
            snprintf(frases[i], MAX_LEN_FRASE, "%s %s %s %s %s",
                bancoPalabras.palabras[(b)       % total],
                bancoPalabras.palabras[(b + 100) % total],
                bancoPalabras.palabras[(b + 200) % total],
                bancoPalabras.palabras[(b + 300) % total],
                bancoPalabras.palabras[(b + 400) % total]);
            totalFrases++;
        }
        if (totalFrases == 0) {
            snprintf(frases[0], MAX_LEN_FRASE, "proceso ejecuta instruccion sistema operativo");
            totalFrases = 1;
        }
        return;
    }

    // Leer frases del archivo
    char buf[MAX_LEN_FRASE];
    while (totalFrases < MAX_FRASES && fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 3)
            strncpy(frases[totalFrases++], buf, MAX_LEN_FRASE - 1);
    }
    fclose(f);

    if (totalFrases == 0) {
        snprintf(frases[0], MAX_LEN_FRASE, "frase de prueba del sistema operativo");
        totalFrases = 1;
    }
}

// ---------------------------------------------------
// PROCESAR FRASE EN E/S
// Cuando el proceso va a E/S, lee una frase mezclando
// palabras de RAM y SWAP. Las de SWAP causan fallos de pagina NRU.
// Si no hay paginas en SWAP, fuerza al menos 1 fallo.
// ---------------------------------------------------
void procesarFraseES(Proceso *p, int cicloActual)
{
    if (p->numPaginas == 0) return;

    int ip = (int)(p - tablaSistema.tablaBCPs);

    // Recorrer todas las paginas del proceso que estan en SWAP
    for (int s = 0; s < areaSwap.numPaginas; s++) {
        if (areaSwap.paginas[s].indiceProceso != ip) continue;
        if (areaSwap.paginas[s].numPalabras == 0) continue;

        int pgIdx = areaSwap.paginas[s].indicePagina;

        // Cada pagina en SWAP que pertenece al proceso es un fallo de pagina
        int resultado = manejarFalloPagina(p, pgIdx, cicloActual);
        if (resultado >= 0) {
            p->fallosPagina++;
            tablaSistema.totalFallosPagina++;
        }

        // Con 1 fallo por llamada es suficiente para no saturar
        break;
    }

    // Si no habia nada en SWAP, forzar moviendo una pagina de RAM a SWAP
    // para que la proxima llamada si genere fallo
    int ip2 = ip;
    int tieneEnSwap = 0;
    for (int s = 0; s < areaSwap.numPaginas; s++)
        if (areaSwap.paginas[s].indiceProceso == ip2) { tieneEnSwap = 1; break; }

    if (!tieneEnSwap) {
        // Mover la pagina con bitR=0 (clase NRU mas baja) a SWAP
        for (int pg = 0; pg < p->numPaginas; pg++) {
            int m = p->paginasEnRAM[pg];
            if (m < 0) continue;
            if (memoriaPrincipal.marcos[m].bitR == 0 &&
                areaSwap.numPaginas < MAX_PAGINAS_SWAP) {

                areaSwap.paginas[areaSwap.numPaginas] = memoriaPrincipal.marcos[m];
                areaSwap.paginas[areaSwap.numPaginas].indiceProceso = ip;
                areaSwap.paginas[areaSwap.numPaginas].indicePagina  = pg;
                areaSwap.numPaginas++;

                memoriaPrincipal.marcos[m].indiceProceso = -1;
                memoriaPrincipal.marcos[m].indicePagina  = -1;
                memoriaPrincipal.marcos[m].numPalabras   = 0;
                memoriaPrincipal.numMarcosOcupados--;
                p->paginasEnRAM[pg]  = -1;
                p->bitReferencia[pg] = 0;
                break;
            }
        }
    }
}

// INICIALIZACION DE PROCESOS

// Genera un tiempo de llegada unico entre 0 y 800
static int tiempoLlegadaUnico(void)
{
    int t;
    do { t = rand() % 801; } while (tiemposUsados[t]);
    tiemposUsados[t] = 1;
    return t;
}

// Redondea un tamano en KB a la siguiente potencia de 2
static int potencia2(int kb)
{
    int p = BUDDY_MIN_KB;
    while (p < kb) p <<= 1;
    return p;
}

// Genera la lista de crecimiento de memoria: 15 ceros y 5 valores entre 1-50
static void generarCrecimientoMem(Proceso *p)
{
    memset(p->crecimientoMem, 0, sizeof(p->crecimientoMem));
    int usadas[20] = {0};
    int col = 0;
    while (col < 5) {
        int pos = rand() % 20;
        if (!usadas[pos]) {
            p->crecimientoMem[pos] = rand() % 50 + 1;
            usadas[pos] = 1;
            col++;
        }
    }
}

// Inicializa todos los campos de un proceso con valores aleatorios
static void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));
    char letra = 'A' + (index % 26);
    snprintf(p->id,     sizeof(p->id),     "%c-%d", letra, index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);
    p->tiempoLlegada   = tiempoLlegadaUnico();
    p->ciclosTotales   = rand() % 85001;        // 0 a 85000 ciclos
    p->ciclosRestantes = p->ciclosTotales;
    p->rafagaActual    = rand() % 61 + 10;      // 10 a 70
    p->cambiosContexto = rand() % 21 + 10;      // 10 a 30
    p->tipoProceso     = rand() % 2;
    p->estado          = ESTADO_LISTO;
    p->dispositivoES   = -1;
    p->variable1       = rand() % 1000;
    p->variable2       = rand() % 1000;
    p->memoriaUsadaKB  = rand() % 29 + 4;       // 4 a 32 KB
    p->numMarcos       = (rand() % ((MARCOS_MAX - MARCOS_MIN) / 2 + 1)) * 2 + MARCOS_MIN;
    if (p->numMarcos > MARCOS_MAX) p->numMarcos = MARCOS_MAX;
    for (int i = 0; i < MAX_PAGINAS_PROCESO; i++) p->paginasEnRAM[i] = -1;
    for (int i = 0; i < MAX_BLOQUES_BUDDY_PROC; i++) p->idxsBuddy[i] = -1;
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

// Mezcla un arreglo de indices aleatoriamente (Fisher-Yates)
static void mezclarIndices(int *idx, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
    }
}

// Ordena la lista de solicitudes por tiempo de llegada (burbuja)
static void ordenarSolicitudesPorLlegada(Lista *s)
{
    int cambiado = 1;
    while (cambiado) {
        cambiado = 0;
        for (Nodo *n = s->cabeza; n && n->siguiente; n = n->siguiente) {
            Proceso *a = n->proceso, *b = n->siguiente->proceso;
            if (a->tiempoLlegada > b->tiempoLlegada) {
                n->proceso = b; n->siguiente->proceso = a; cambiado = 1;
            }
        }
    }
}

// Reparte los 250 procesos: 150 al ciclo de ejecucion y 100 a solicitudes
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

// ACTUALIZAR VARIABLES GLOBALES

void actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes, Cola *colaListos, SistemaES *es, int reloj)
{
    int sumEspera = 0, sumCiclos = 0, ejecutando = 0;

    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (p->estado != ESTADO_TERMINADO) sumCiclos += p->ciclosRestantes;
        if (p->estado == ESTADO_EJECUTANDO) ejecutando++;
    }

    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
        sumEspera += n->proceso->tiempoEspera;

    int totalES = es->disco.tamanio + es->pantalla.tamanio +
                  es->teclado.tamanio + es->impresora.tamanio;

    tablaSistema.procesosTerminados   = estadMem.procesosTerminados;
    tablaSistema.procesosEjecutando   = ejecutando;
    tablaSistema.procesosEnES         = totalES;
    tablaSistema.procesosEnColaListos = colaListos->tamanio;
    tablaSistema.procesosEnSolicitud  = solicitudes->tamanio;
    tablaSistema.cicloActual          = reloj;
    tablaSistema.sumaEspera           = sumEspera;
    tablaSistema.sumaCiclosRestantes  = sumCiclos;
    tablaSistema.promedioEspera       = colaListos->tamanio ?
                                        sumEspera / colaListos->tamanio : 0;
    tablaSistema.promedioCiclos       = TOTAL_PROCESOS ?
                                        sumCiclos / TOTAL_PROCESOS : 0;
    tablaSistema.memoriaLibreKB       = memoriaBuddy.memoriaLibreKB;
    tablaSistema.desperdicioTotal     = memoriaBuddy.desperdicioInternoTotal;
}

// ---------------------------------------------------
// LISTA DOBLEMENTE ENLAZADA
// ---------------------------------------------------
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

// COLA FIFO

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
    free(tmp); c->tamanio--;
    return p;
}

int estaVaciaCola(Cola *c) { return c->frente == NULL; }

// Mueve un proceso al frente de la cola si esta dentro de ella
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

// ---------------------------------------------------
// SISTEMA E/S — 4 dispositivos
// ---------------------------------------------------
void inicializarSistemaES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

void asignarES(Proceso *p, SistemaES *es, int cicloActual)
{
    int mults[4] = {2, 4, 8, 12};
    int tipo  = rand() % 4;
    int base  = rand() % 100 + 1;

    p->tiempoES          = base * mults[tipo];
    p->dispositivoES     = tipo;
    p->estado            = ESTADO_ESPERA_ES;  // primero cambiar estado
    p->ciclosEnEjecucion = 0;

    procesarFraseES(p, cicloActual);  // luego procesar frase con estado correcto

    Cola *dest = NULL;
    switch (tipo) {
        case 0: dest = &es->disco;     break;
        case 1: dest = &es->pantalla;  break;
        case 2: dest = &es->teclado;   break;
        case 3: dest = &es->impresora; break;
    }
    if (!dest) return;

    if (p->esApropiativo) encolarAlFrente(dest, p);
    else                  encolar(dest, p);
}

// Ingresa a la cola de listos los procesos de solicitudes que ya llegaron
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

// Incrementa el tiempo de espera de cada proceso en la cola de listos
void actualizarEspera(Cola *colaListos)
{
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
        if (n->proceso->estado == ESTADO_LISTO)
            n->proceso->tiempoEspera++;
}

// BUDDY SYSTEM — 8 MB total

void inicializarBuddy(void)
{
    memset(&memoriaBuddy, 0, sizeof(MemoriaBuddy));
    pthread_mutex_init(&memoriaBuddy.mutex, NULL);
    memoriaBuddy.bloques[0].tamanioKB    = BUDDY_MEMORIA_TOTAL_KB;
    memoriaBuddy.bloques[0].baseDir      = 0;
    memoriaBuddy.bloques[0].libre        = 1;
    memoriaBuddy.bloques[0].indexProceso = -1;
    memoriaBuddy.bloques[0].socioIdx     = -1;
    memoriaBuddy.numBloques              = 1;
    memoriaBuddy.memoriaLibreKB          = BUDDY_MEMORIA_TOTAL_KB;
}

// Divide un bloque en mitades hasta llegar al tamano objetivo
static int dividirHasta(int idx, int targetKB)
{
    while (memoriaBuddy.bloques[idx].tamanioKB > targetKB) {
        if (memoriaBuddy.numBloques >= BUDDY_MAX_BLOQUES - 1) break;
        int mitad = memoriaBuddy.bloques[idx].tamanioKB / 2;
        if (mitad < BUDDY_MIN_KB) break;
        int nuevo = memoriaBuddy.numBloques;
        memoriaBuddy.bloques[nuevo].tamanioKB    = mitad;
        memoriaBuddy.bloques[nuevo].baseDir      = memoriaBuddy.bloques[idx].baseDir + mitad;
        memoriaBuddy.bloques[nuevo].libre        = 1;
        memoriaBuddy.bloques[nuevo].indexProceso = -1;
        memoriaBuddy.bloques[nuevo].socioIdx     = idx;
        memoriaBuddy.numBloques++;
        memoriaBuddy.bloques[idx].tamanioKB = mitad;
        memoriaBuddy.bloques[idx].socioIdx  = nuevo;
    }
    return idx;
}

// Asigna un bloque Buddy al proceso, redondeando a la siguiente potencia de 2
int asignarMemoriaBuddy(Proceso *p, int memoriaKB)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int target = potencia2(memoriaKB);

    // Buscar el bloque libre mas pequeno que sea suficientemente grande
    int mejor = -1;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        BloqueBS *b = &memoriaBuddy.bloques[i];
        if (b->libre && b->tamanioKB >= target) {
            if (mejor == -1 || b->tamanioKB < memoriaBuddy.bloques[mejor].tamanioKB)
                mejor = i;
        }
    }

    if (mejor == -1) { pthread_mutex_unlock(&memoriaBuddy.mutex); return -1; }

    int idx = dividirHasta(mejor, target);
    int ip  = (int)(p - tablaSistema.tablaBCPs);
    memoriaBuddy.bloques[idx].libre        = 0;
    memoriaBuddy.bloques[idx].indexProceso = ip;
    memoriaBuddy.memoriaLibreKB           -= target;
    memoriaBuddy.memoriaUsadaKB           += target;
    memoriaBuddy.desperdicioInternoTotal  += target - memoriaKB;
    p->bloqueMemoriaKB    += target;
    p->desperdicioInterno += target - memoriaKB;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
    return idx;
}

// Libera un bloque Buddy y fusiona con su socio si ambos estan libres
static void liberarUnBloque(int idx, int ip)
{
    if (idx < 0 || idx >= memoriaBuddy.numBloques) return;
    BloqueBS *b = &memoriaBuddy.bloques[idx];
    if (b->indexProceso != ip || b->libre) return;

    int tam = b->tamanioKB;
    b->libre        = 1;
    b->indexProceso = -1;
    memoriaBuddy.memoriaLibreKB += tam;
    memoriaBuddy.memoriaUsadaKB -= tam;

    // Intentar fusionar con el bloque socio
    int actual = idx;
    while (1) {
        int socio = memoriaBuddy.bloques[actual].socioIdx;
        if (socio < 0 || socio >= memoriaBuddy.numBloques) break;
        BloqueBS *bA = &memoriaBuddy.bloques[actual];
        BloqueBS *bS = &memoriaBuddy.bloques[socio];
        if (!bS->libre || bS->tamanioKB != bA->tamanioKB) break;
        if (bA->baseDir < bS->baseDir) {
            bA->tamanioKB *= 2;
            bA->socioIdx   = bS->socioIdx;
            bS->tamanioKB  = 0; bS->libre = 0;
        } else {
            bS->tamanioKB *= 2;
            bS->socioIdx   = bA->socioIdx;
            bA->tamanioKB  = 0; bA->libre = 0;
            actual = socio;
        }
    }
}

// Libera todos los bloques Buddy que tiene asignados un proceso
void liberarTodosLosBloquesBuddy(Proceso *p)
{
    if (p->numBloquesBuddy == 0) return;
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int ip = (int)(p - tablaSistema.tablaBCPs);
    for (int i = 0; i < p->numBloquesBuddy; i++) {
        liberarUnBloque(p->idxsBuddy[i], ip);
        p->idxsBuddy[i] = -1;
    }
    memoriaBuddy.desperdicioInternoTotal -= p->desperdicioInterno;
    if (memoriaBuddy.desperdicioInternoTotal < 0)
        memoriaBuddy.desperdicioInternoTotal = 0;
    p->numBloquesBuddy    = 0;
    p->bloqueMemoriaKB    = 0;
    p->desperdicioInterno = 0;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
}

// PAGINACION NRU + SWAP

void inicializarPaginacion(void)
{
    memset(&memoriaPrincipal, 0, sizeof(MemoriaPrincipal));
    memset(&areaSwap,         0, sizeof(AreaSwap));
    memoriaPrincipal.numMarcosTotal = MARCOS_MAX * TOTAL_PROCESOS;
    for (int i = 0; i < memoriaPrincipal.numMarcosTotal; i++) {
        memoriaPrincipal.marcos[i].indiceProceso = -1;
        memoriaPrincipal.marcos[i].indicePagina  = -1;
    }
}

// Llena una pagina con palabras del banco (cursor circular)
static void cargarPalabrasPagina(Pagina *pag)
{
    pag->numPalabras = 0;
    for (int w = 0; w < PALABRAS_POR_PAGINA && bancoPalabras.totalPalabras > 0; w++) {
        strncpy(pag->palabras[w],
                bancoPalabras.palabras[bancoPalabras.cursor],
                MAX_LEN_PALABRA - 1);
        pag->numPalabras++;
        bancoPalabras.cursor = (bancoPalabras.cursor + 1) % bancoPalabras.totalPalabras;
    }
}

// Asigna paginas al proceso: las primeras van a RAM y el resto a SWAP
void asignarPaginasProceso(Proceso *p)
{
    int totalPals = p->memoriaUsadaKB * 10;
    p->numPaginas = (totalPals + PALABRAS_POR_PAGINA - 1) / PALABRAS_POR_PAGINA;
    if (p->numPaginas > MAX_PAGINAS_PROCESO) p->numPaginas = MAX_PAGINAS_PROCESO;
    if (p->numPaginas < 1) p->numPaginas = 1;

    int ip = (int)(p - tablaSistema.tablaBCPs);
    int asignados = 0;

    for (int pg = 0; pg < p->numPaginas; pg++) {
        p->paginasEnRAM[pg]  = -1;
        p->bitReferencia[pg] = 0;
        p->bitModificado[pg] = 0;
    }

    // Asignar hasta numMarcos paginas en RAM
    for (int pg = 0; pg < p->numPaginas && asignados < p->numMarcos; pg++) {
        int marco = -1;
        for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++)
            if (memoriaPrincipal.marcos[m].indiceProceso == -1) { marco = m; break; }
        if (marco == -1) break;

        Pagina *pag = &memoriaPrincipal.marcos[marco];
        pag->indiceProceso = ip;
        pag->indicePagina  = pg;
        pag->bitR = 0; pag->bitM = 0;
        pag->tiempoEntrada = tablaSistema.cicloActual;
        cargarPalabrasPagina(pag);

        p->paginasEnRAM[pg] = marco;
        memoriaPrincipal.numMarcosOcupados++;
        asignados++;
    }

    // El resto de paginas va a SWAP
    for (int pg = asignados; pg < p->numPaginas; pg++) {
        if (areaSwap.numPaginas >= MAX_PAGINAS_SWAP) break;
        Pagina *sp = &areaSwap.paginas[areaSwap.numPaginas];
        sp->indiceProceso = ip;
        sp->indicePagina  = pg;
        sp->bitR = 0; sp->bitM = 0;
        sp->tiempoEntrada = 0;
        cargarPalabrasPagina(sp);
        areaSwap.numPaginas++;
    }
}

// Libera todas las paginas de un proceso de RAM y de SWAP
void liberarPaginasProceso(Proceso *p)
{
    int ip = (int)(p - tablaSistema.tablaBCPs);

    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == ip) {
            memoriaPrincipal.marcos[m].indiceProceso = -1;
            memoriaPrincipal.marcos[m].indicePagina  = -1;
            memoriaPrincipal.marcos[m].numPalabras   = 0;
            memoriaPrincipal.marcos[m].bitR = 0;
            memoriaPrincipal.marcos[m].bitM = 0;
            memoriaPrincipal.numMarcosOcupados--;
        }
    }

    // Compactar SWAP quitando las paginas de este proceso
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

// NRU: elige la victima de menor clase (R=0, M=0 es clase 0, la mejor)
static int seleccionarVictimaNRU(void)
{
    int victima = -1, claseV = 4;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++) {
        if (memoriaPrincipal.marcos[m].indiceProceso == -1) continue;
        int clase = memoriaPrincipal.marcos[m].bitR * 2 +
                    memoriaPrincipal.marcos[m].bitM;
        if (clase < claseV ||
           (clase == claseV && victima >= 0 &&
            memoriaPrincipal.marcos[m].tiempoEntrada <
            memoriaPrincipal.marcos[victima].tiempoEntrada)) {
            claseV  = clase;
            victima = m;
        }
    }
    return victima;
}

// Maneja un fallo de pagina: trae la pagina de SWAP a RAM usando NRU si no hay marcos libres
int manejarFalloPagina(Proceso *p, int indicePagina, int cicloActual)
{
    // Buscar un marco libre primero
    int marco = -1;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++)
        if (memoriaPrincipal.marcos[m].indiceProceso == -1) { marco = m; break; }

    int usaVictima = 0;
    if (marco == -1) {
        // No hay marcos libres: aplicar reemplazo NRU
        marco = seleccionarVictimaNRU();
        if (marco == -1) return -1;
        usaVictima = 1;

        // Mandar la victima a SWAP
        Pagina *vic = &memoriaPrincipal.marcos[marco];
        if (areaSwap.numPaginas < MAX_PAGINAS_SWAP) {
            areaSwap.paginas[areaSwap.numPaginas] = *vic;
            areaSwap.numPaginas++;
        }
        int ipV = vic->indiceProceso, pgV = vic->indicePagina;
        if (ipV >= 0 && ipV < TOTAL_PROCESOS && pgV >= 0 && pgV < MAX_PAGINAS_PROCESO) {
            tablaSistema.tablaBCPs[ipV].paginasEnRAM[pgV]  = -1;
            tablaSistema.tablaBCPs[ipV].bitReferencia[pgV] = 0;
            tablaSistema.tablaBCPs[ipV].bitModificado[pgV] = 0;
        }
    }

    // Buscar la pagina pedida en SWAP
    int ip = (int)(p - tablaSistema.tablaBCPs);
    int swapIdx = -1;
    for (int s = 0; s < areaSwap.numPaginas; s++) {
        if (areaSwap.paginas[s].indiceProceso == ip &&
            areaSwap.paginas[s].indicePagina  == indicePagina) {
            swapIdx = s; break;
        }
    }

    // Traer la pagina de SWAP al marco, o cargar palabras nuevas si no estaba en SWAP
    Pagina *dest = &memoriaPrincipal.marcos[marco];
    if (swapIdx >= 0) {
        *dest = areaSwap.paginas[swapIdx];
        // Compactar el area de SWAP
        for (int s = swapIdx; s < areaSwap.numPaginas - 1; s++)
            areaSwap.paginas[s] = areaSwap.paginas[s + 1];
        areaSwap.numPaginas--;
    } else {
        dest->indiceProceso = ip;
        dest->indicePagina  = indicePagina;
        cargarPalabrasPagina(dest);
    }

    dest->bitR = 1; dest->bitM = 0;
    dest->tiempoEntrada = cicloActual;

    if (!usaVictima) memoriaPrincipal.numMarcosOcupados++;
    p->paginasEnRAM[indicePagina]  = marco;
    p->bitReferencia[indicePagina] = 1;
    p->bitModificado[indicePagina] = 0;
    return marco;
}

// Cada 50 ciclos resetea los bits R de todos los marcos (parte del ciclo NRU)
void resetarBitsR(int cicloActual)
{
    if (cicloActual % 50 != 0) return;
    for (int m = 0; m < memoriaPrincipal.numMarcosTotal; m++)
        memoriaPrincipal.marcos[m].bitR = 0;
    for (int i = 0; i < TOTAL_PROCESOS; i++)
        for (int pg = 0; pg < tablaSistema.tablaBCPs[i].numPaginas; pg++)
            tablaSistema.tablaBCPs[i].bitReferencia[pg] = 0;
}

// Cada 300 ciclos: mitad de procesos reducen paginas a la mitad, la otra mitad las duplica
void redimensionarMemoriaPrincipal(Lista *enEjecucion, int cicloActual)
{
    int total = enEjecucion->tamanio;
    int cont  = 0;

    for (Nodo *n = enEjecucion->cabeza; n; n = n->siguiente, cont++) {
        Proceso *p = n->proceso;
        if (p->estado == ESTADO_TERMINADO) continue;

        if (cont < total / 2) {
            // Primera mitad: reducir marcos a la mitad
            int nuevo = p->numMarcos / 2;
            if (nuevo < MARCOS_MIN) nuevo = MARCOS_MIN;
            int liberados = 0, objetivo = p->numMarcos - nuevo;
            for (int pg = p->numPaginas - 1; pg >= 0 && liberados < objetivo; pg--) {
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
                p->paginasEnRAM[pg]  = -1;
                p->bitReferencia[pg] = 0;
                liberados++;
            }
            p->numMarcos = nuevo;
        } else {
            // Segunda mitad: duplicar marcos
            int nuevo = p->numMarcos * 2;
            if (nuevo > MARCOS_MAX) nuevo = MARCOS_MAX;
            int extra = nuevo - p->numMarcos;
            for (int pg = 0; pg < p->numPaginas && extra > 0; pg++) {
                if (p->paginasEnRAM[pg] >= 0) continue;
                if (manejarFalloPagina(p, pg, cicloActual) >= 0)
                    extra--;
            }
            p->numMarcos = nuevo;
        }
    }
    printf("[MEM] Redimension en ciclo %d: mitad reduce, mitad amplia paginas\n", cicloActual);
}

// Cada vez que el proceso entra a CPU puede pedir mas memoria Buddy
void crecerMemoriaProceso(Proceso *p)
{
    if (p->numBloquesBuddy >= MAX_BLOQUES_BUDDY_PROC) return;
    int extra = p->crecimientoMem[p->indiceCrecimiento % 20];
    p->indiceCrecimiento++;
    if (extra <= 0) return;
    int idx = asignarMemoriaBuddy(p, extra);
    if (idx < 0) return;
    p->idxsBuddy[p->numBloquesBuddy] = idx;
    p->numBloquesBuddy++;
}

// CPU

// Prepara al proceso para ejecutar: asigna rafaga, descuenta ciclos, marca paginas
void procesarEntradaCPU(Proceso *p, int reloj)
{
    p->cambiosContexto = rand() % 21 + 10;
    crecerMemoriaProceso(p);

    p->estado = ESTADO_EJECUTANDO;
    p->vecesEnCPU++;
    p->iteraciones++;

    // El tiempo de respuesta se calcula solo la primera vez que entra a CPU
    if (p->vecesEnCPU == 1)
        p->tiempoRespuesta = reloj - p->tiempoLlegada;

    // Rafaga: cuantos ciclos quiere ejecutar en esta instancia (10-70)
    // Si quedan menos ciclos que la rafaga, se ajusta al restante
    int rafaga = rand() % 61 + 10;
    if (rafaga > p->ciclosRestantes) rafaga = p->ciclosRestantes;
    if (rafaga < 1) rafaga = 1;

    p->rafagaActual = rafaga;

    // Descontar la rafaga completa; el controlador ajusta si hay preempcion en RR
    int ejecutados = rafaga;
    p->ciclosRestantes   -= ejecutados;
    p->tiempoEjecucion   += ejecutados;
    p->ciclosEnEjecucion += ejecutados;
    p->restanteQuantum    = tablaSistema.quantumActual;

    // Marcar como referenciadas todas las paginas del proceso que estan en RAM
    for (int pg = 0; pg < p->numPaginas; pg++) {
        if (p->paginasEnRAM[pg] >= 0) {
            memoriaPrincipal.marcos[p->paginasEnRAM[pg]].bitR = 1;
            p->bitReferencia[pg] = 1;
        }
    }
}

// Marca el proceso como terminado y libera su memoria
void procesarTerminacion(Proceso *p, int reloj)
{
    p->estado        = ESTADO_TERMINADO;
    p->tiempoRetorno = reloj - p->tiempoLlegada;
    liberarTodosLosBloquesBuddy(p);
    liberarPaginasProceso(p);
    estadMem.procesosTerminados++;
    estadMem.tiempoTotalEjecucion += p->tiempoEjecucion;
    printf("  [FIN] %s | ejec:%d | espera:%d | retorno:%d | fallos:%d\n",
           p->id, p->tiempoEjecucion, p->tiempoEspera,
           p->tiempoRetorno, p->fallosPagina);
}

// ESTADISTICAS DE MEMORIA

// Calcula el desperdicio externo: memoria libre fragmentada que no forma un bloque continuo
void calcularDesperdicioExterno(void)
{
    pthread_mutex_lock(&memoriaBuddy.mutex);
    int maxLibre = 0, totalLibre = 0;
    for (int i = 0; i < memoriaBuddy.numBloques; i++) {
        BloqueBS *b = &memoriaBuddy.bloques[i];
        if (b->libre && b->tamanioKB > 0) {
            totalLibre += b->tamanioKB;
            if (b->tamanioKB > maxLibre) maxLibre = b->tamanioKB;
        }
    }
    estadMem.desperdicioExterno = (totalLibre > maxLibre) ? totalLibre - maxLibre : 0;
    pthread_mutex_unlock(&memoriaBuddy.mutex);
}

void actualizarPromedioFinalizados(int cicloActual)
{
    if (cicloActual > 0 && estadMem.procesosTerminados > 0)
        estadMem.promedioFinalizadosPorCiclo =
            (float)estadMem.procesosTerminados / (float)cicloActual;
    else
        estadMem.promedioFinalizadosPorCiclo = 0.0f;
}

void mostrarEstadisticasMemoria(void)
{
    int term = estadMem.procesosTerminados;
    printf("\n--- ESTADISTICAS DE MEMORIA ---\n");
    printf("  Buddy total          : %d KB\n",   BUDDY_MEMORIA_TOTAL_KB);
    printf("  Buddy libre          : %d KB\n",   memoriaBuddy.memoriaLibreKB);
    printf("  Buddy usado          : %d KB\n",   memoriaBuddy.memoriaUsadaKB);
    printf("  Desperdicio interno  : %d KB\n",   memoriaBuddy.desperdicioInternoTotal);
    printf("  Desperdicio externo  : %d KB\n",   estadMem.desperdicioExterno);
    printf("  Marcos RAM ocupados  : %d\n",      memoriaPrincipal.numMarcosOcupados);
    printf("  Paginas en SWAP      : %d\n",      areaSwap.numPaginas);
    printf("  Fallos de pagina NRU : %d\n",      tablaSistema.totalFallosPagina);
    printf("  Procesos terminados  : %d\n",      term);
    printf("  Prom terminados/ciclo: %.4f\n",    estadMem.promedioFinalizadosPorCiclo);
    printf("  Prom ejec/proceso    : %d ciclos\n",
           term ? estadMem.tiempoTotalEjecucion / term : 0);
    printf("-------------------------------\n");
}

// CAMBIO AUTOMATICO DE ALGORITMO
// Usa 3 variables de tabla y 5 del BCP para decidir

int evaluarCambioAlgoritmo(Cola *colaListos, SistemaES *es)
{
    // Variables de tabla: procesosEnColaListos, procesosEnES, promedioEspera
    int enListo = tablaSistema.procesosEnColaListos;
    int enES    = tablaSistema.procesosEnES;
    int total   = enListo + enES + 1;
    float propL = (float)enListo / total;

    // Variables de BCP: tiempoEspera, vecesEnCPU, tipoProceso, ciclosRestantes, rafagaActual
    float promEspera = tablaSistema.promedioEspera;
    int ioCount = 0, cantProc = 0;
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente) {
        if (n->proceso->tipoProceso == 1) ioCount++;
        cantProc++;
    }
    float propIO = cantProc ? (float)ioCount / cantProc : 0.0f;

    int alg = tablaSistema.algoritmoActual;

    if (alg == ALG_FCFS) {
        // Cambiar a RR si hay mucha cola, mucha espera y hay procesos en E/S
        if (propL > 0.65f && promEspera > 400.0f && enES >= 3) {
            printf("[AUTO] FCFS -> RR (cola=%.0f%% espera=%.0f propIO=%.0f%%)\n",
                   propL * 100, promEspera, propIO * 100);
            return ALG_RR;
        }
    } else {
        // Volver a FCFS si el sistema esta estable
        if (propL < 0.20f && propIO < 0.20f && promEspera < 80.0f) {
            printf("[AUTO] RR -> FCFS (sistema estable)\n");
            return ALG_FCFS;
        }
    }

    (void)es;
    return tablaSistema.algoritmoActual;
}