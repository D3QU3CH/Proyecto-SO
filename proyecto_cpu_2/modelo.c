#include "modelo.h"

// ─────────────────────────────────────────────────────────────────────────────
// DATOS GLOBALES
// ─────────────────────────────────────────────────────────────────────────────

// Frutas: recursos compartidos usados en la seccion critica (RR)
char memoria[TAM_MEM][20] = {
    "Manzana",   "Pera",      "Mango",     "Sandia",    "Pina",
    "Uva",       "Fresa",     "Banano",    "Naranja",   "Limon",
    "Papaya",    "Melon",     "Guanabana", "Mammon",    "Cas",
    "Nance",     "Maranon",   "Jobo",      "Guayaba",   "Carambola"
};

int recursoOcupado[TAM_MEM];

// Tabla global: fuente de verdad de todos los BCPs.
// NINGUN proceso sale de aqui durante la simulacion.
Proceso tablaProcesos[TOTAL_PROCESOS];

// ─────────────────────────────────────────────────────────────────────────────
// LISTAS FIJAS
//
// listaProcesosEnEjecucion : 150 punteros a BCPs que arrancan en el ciclo.
//                            El proceso NUNCA se elimina de aqui.
//                            Su campo `estado` cambia segun el ciclo.
//
// listaNuevasSolicitudes   : 100 punteros a BCPs que esperan su tiempoLlegada.
//                            Cuando el reloj >= tiempoLlegada se encolan en
//                            colaListos y se marcan como ingresados.
//                            Tampoco se eliminan de esta lista.
// ─────────────────────────────────────────────────────────────────────────────

Proceso *listaProcesosEnEjecucion[EN_SISTEMA];
Proceso *listaNuevasSolicitudes[EN_ESPERA];

// Marca si el proceso de nuevasSolicitudes ya fue ingresado a colaListos
static int yaIngresado[EN_ESPERA];

// Registro de tiempos de llegada unicos (0-799)
static int tiemposUsados[800];

// ─────────────────────────────────────────────────────────────────────────────
// BCP – inicializarProceso
// ─────────────────────────────────────────────────────────────────────────────

void inicializarProceso(Proceso *p, int index)
{
    memset(p, 0, sizeof(Proceso));

    // ID en orden alfabetico: A-0, B-1, ... Z-25, A-26, B-27 ...
    snprintf(p->id,     sizeof(p->id),     "%c-%d", 'A' + (index % 26), index);
    snprintf(p->nombre, sizeof(p->nombre), "Proceso_%d", index);

    p->tiempoLlegada   = 0;           // se asigna despues con generarTiempoUnico
    p->ciclosTotales   = rand() % 85001; // 0 a 85000 segun enunciado
    p->ciclosRestantes = p->ciclosTotales;

    p->rafagaActual    = 0;
    p->tiempoEjecucion = 0;
    p->tiempoEspera    = 0;
    p->tiempoRespuesta = 0;
    p->tiempoRetorno   = 0;

    p->estado          = 0;           // empieza LISTO

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

    p->variable1       = -1;
    p->variable2       = -1;
    p->enSeccionCritica= 0;

    p->usoMemoria      = rand() % 100;
    p->listaOrigen     = -1;          // se asigna en cargarListas()

    p->socioIndex             = -1;
    p->socioTerminado         = 0;
    p->reporteSocioGenerado   = 0;

    inicializarNRU(p);
    p->fallosPagina  = 0;
    p->reemplazosNRU = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// COLA ENLAZADA
// Recuerda: los nodos son temporales. Los BCPs viven en tablaProcesos[].
// Desencolar borra el nodo pero el BCP sigue intacto en su lista.
// ─────────────────────────────────────────────────────────────────────────────

void inicializarCola(Cola *c)
{
    c->frente  = NULL;
    c->final   = NULL;
    c->tamanio = 0;
}

void encolar(Cola *c, Proceso *p)
{
    Nodo *nuevo    = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso  = p;
    nuevo->siguiente = NULL;

    if (c->final == NULL)
        c->frente = c->final = nuevo;
    else
    {
        c->final->siguiente = nuevo;
        c->final = nuevo;
    }
    c->tamanio++;
}

Proceso *desencolar(Cola *c)
{
    // Solo elimina el NODO de la cola.
    // El Proceso* que contenia sigue vivo en tablaProcesos[].
    if (estaVacia(c))
        return NULL;

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

// Insercion ordenada por tiempoLlegada ascendente (para FCFS)
void insertarOrdenado(Cola *c, Proceso *p)
{
    Nodo *nuevo      = (Nodo *)malloc(sizeof(Nodo));
    nuevo->proceso   = p;
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

    nuevo->siguiente   = actual->siguiente;
    actual->siguiente  = nuevo;
    if (nuevo->siguiente == NULL)
        c->final = nuevo;
    c->tamanio++;
}

void liberarCola(Cola *c)
{
    // Solo libera los nodos, NO los BCPs (esos viven en tablaProcesos[])
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
    return es->disco.tamanio    +
           es->pantalla.tamanio +
           es->teclado.tamanio  +
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

// ─────────────────────────────────────────────────────────────────────────────
// INICIALIZACION DEL SISTEMA
// ─────────────────────────────────────────────────────────────────────────────

static int generarTiempoUnico(void)
{
    int t;
    do { t = rand() % 800; } while (tiemposUsados[t] == 1);
    tiemposUsados[t] = 1;
    return t;
}

// Ordena tablaProcesos[] por tiempoLlegada ascendente (burbuja simple)
static void ordenarPorLlegada(void)
{
    for (int i = 0; i < TOTAL_PROCESOS - 1; i++)
        for (int j = i + 1; j < TOTAL_PROCESOS; j++)
            if (tablaProcesos[i].tiempoLlegada > tablaProcesos[j].tiempoLlegada)
            {
                Proceso tmp       = tablaProcesos[i];
                tablaProcesos[i]  = tablaProcesos[j];
                tablaProcesos[j]  = tmp;
            }
}

void inicializarSistema(void)
{
    memset(tiemposUsados, 0, sizeof(tiemposUsados));
    memset(yaIngresado,   0, sizeof(yaIngresado));

    // Crear los 250 BCPs con ID, ciclosTotales y tiempoLlegada unico
    for (int i = 0; i < TOTAL_PROCESOS; i++)
    {
        inicializarProceso(&tablaProcesos[i], i);
        tablaProcesos[i].tiempoLlegada = generarTiempoUnico();
    }

    // Ordenar por tiempoLlegada para que los primeros 150 sean los que
    // llegan antes (arrancan en el ciclo) y los 100 restantes esperen
    ordenarPorLlegada();

    // Asignar socios despues de tener todos los BCPs listos
    asignarSocios();
}

// ─────────────────────────────────────────────────────────────────────────────
// CARGA DE LISTAS Y COLA INICIAL
//
// Llena las dos listas fijas con punteros a tablaProcesos[].
// Los primeros EN_SISTEMA procesos (los que llegan antes) van a
// listaProcesosEnEjecucion y se encolan en colaListos con estado=0.
// Los EN_ESPERA restantes van a listaNuevasSolicitudes con estado=0
// pero NO se encolan todavia; esperan a que el reloj los llame.
// ─────────────────────────────────────────────────────────────────────────────

void cargarListas(Cola *colaListos)
{
    // Lista 1: procesos que arrancan en el ciclo
    for (int i = 0; i < EN_SISTEMA; i++)
    {
        tablaProcesos[i].listaOrigen = 0;   // listaProcesosEnEjecucion
        tablaProcesos[i].estado      = 0;   // LISTO
        listaProcesosEnEjecucion[i]  = &tablaProcesos[i];

        // Se encolan en colaListos para que el planificador los atienda
        encolar(colaListos, &tablaProcesos[i]);

        printf("  [LISTA-EJE] %s | Llegada: %d | Ciclos: %d\n",
               tablaProcesos[i].id,
               tablaProcesos[i].tiempoLlegada,
               tablaProcesos[i].ciclosTotales);
    }

    // Lista 2: procesos que esperan su tiempoLlegada
    for (int i = 0; i < EN_ESPERA; i++)
    {
        int idx = EN_SISTEMA + i;
        tablaProcesos[idx].listaOrigen = 1; // listaNuevasSolicitudes
        tablaProcesos[idx].estado      = 0; // LISTO (esperando reloj)
        listaNuevasSolicitudes[i]      = &tablaProcesos[idx];
        yaIngresado[i]                 = 0; // todavia no entro a colaListos

        printf("  [LISTA-NUE] %s | Llegada: %d | Ciclos: %d\n",
               tablaProcesos[idx].id,
               tablaProcesos[idx].tiempoLlegada,
               tablaProcesos[idx].ciclosTotales);
    }

    printf("\n  [OK] Lista procesosEnEjecucion: %d procesos cargados\n",
           EN_SISTEMA);
    printf("  [OK] Lista nuevasSolicitudes:   %d procesos cargados\n\n",
           EN_ESPERA);
}

// ─────────────────────────────────────────────────────────────────────────────
// INGRESO DINAMICO DE NUEVAS SOLICITUDES
//
// Se llama en cada ciclo del planificador.
// Recorre listaNuevasSolicitudes[] y encola en colaListos a los procesos
// cuyo tiempoLlegada ya fue alcanzado por el reloj.
// El proceso NO sale de listaNuevasSolicitudes; solo se encola en colaListos
// y se marca como yaIngresado para no encolarlo dos veces.
// ─────────────────────────────────────────────────────────────────────────────

void ingresarNuevosSegunReloj(Cola *colaListos, int reloj)
{
    for (int i = 0; i < EN_ESPERA; i++)
    {
        if (yaIngresado[i])
            continue;

        Proceso *p = listaNuevasSolicitudes[i];

        if (p->tiempoLlegada <= reloj)
        {
            p->estado    = 0;   // LISTO
            p->bloqueado = 0;

            // Insertar ordenado por tiempoLlegada para respetar FCFS
            insertarOrdenado(colaListos, p);
            yaIngresado[i] = 1;

            printf("  [INGRESO] %s llega al ciclo (reloj=%d, llegada=%d)\n",
                   p->id, reloj, p->tiempoLlegada);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UTILIDADES DE CONTEO SOBRE LAS LISTAS
// ─────────────────────────────────────────────────────────────────────────────

// Cuantos procesos en listaProcesosEnEjecucion aun no terminaron
int contarActivosEnEjecucion(void)
{
    int cnt = 0;
    for (int i = 0; i < EN_SISTEMA; i++)
        if (listaProcesosEnEjecucion[i]->estado != 3)
            cnt++;
    return cnt;
}

// Cuantos procesos en listaNuevasSolicitudes aun no ingresaron al ciclo
int contarPendientesNuevas(void)
{
    int cnt = 0;
    for (int i = 0; i < EN_ESPERA; i++)
        if (!yaIngresado[i])
            cnt++;
    return cnt;
}

// ─────────────────────────────────────────────────────────────────────────────
// SOCIOS
// ─────────────────────────────────────────────────────────────────────────────

void asignarSocios(void)
{
    int indices[TOTAL_PROCESOS];
    for (int i = 0; i < TOTAL_PROCESOS; i++)
        indices[i] = i;

    // Fisher-Yates shuffle
    for (int i = TOTAL_PROCESOS - 1; i > 0; i--)
    {
        int j   = rand() % (i + 1);
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

    printf("  [SOCIOS] %d pares asignados\n", TOTAL_PROCESOS / 2);
}

void notificarTerminacion(Proceso *p, pthread_mutex_t *mtx)
{
    if (p->socioIndex < 0 || p->socioIndex >= TOTAL_PROCESOS)
        return;

    Proceso *socio = &tablaProcesos[p->socioIndex];

    pthread_mutex_lock(mtx);

    if (socio->estado == 3 && !p->reporteSocioGenerado)
    {
        p->reporteSocioGenerado    = 1;
        socio->reporteSocioGenerado = 1;

        FILE *f = fopen("socios.log", "a");
        if (f)
        {
            fprintf(f, "\n=== PAR DE SOCIOS COMPLETADO ===\n");
            fprintf(f, "  Proceso A : %-8s | Lista: %s | Retorno: %d | VecesEnCPU: %d\n",
                    p->id,
                    p->listaOrigen == 0 ? "EnEjecucion" : "NuevasSolicitudes",
                    p->tiempoRetorno, p->vecesEnCPU);
            fprintf(f, "  Proceso B : %-8s | Lista: %s | Retorno: %d | VecesEnCPU: %d\n",
                    socio->id,
                    socio->listaOrigen == 0 ? "EnEjecucion" : "NuevasSolicitudes",
                    socio->tiempoRetorno, socio->vecesEnCPU);
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
        p->marcosNRU[i].bitR         = 0;
        p->marcosNRU[i].bitM         = 0;
        p->marcosNRU[i].valido       = 0;
    }
}

static int claseNRU(MarcoNRU *m)
{
    // Clase 0: R=0,M=0  Clase 1: R=0,M=1
    // Clase 2: R=1,M=0  Clase 3: R=1,M=1
    return (m->bitR << 1) | m->bitM;
}

static int elegirVictimaNRU(Proceso *p)
{
    int mejor    = -1;
    int mejorCl  = 4;

    for (int i = 0; i < NRU_NUM_MARCOS; i++)
    {
        if (!p->marcosNRU[i].valido)
            return i;   // marco libre, usarlo directamente

        int cl = claseNRU(&p->marcosNRU[i]);
        if (cl < mejorCl)
        {
            mejorCl = cl;
            mejor   = i;
        }
    }
    return mejor;
}

void accederPaginaNRU(Proceso *p)
{
    int pagVirtual = rand() % NRU_NUM_PAGINAS;

    // Buscar si la pagina ya esta en algun marco (hit)
    for (int i = 0; i < NRU_NUM_MARCOS; i++)
    {
        if (p->marcosNRU[i].valido &&
            p->marcosNRU[i].numeroPagina == pagVirtual)
        {
            p->marcosNRU[i].bitR = 1;
            if (rand() % 3 == 0)        // 33% probabilidad de escritura
                p->marcosNRU[i].bitM = 1;
            return;
        }
    }

    // Fallo de pagina: elegir victima y reemplazar
    p->fallosPagina++;
    int victima = elegirVictimaNRU(p);

    if (p->marcosNRU[victima].valido)
    {
        p->reemplazosNRU++;
        printf("  [NRU ] %s | Reemplaza pag %d (clase %d) -> pag %d\n",
               p->id,
               p->marcosNRU[victima].numeroPagina,
               claseNRU(&p->marcosNRU[victima]),
               pagVirtual);
    }

    p->marcosNRU[victima].numeroPagina = pagVirtual;
    p->marcosNRU[victima].bitR         = 1;
    p->marcosNRU[victima].bitM         = (rand() % 3 == 0) ? 1 : 0;
    p->marcosNRU[victima].valido       = 1;
}

void limpiarBitsR(Proceso *p)
{
    for (int i = 0; i < NRU_NUM_MARCOS; i++)
        p->marcosNRU[i].bitR = 0;
}