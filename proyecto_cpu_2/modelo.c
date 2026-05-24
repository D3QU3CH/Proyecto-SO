#include "modelo.h"

// ─────────────────────────────────────────────────────────────────────────────
// DATOS GLOBALES
// ─────────────────────────────────────────────────────────────────────────────

// Frutas (recursos compartidos de la seccion critica)
char memoria[TAM_MEM][20] = {
    "Manzana", "Pera", "Mango", "Sandia", "Pina",
    "Uva", "Fresa", "Banano", "Naranja", "Limon",
    "Papaya", "Melon", "Guanabana", "Mammon", "Cas",
    "Nance", "Maranon", "Jobo", "Guayaba", "Carambola"};

int recursoOcupado[TAM_MEM];

// Tabla global de BCPs
Proceso tablaProcesos[TOTAL_PROCESOS];

// Registro de tiempos unicos de llegada
static int tiemposUsados[801];

// Siguiente proceso pendiente de ingresar al ciclo dinamicamente
static int indiceSiguiente = EN_SISTEMA;

// ─────────────────────────────────────────────────────────────────────────────
// BCP – inicializarProceso
// ─────────────────────────────────────────────────────────────────────────────

void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));

    snprintf(p->id, sizeof(p->id), "%c-%d", 'A' + (index % 26), index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);

    p->tiempoLlegada = 0;
    p->ciclosTotales = rand() % 85001 + 5000;
    p->ciclosRestantes = p->ciclosTotales;
    p->rafagaActual = 0;
    p->tiempoEjecucion = 0;
    p->tiempoEspera = 0;
    p->tiempoRespuesta = 0;
    p->tiempoRetorno = 0;

    p->estado = 0;

    p->vecesEnCPU = 0;
    p->iteraciones = 0;
    p->restanteQuantum = 0;
    p->cambiosContexto = 0;
    p->esApropiativo = 0;
    p->tipoProceso = 0;

    p->aprovechamiento = 0;
    p->desperdicio = 0;

    p->dispositivoES = -1;
    p->tiempoES = 0;
    p->bloqueado = 0;

    p->variable1 = -1;
    p->variable2 = -1;
    p->enSeccionCritica = 0;

    p->usoMemoria = rand() % 100;

    // Socios
    p->socioIndex = -1;
    p->socioTerminado = 0;
    p->reporteSocioGenerado = 0;

    // NRU
    inicializarNRU(p);
    p->fallosPagina = 0;
    p->reemplazosNRU = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// COLA
// ─────────────────────────────────────────────────────────────────────────────

void inicializarCola(Cola *c)
{
    c->frente = NULL;
    c->final = NULL;
    c->tamanio = 0;
}

void encolar(Cola *c, Proceso *p)
{
    Nodo *nuevo = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso = p;
    nuevo->siguiente = NULL;

    if (c->final == NULL)
    {
        c->frente = c->final = nuevo;
    }
    else
    {
        c->final->siguiente = nuevo;
        c->final = nuevo;
    }

    c->tamanio++;
}

Proceso *desencolar(Cola *c)
{
    if (estaVacia(c))
        return NULL;

    Nodo *temp = c->frente;
    Proceso *p = temp->proceso;

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

// Insercion ordenada por tiempoLlegada ascendente (para FCFS)

void insertarOrdenado(Cola *c, Proceso *p)
{
    Nodo *nuevo = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso = p;
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
    {
        actual = actual->siguiente;
    }

    nuevo->siguiente = actual->siguiente;
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

// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA E/S
// ─────────────────────────────────────────────────────────────────────────────

void inicializarES(SistemaES *es)
{
    inicializarCola(&es->disco);
    inicializarCola(&es->pantalla);
    inicializarCola(&es->teclado);
    inicializarCola(&es->impresora);
}

int contarES(SistemaES *es)
{
    return es->disco.tamanio +
           es->pantalla.tamanio +
           es->teclado.tamanio +
           es->impresora.tamanio;
}

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA (frutas / recursos compartidos)
// ─────────────────────────────────────────────────────────────────────────────

void inicializarMemoria(void)
{
    for (int i = 0; i < TAM_MEM; i++)
        recursoOcupado[i] = 0;
}

int usarRecurso(int index)
{
    if (index < 0 || index >= TAM_MEM)
        return 0;
    if (recursoOcupado[index] == 1)
        return 0;

    recursoOcupado[index] = 1;
    return 1;
}

void liberarRecurso(int index)
{
    if (index >= 0 && index < TAM_MEM)
        recursoOcupado[index] = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// TABLA GLOBAL DE PROCESOS
// ─────────────────────────────────────────────────────────────────────────────

static int generarTiempoUnico(void)
{
    int t;
    do
    {
        t = rand() % 800;
    } while (tiemposUsados[t] == 1);
    tiemposUsados[t] = 1;
    return t;
}

void inicializarSistema(void)
{
    memset(tiemposUsados, 0, sizeof(tiemposUsados));

    for (int i = 0; i < TOTAL_PROCESOS; i++)
    {
        inicializarProceso(&tablaProcesos[i], i);
        tablaProcesos[i].tiempoLlegada = generarTiempoUnico();
    }

    // Asignar socios despues de crear todos los BCPs
    asignarSocios();
}

// Ordena la tabla por tiempoLlegada ascendente (burbuja)

static void ordenarPorLlegada(void)
{
    for (int i = 0; i < TOTAL_PROCESOS - 1; i++)
        for (int j = i + 1; j < TOTAL_PROCESOS; j++)
            if (tablaProcesos[i].tiempoLlegada > tablaProcesos[j].tiempoLlegada)
            {
                Proceso tmp = tablaProcesos[i];
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

// Agrega procesos cuyo tiempoLlegada <= reloj actual

void ingresarProcesosNuevos(Cola *procesosEnCiclo, int reloj)
{
    while (indiceSiguiente < TOTAL_PROCESOS)
    {
        Proceso *p = &tablaProcesos[indiceSiguiente];
        if (p->tiempoLlegada <= reloj)
        {
            insertarOrdenado(procesosEnCiclo, p);
            indiceSiguiente++;
        }
        else
        {
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SOCIOS
// ─────────────────────────────────────────────────────────────────────────────

// Asigna socios al azar en pares; un proceso sin pareja queda con socioIndex=-1

void asignarSocios(void)
{
    // Shuffle de indices
    int indices[TOTAL_PROCESOS];
    for (int i = 0; i < TOTAL_PROCESOS; i++)
        indices[i] = i;

    for (int i = TOTAL_PROCESOS - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    // Emparejar de 2 en 2
    for (int i = 0; i + 1 < TOTAL_PROCESOS; i += 2)
    {
        int a = indices[i];
        int b = indices[i + 1];
        tablaProcesos[a].socioIndex = b;
        tablaProcesos[b].socioIndex = a;
    }

    printf("  [SOCIOS] Socios asignados (%d pares)\n", TOTAL_PROCESOS / 2);
}

// Notifica que p termino; si su socio tambien termino genera el reporte conjunto

void notificarTerminacion(Proceso *p, pthread_mutex_t *mtx)
{
    if (p->socioIndex < 0 || p->socioIndex >= TOTAL_PROCESOS)
        return;

    Proceso *socio = &tablaProcesos[p->socioIndex];

    pthread_mutex_lock(mtx);

    if (socio->estado == 3 && !p->reporteSocioGenerado)
    {
        // Ambos terminaron -> reporte conjunto
        p->reporteSocioGenerado = 1;
        socio->reporteSocioGenerado = 1;

        FILE *f = fopen("socios.log", "a");
        if (f)
        {
            fprintf(f, "\n=== PAR DE SOCIOS COMPLETADO ===\n");
            fprintf(f, "  Proceso A : %-8s | Retorno: %d | VecesEnCPU: %d\n",
                    p->id, p->tiempoRetorno, p->vecesEnCPU);
            fprintf(f, "  Proceso B : %-8s | Retorno: %d | VecesEnCPU: %d\n",
                    socio->id, socio->tiempoRetorno, socio->vecesEnCPU);
            fprintf(f, "  Retorno combinado: %d\n",
                    p->tiempoRetorno + socio->tiempoRetorno);
            fclose(f);
        }

        printf("  [SOCIOS] Par %s <-> %s completado. Retorno combinado: %d\n",
               p->id, socio->id,
               p->tiempoRetorno + socio->tiempoRetorno);
    }
    else
    {
        // Solo este termino; marcar al socio para que sepa
        socio->socioTerminado = 1;
        printf("  [SOCIOS] %s termino. Esperando socio %s...\n",
               p->id, socio->id);
    }

    pthread_mutex_unlock(mtx);
}

// ─────────────────────────────────────────────────────────────────────────────
// NRU (Not Recently Used)
// ─────────────────────────────────────────────────────────────────────────────

void inicializarNRU(Proceso *p)
{
    for (int i = 0; i < NRU_NUM_MARCOS; i++)
    {
        p->marcosNRU[i].numeroPagina = -1;
        p->marcosNRU[i].bitR = 0;
        p->marcosNRU[i].bitM = 0;
        p->marcosNRU[i].valido = 0;
    }
}

// Devuelve la clase NRU (0-3) de un marco segun bits R y M

static int claseNRU(MarcoNRU *m)
{
    return (m->bitR << 1) | m->bitM;
    // clase 0: R=0 M=0  clase 1: R=0 M=1
    // clase 2: R=1 M=0  clase 3: R=1 M=1
}

// Elige el marco a reemplazar: menor clase NRU; dentro de la clase, el primero

static int elegirVictimaНРU(Proceso *p)
{
    int mejor = -1;
    int mejorCl = 4;

    for (int i = 0; i < NRU_NUM_MARCOS; i++)
    {
        if (!p->marcosNRU[i].valido)
        {
            return i; // marco libre: usarlo directamente
        }
        int cl = claseNRU(&p->marcosNRU[i]);
        if (cl < mejorCl)
        {
            mejorCl = cl;
            mejor = i;
        }
    }
    return mejor;
}

// Simula acceso a una pagina virtual del proceso

void accederPaginaNRU(Proceso *p)
{
    int pagVirtual = rand() % NRU_NUM_PAGINAS;

    // Buscar si ya esta en algun marco
    for (int i = 0; i < NRU_NUM_MARCOS; i++)
    {
        if (p->marcosNRU[i].valido &&
            p->marcosNRU[i].numeroPagina == pagVirtual)
        {
            // Hit: marcar R y posiblemente M
            p->marcosNRU[i].bitR = 1;
            if (rand() % 3 == 0) // 33% de escritura
                p->marcosNRU[i].bitM = 1;
            return;
        }
    }

    // Fallo de pagina: elegir victima NRU
    p->fallosPagina++;
    int victima = elegirVictimaНРU(p);

    if (p->marcosNRU[victima].valido)
    {
        p->reemplazosNRU++;
        printf("  [NRU ] %s | Reemplaza pag %d (clase %d) por pag %d\n",
               p->id,
               p->marcosNRU[victima].numeroPagina,
               claseNRU(&p->marcosNRU[victima]),
               pagVirtual);
    }

    p->marcosNRU[victima].numeroPagina = pagVirtual;
    p->marcosNRU[victima].bitR = 1;
    p->marcosNRU[victima].bitM = (rand() % 3 == 0) ? 1 : 0;
    p->marcosNRU[victima].valido = 1;
}

// Limpia bits R de todos los marcos (se llama periodicamente por el reloj)

void limpiarBitsR(Proceso *p)
{
    for (int i = 0; i < NRU_NUM_MARCOS; i++)
        p->marcosNRU[i].bitR = 0;
}