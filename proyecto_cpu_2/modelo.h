#ifndef MODELO_H
#define MODELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

// ─── CONFIGURACION (ajusta para pruebas) ─────────────────────────────────────
#define TOTAL_PROCESOS        250
#define PROCESOS_EN_CICLO     150
#define PROCESOS_EN_SOLICITUD 100

// ─── PAGINACION ───────────────────────────────────────────────────────────────
#define PALABRAS_POR_PAGINA   20
#define MARCOS_MIN            8
#define MARCOS_MAX            20
#define MAX_PAGINAS_PROCESO   64   // maximo de paginas por proceso
#define MAX_PAGINAS_SWAP      1024

// ─── PALABRAS ────────────────────────────────────────────────────────────────
#define MAX_PALABRAS          8000
#define MAX_LEN_PALABRA       64
#define MAX_PALABRAS_POR_SLOT 512

// ─── ALGORITMOS ──────────────────────────────────────────────────────────────
#define ALG_FCFS  0
#define ALG_RR    1

// ─────────────────────────────────────────────────────────────────────────────
// BCP — 25 variables
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    char id[12];          //  1
    char nombre[50];      //  2
    int  tiempoLlegada;   //  3
    int  ciclosTotales;   //  4
    int  ciclosRestantes; //  5
    int  rafagaActual;    //  6
    int  tiempoEjecucion; //  7
    int  tiempoEspera;    //  8
    int  tiempoRespuesta; //  9
    int  tiempoRetorno;   // 10
    int  estado;          // 11  0=LISTO 1=EJECUTANDO 2=ESPERA_ES 3=TERMINADO 4=BLOQ_SC
    int  vecesEnCPU;      // 12
    int  iteraciones;     // 13
    int  restanteQuantum; // 14
    int  cambiosContexto; // 15
    int  esApropiativo;   // 16
    int  tipoProceso;     // 17  0=CPU-bound 1=ES-bound
    int  aprovechamiento; // 18
    int  desperdicio;     // 19
    int  dispositivoES;   // 20  -1=ninguno 0=disco 1=pantalla 2=teclado 3=impresora
    int  tiempoES;        // 21
    int  bloqueado;       // 22
    int  variable1;       // 23
    int  variable2;       // 24
    int  enSeccionCritica;// 25

    int yaIngresado;

    // Buddy System
    int memoriaUsadaKB;
    int bloqueMemoriaKB;
    int desperdicioInterno;

    // Crecimiento
    int crecimientoMem[20];
    int indiceCrecimiento;

    // Paginacion NRU
    int numMarcos;           // marcos asignados en RAM (8-20)
    int numPaginas;          // total paginas del proceso
    int paginasEnRAM[MAX_PAGINAS_PROCESO];   // indice de pagina -> marco RAM (-1=en swap)
    int bitReferencia[MAX_PAGINAS_PROCESO];  // bit R NRU
    int bitModificado[MAX_PAGINAS_PROCESO];  // bit M NRU
    int fallosPagina;
    int ciclosDesdeUltimoReset; // para reset periodico de bits R
} Proceso;

// ─────────────────────────────────────────────────────────────────────────────
// TABLA DEL SISTEMA — 20 variables
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    Proceso tablaBCPs[TOTAL_PROCESOS];

    int totalProcesos;           //  1
    int procesosEnCiclo;         //  2
    int procesosEnSolicitud;     //  3
    int procesosEnColaListos;    //  4
    int procesosEjecutando;      //  5
    int procesosEnES;            //  6
    int procesosTerminados;      //  7
    int procesosBloqueados;      //  8
    int algoritmoActual;         //  9  ALG_FCFS / ALG_RR
    int quantumActual;           // 10
    int cicloActual;             // 11
    int totalCambiosContexto;    // 12
    int totalFallosPagina;       // 13
    int sumaEspera;              // 14
    int sumaCiclosRestantes;     // 15
    int promedioEspera;          // 16
    int promedioCiclos;          // 17
    int procesosIngresadosDinam; // 18
    int memoriaLibreKB;          // 19
    int desperdicioTotal;        // 20
} TablaProcesos;

extern TablaProcesos tablaSistema;

// ─────────────────────────────────────────────────────────────────────────────
// LISTA DOBLEMENTE ENLAZADA
// ─────────────────────────────────────────────────────────────────────────────
typedef struct Nodo {
    Proceso     *proceso;
    struct Nodo *siguiente;
    struct Nodo *anterior;
} Nodo;

typedef struct {
    Nodo *cabeza;
    Nodo *cola;
    int   tamanio;
} Lista;

// ─────────────────────────────────────────────────────────────────────────────
// COLA FIFO
// ─────────────────────────────────────────────────────────────────────────────
typedef struct NodoCola {
    Proceso         *proceso;
    struct NodoCola *siguiente;
} NodoCola;

typedef struct {
    NodoCola *frente;
    NodoCola *final;
    int       tamanio;
} Cola;

// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA E/S
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    Cola disco;
    Cola pantalla;
    Cola teclado;
    Cola impresora;
} SistemaES;

// ─────────────────────────────────────────────────────────────────────────────
// BUDDY SYSTEM
// ─────────────────────────────────────────────────────────────────────────────
typedef struct BloqueBS {
    int tamanioKB;
    int baseDir;
    int libre;
    int indexProceso;
    struct BloqueBS *socio;
} BloqueBS;

typedef struct {
    BloqueBS        bloques[512];
    int             numBloques;
    int             memoriaLibreKB;
    int             memoriaUsadaKB;
    int             desperdicioInternoTotal;
    pthread_mutex_t mutex;
} MemoriaBuddy;

extern MemoriaBuddy memoriaBuddy;

// ─────────────────────────────────────────────────────────────────────────────
// BANCO DE PALABRAS
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    char palabras[MAX_PALABRAS][MAX_LEN_PALABRA];
    int  totalPalabras;
    int  cursor;
} BancoPalabras;

extern BancoPalabras bancoPalabras;

// ─────────────────────────────────────────────────────────────────────────────
// PAGINA (RAM o SWAP)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    char palabras[PALABRAS_POR_PAGINA][MAX_LEN_PALABRA];
    int  numPalabras;
    int  indiceProceso;  // a quien pertenece (-1=libre)
    int  indicePagina;   // que pagina del proceso es
    int  bitR;           // NRU: referenciada
    int  bitM;           // NRU: modificada
    int  tiempoEntrada;  // ciclo en que entro a RAM (para FIFO interno si se quiere)
} Pagina;

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA PRINCIPAL (paginada)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    Pagina marcos[MARCOS_MAX * TOTAL_PROCESOS]; // pool de marcos
    int    numMarcosTotal;
    int    numMarcosOcupados;

    // Estadisticas
    int   desperdicioExterno;
    int   procesosEnEjecucion;
    int   procesosTerminados;
    int   tiempoTotalEjecucion;
    float promedioFinalizadosPorCiclo;
} MemoriaPrincipal;

extern MemoriaPrincipal memoriaPrincipal;

// ─────────────────────────────────────────────────────────────────────────────
// AREA SWAP
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    Pagina paginas[MAX_PAGINAS_SWAP];
    int    numPaginas;
} AreaSwap;

extern AreaSwap areaSwap;

// Slot legacy (compatibilidad con codigo existente)
typedef struct {
    int  ocupado;
    int  indiceProceso;
    char palabras[MAX_PALABRAS_POR_SLOT][MAX_LEN_PALABRA];
    int  numPalabras;
    int  capacidadPalabras;
} SlotMemoria;

typedef struct {
    SlotMemoria slots[PROCESOS_EN_CICLO];
    int  numSlotsOcupados;
    int  desperdicioInternoTotal;
    int  desperdicioExterno;
    int  procesosEnEjecucion;
    int  procesosTerminados;
    int  tiempoTotalEjecucion;
    float promedioFinalizadosPorCiclo;
} MemoriaPrincipalLegacy;

extern MemoriaPrincipalLegacy memoriaPrincipalLegacy;

// ─────────────────────────────────────────────────────────────────────────────
// CONTEXTO DE HILOS
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    Lista  *procesosEnEjecucion;
    Lista  *solicitudes;
    Cola   *colaListos;
    SistemaES *es;
    int    *reloj;
    int    *terminado;
    int    *algoritmo;       // puntero al algoritmo actual
    int    *quantum;         // puntero al quantum actual
    int    *procesoPrivilId; // indice del proceso apropiativo (-1=ninguno)

    pthread_mutex_t mutexPrincipal;
    sem_t semDisco;
    sem_t semPantalla;
    sem_t semTeclado;
    sem_t semImpresora;
} ContextoHilos;

typedef struct {
    Cola            *colaES;
    Cola            *colaListos;
    pthread_mutex_t *mutex;
    sem_t           *sem;
    int             *terminado;
    const char      *nombre;
} ArgHiloES;

// ─────────────────────────────────────────────────────────────────────────────
// PROTOTIPOS — modelo.c
// ─────────────────────────────────────────────────────────────────────────────

// Inicializacion
void inicializarTablaSistema(void);
void poblarListas(Lista *enEjecucion, Lista *solicitudes);
void actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                  Cola *colaListos, SistemaES *es, int reloj);
// Lista
void    inicializarLista(Lista *l);
void    insertarEnLista(Lista *l, Proceso *p);
int     estaVaciaLista(Lista *l);

// Cola
void     inicializarCola(Cola *c);
void     encolar(Cola *c, Proceso *p);
void     encolarAlFrente(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int      estaVaciaCola(Cola *c);

// E/S
void inicializarSistemaES(SistemaES *es);
void asignarES(Proceso *p, SistemaES *es, Cola *colaListos);

// Ingreso dinamico
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj);
void actualizarEspera(Cola *colaListos);

// Buddy
void inicializarBuddy(void);
int  asignarMemoriaBuddy(Proceso *p, int memoriaKB);
void liberarMemoriaBuddy(Proceso *p);

// Paginacion NRU
void inicializarPaginacion(void);
void asignarPaginasProceso(Proceso *p);
void liberarPaginasProceso(Proceso *p);
int  manejarFalloPagina(Proceso *p, int indicePagina, int cicloActual);
void resetarBitsR(int cicloActual);
void redimensionarMemoriaPrincipal(Lista *enEjecucion, int cicloActual);

// Memoria legacy (slots + palabras)
void inicializarMemoriaPrincipal(void);
int  asignarSlotMemoria(Proceso *p);
void agregarPalabrasAlSlot(Proceso *p, int cantidad);
void liberarSlotMemoria(Proceso *p);
void crecerMemoriaProceso(Proceso *p);

// Banco de palabras y frases
void cargarPalabras(const char *rutaArchivo);
void cargarFrases(const char *ruta);

// Estadisticas
void calcularDesperdicioExterno(void);
void actualizarPromedioFinalizados(int cicloActual);
void mostrarEstadisticasMemoria(void);

// CPU
void procesarEntradaCPU(Proceso *p);
void procesarTerminacion(Proceso *p);

// Cambio automatico de algoritmo
int  evaluarCambioAlgoritmo(Cola *colaListos, SistemaES *es);

#endif