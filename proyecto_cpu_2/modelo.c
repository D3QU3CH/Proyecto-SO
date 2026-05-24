#include "modelo.h"

/* =====================================================================
 * modelo.c  –  Implementacion de todas las estructuras de datos
 *
 * Fusion de: proceso.c + cola.c + es.c (datos) + memoria.c + sistema.c
 *            (solo la parte de datos/inicializacion)
 * ===================================================================== */

/* ================================================================
 * DATOS GLOBALES
 * ================================================================ */

/* Frutas (recursos compartidos de la seccion critica) */
char memoria[TAM_MEM][20] = {
    "Manzana",  "Pera",      "Mango",    "Sandia",   "Pina",
    "Uva",      "Fresa",     "Banano",   "Naranja",  "Limon",
    "Papaya",   "Melon",     "Guanabana","Mammon",   "Cas",
    "Nance",    "Maranon",   "Jobo",     "Guayaba",  "Carambola"
};
int recursoOcupado[TAM_MEM];

/* Tabla global de BCPs */
Proceso tablaProcesos[TOTAL_PROCESOS];

/* Registro de tiempos unicos de llegada */
static int tiemposUsados[801];

/* Siguiente proceso pendiente de ingresar al ciclo dinamicamente */
static int indiceSiguiente = EN_SISTEMA;

/* ================================================================
 * BCP – inicializarProceso
 * ================================================================ */
void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));

    snprintf(p->id,     sizeof(p->id),     "%c-%d", 'A' + (index % 26), index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);

    p->tiempoLlegada   = 0;
    p->ciclosTotales   = rand() % 85001 + 5000;
    p->ciclosRestantes = p->ciclosTotales;
    p->rafagaActual    = 0;
    p->tiempoEjecucion = 0;
    p->tiempoEspera    = 0;
    p->tiempoRespuesta = 0;
    p->tiempoRetorno   = 0;

    p->estado          = 0;   /* listo */

    p->vecesEnCPU      = 0;
    p->iteraciones     = 0;
    p->restanteQuantum = 0;
    p->cambiosContexto = 0;
    p->esApropiativo   = 0;
    p->tipoProceso     = 0;

    p->aprovechamiento = 0;
    p->desperdicio     = 0;

    p->dispositivoES   = -1;
    p->tiempoES        = 0;
    p->bloqueado       = 0;

    p->variable1        = -1;
    p->variable2        = -1;
    p->enSeccionCritica = 0;

    p->usoMemoria = rand() % 100;
}

/* ================================================================
 * COLA
 * ================================================================ */
void inicializarCola(Cola *c)
{
    c->frente  = NULL;
    c->final   = NULL;
    c->tamanio = 0;
}

void encolar(Cola *c, Proceso *p)
{
    Nodo *nuevo     = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso  = p;
    nuevo->siguiente = NULL;

    if (c->final == NULL) {
        c->frente = c->final = nuevo;
    } else {
        c->final->siguiente = nuevo;
        c->final = nuevo;
    }
    c->tamanio++;
}

Proceso *desencolar(Cola *c)
{
    if (estaVacia(c)) return NULL;

    Nodo    *temp = c->frente;
    Proceso *p    = temp->proceso;

    c->frente = c->frente->siguiente;
    if (c->frente == NULL)
        c->final = NULL;

    free(temp);
    c->tamanio--;
    return p;
}

int estaVacia(Cola *c)
{
    return c->frente == NULL;
}

/* Insercion ordenada por tiempoLlegada ascendente (para FCFS) */
void insertarOrdenado(Cola *c, Proceso *p)
{
    Nodo *nuevo     = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso  = p;
    nuevo->siguiente = NULL;

    if (c->frente == NULL ||
        p->tiempoLlegada < c->frente->proceso->tiempoLlegada)
    {
        nuevo->siguiente = c->frente;
        c->frente = nuevo;
        if (c->final == NULL)
            c->final = nuevo;
        c->tamanio++;
        return;
    }

    Nodo *actual = c->frente;
    while (actual->siguiente != NULL &&
           actual->siguiente->proceso->tiempoLlegada <= p->tiempoLlegada)
        actual = actual->siguiente;

    nuevo->siguiente  = actual->siguiente;
    actual->siguiente = nuevo;
    if (nuevo->siguiente == NULL)
        c->final = nuevo;

    c->tamanio++;
}

void liberarCola(Cola *c)
{
    while (!estaVacia(c))
        desencolar(c);
}

/* ================================================================
 * SISTEMA E/S
 * ================================================================ */
void inicializarES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

int contarES(SistemaES *es)
{
    return es->disco.tamanio    +
           es->pantalla.tamanio +
           es->teclado.tamanio  +
           es->impresora.tamanio;
}

/* ================================================================
 * MEMORIA (frutas / recursos compartidos)
 * ================================================================ */
void inicializarMemoria(void)
{
    for (int i = 0; i < TAM_MEM; i++)
        recursoOcupado[i] = 0;
}

int usarRecurso(int index)
{
    if (index < 0 || index >= TAM_MEM) return 0;
    if (recursoOcupado[index] == 1)    return 0;
    recursoOcupado[index] = 1;
    return 1;
}

void liberarRecurso(int index)
{
    if (index >= 0 && index < TAM_MEM)
        recursoOcupado[index] = 0;
}

/* ================================================================
 * TABLA GLOBAL DE PROCESOS
 * ================================================================ */
static int generarTiempoUnico(void)
{
    int t;
    do { t = rand() % 800; } while (tiemposUsados[t] == 1);
    tiemposUsados[t] = 1;
    return t;
}

void inicializarSistema(void)
{
    memset(tiemposUsados, 0, sizeof(tiemposUsados));
    for (int i = 0; i < TOTAL_PROCESOS; i++) {
        inicializarProceso(&tablaProcesos[i], i);
        tablaProcesos[i].tiempoLlegada = generarTiempoUnico();
    }
}

/* Ordena la tabla por tiempoLlegada ascendente (burbuja) */
static void ordenarPorLlegada(void)
{
    for (int i = 0; i < TOTAL_PROCESOS - 1; i++)
        for (int j = i + 1; j < TOTAL_PROCESOS; j++)
            if (tablaProcesos[i].tiempoLlegada > tablaProcesos[j].tiempoLlegada) {
                Proceso tmp      = tablaProcesos[i];
                tablaProcesos[i] = tablaProcesos[j];
                tablaProcesos[j] = tmp;
            }
}

void cargarProcesosEnCola(Cola *procesosEnCiclo, Cola *nuevasSolicitudes)
{
    ordenarPorLlegada();
    for (int i = 0; i < EN_SISTEMA; i++)
        encolar(procesosEnCiclo, &tablaProcesos[i]);
    for (int i = EN_SISTEMA; i < TOTAL_PROCESOS; i++)
        encolar(nuevasSolicitudes, &tablaProcesos[i]);
}

/* Agrega procesos cuyo tiempoLlegada <= reloj actual */
void ingresarProcesosNuevos(Cola *procesosEnCiclo, int reloj)
{
    while (indiceSiguiente < TOTAL_PROCESOS) {
        Proceso *p = &tablaProcesos[indiceSiguiente];
        if (p->tiempoLlegada <= reloj) {
            insertarOrdenado(procesosEnCiclo, p);
            indiceSiguiente++;
        } else {
            break;
        }
    }
}