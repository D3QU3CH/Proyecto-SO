#ifndef MODELO_H
#define MODELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

/* ═══════════════════════════════════════════════════════════════════════════
   CONSTANTES GLOBALES
   ═══════════════════════════════════════════════════════════════════════════ */
#define TOTAL_PROCESOS        250
#define PROCESOS_EN_CICLO     150
#define PROCESOS_EN_SOLICITUD 100

#define PALABRAS_POR_PAGINA   20
#define MARCOS_MIN            8
#define MARCOS_MAX            20
#define MAX_PAGINAS_PROCESO   64
#define MAX_PAGINAS_SWAP      2048

#define MAX_PALABRAS          8000
#define MAX_LEN_PALABRA       64
#define MAX_PALABRAS_POR_SLOT 512

/* Máximo de bloques Buddy asignados simultáneamente por proceso
   (1 inicial + hasta 5 crecimientos activos) */
#define MAX_BLOQUES_BUDDY_PROCESO 8

#define ALG_FCFS  0
#define ALG_RR    1

#define ESTADO_LISTO      0
#define ESTADO_EJECUTANDO 1
#define ESTADO_ESPERA_ES  2
#define ESTADO_TERMINADO  3

/* ═══════════════════════════════════════════════════════════════════════════
   BCP — 25 variables obligatorias + campos de soporte
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    /* --- 25 variables obligatorias --- */
    char id[12];           /*  1 */
    char nombre[50];       /*  2 */
    int  tiempoLlegada;    /*  3 */
    int  ciclosTotales;    /*  4 */
    int  ciclosRestantes;  /*  5 */
    int  rafagaActual;     /*  6 */
    int  tiempoEjecucion;  /*  7 */
    int  tiempoEspera;     /*  8 */
    int  tiempoRespuesta;  /*  9 */
    int  tiempoRetorno;    /* 10 */
    int  estado;           /* 11 */
    int  vecesEnCPU;       /* 12 */
    int  iteraciones;      /* 13 */
    int  restanteQuantum;  /* 14 */
    int  cambiosContexto;  /* 15 */
    int  esApropiativo;    /* 16 */
    int  tipoProceso;      /* 17  0=CPU-bound  1=ES-bound */
    int  aprovechamiento;  /* 18 */
    int  desperdicio;      /* 19 */
    int  dispositivoES;    /* 20 */
    int  tiempoES;         /* 21 */
    int  bloqueado;        /* 22 */
    int  variable1;        /* 23 */
    int  variable2;        /* 24 */
    int  fallosPagina;     /* 25 */

    /* --- Campos de soporte --- */
    int  yaIngresado;

    /* Buddy System: array de índices para todos los bloques asignados */
    int  memoriaUsadaKB;
    int  bloqueMemoriaKB;       /* tamaño total asignado (suma de bloques) */
    int  desperdicioInterno;    /* desperdicio interno acumulado           */
    int  idxsBuddy[MAX_BLOQUES_BUDDY_PROCESO]; /* índices de todos los bloques */
    int  numBloquesBuddy;       /* cantidad de bloques activos             */

    /* Crecimiento: 15 ceros + 5 valores 1-50 */
    int  crecimientoMem[20];
    int  indiceCrecimiento;

    /* Paginación NRU */
    int  numMarcos;
    int  numPaginas;
    int  paginasEnRAM[MAX_PAGINAS_PROCESO];
    int  bitReferencia[MAX_PAGINAS_PROCESO];
    int  bitModificado[MAX_PAGINAS_PROCESO];
} Proceso;

/* ═══════════════════════════════════════════════════════════════════════════
   TABLA DEL SISTEMA — 20 variables obligatorias
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    Proceso tablaBCPs[TOTAL_PROCESOS];
    int totalProcesos;           /*  1 */
    int procesosEnCiclo;         /*  2 */
    int procesosEnSolicitud;     /*  3 */
    int procesosEnColaListos;    /*  4 */
    int procesosEjecutando;      /*  5 */
    int procesosEnES;            /*  6 */
    int procesosTerminados;      /*  7 */
    int procesosBloqueados;      /*  8 */
    int algoritmoActual;         /*  9 */
    int quantumActual;           /* 10 */
    int cicloActual;             /* 11 */
    int totalCambiosContexto;    /* 12 */
    int totalFallosPagina;       /* 13 */
    int sumaEspera;              /* 14 */
    int sumaCiclosRestantes;     /* 15 */
    int promedioEspera;          /* 16 */
    int promedioCiclos;          /* 17 */
    int procesosIngresadosDinam; /* 18 */
    int memoriaLibreKB;          /* 19 */
    int desperdicioTotal;        /* 20 */
} TablaProcesos;

extern TablaProcesos tablaSistema;

/* ═══════════════════════════════════════════════════════════════════════════
   LISTA DOBLEMENTE ENLAZADA
   ═══════════════════════════════════════════════════════════════════════════ */
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

/* ═══════════════════════════════════════════════════════════════════════════
   COLA FIFO
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct NodoCola {
    Proceso         *proceso;
    struct NodoCola *siguiente;
} NodoCola;

typedef struct {
    NodoCola *frente;
    NodoCola *final;
    int       tamanio;
} Cola;

/* ═══════════════════════════════════════════════════════════════════════════
   SISTEMA E/S — 4 dispositivos
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    Cola disco;
    Cola pantalla;
    Cola teclado;
    Cola impresora;
} SistemaES;

/* ═══════════════════════════════════════════════════════════════════════════
   BUDDY SYSTEM
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    int tamanioKB;
    int baseDir;
    int libre;
    int indexProceso;
    int socioIdx;       /* índice del socio en el array; -1 si no tiene */
} BloqueBS;

typedef struct {
    BloqueBS        bloques[1024];
    int             numBloques;
    int             memoriaLibreKB;
    int             memoriaUsadaKB;
    int             desperdicioInternoTotal;
    pthread_mutex_t mutex;
} MemoriaBuddy;

extern MemoriaBuddy memoriaBuddy;

/* ═══════════════════════════════════════════════════════════════════════════
   BANCO DE PALABRAS
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    char palabras[MAX_PALABRAS][MAX_LEN_PALABRA];
    int  totalPalabras;
    int  cursor;
} BancoPalabras;

extern BancoPalabras bancoPalabras;

/* ═══════════════════════════════════════════════════════════════════════════
   PAGINACIÓN + ÁREA SWAP
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    char palabras[PALABRAS_POR_PAGINA][MAX_LEN_PALABRA];
    int  numPalabras;
    int  indiceProceso;
    int  indicePagina;
    int  bitR;          /* bit de referencia (NRU) */
    int  bitM;          /* bit de modificación (NRU) */
    int  tiempoEntrada;
} Pagina;

typedef struct {
    Pagina marcos[MARCOS_MAX * TOTAL_PROCESOS];
    int    numMarcosTotal;
    int    numMarcosOcupados;
} MemoriaPrincipal;

extern MemoriaPrincipal memoriaPrincipal;

typedef struct {
    Pagina paginas[MAX_PAGINAS_SWAP];
    int    numPaginas;
} AreaSwap;

extern AreaSwap areaSwap;

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORIA LEGACY (SLOTS) — para estadísticas
   ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    int  ocupado;
    int  indiceProceso;
    char palabras[MAX_PALABRAS_POR_SLOT][MAX_LEN_PALABRA];
    int  numPalabras;
    int  capacidadPalabras;
} SlotMemoria;

typedef struct {
    SlotMemoria slots[PROCESOS_EN_CICLO];
    int   numSlotsOcupados;
    int   desperdicioExterno;
    int   procesosEnEjecucion;
    int   procesosTerminados;
    int   tiempoTotalEjecucion;
    float promedioFinalizadosPorCiclo;
} MemoriaPrincipalLegacy;

extern MemoriaPrincipalLegacy memoriaPrincipalLegacy;

/* ═══════════════════════════════════════════════════════════════════════════
   PROTOTIPOS
   ═══════════════════════════════════════════════════════════════════════════ */

/* Inicialización */
void    inicializarTablaSistema(void);
void    poblarListas(Lista *enEjecucion, Lista *solicitudes);
void    actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                    Cola *colaListos, SistemaES *es, int reloj);

/* Lista */
void    inicializarLista(Lista *l);
void    insertarEnLista(Lista *l, Proceso *p);
void    eliminarDeLista(Lista *l, Proceso *p);
int     estaVaciaLista(Lista *l);

/* Cola */
void     inicializarCola(Cola *c);
void     encolar(Cola *c, Proceso *p);
void     encolarAlFrente(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int      estaVaciaCola(Cola *c);
void     moverAlFrenteCola(Cola *c, Proceso *p);

/* E/S */
void inicializarSistemaES(SistemaES *es);
void asignarES(Proceso *p, SistemaES *es);
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj);
void actualizarEspera(Cola *colaListos);

/* Buddy System */
void inicializarBuddy(void);
int  asignarMemoriaBuddy(Proceso *p, int memoriaKB);
void liberarTodosLosBloquesBuddy(Proceso *p);

/* Paginación NRU */
void inicializarPaginacion(void);
void asignarPaginasProceso(Proceso *p);
void liberarPaginasProceso(Proceso *p);
int  manejarFalloPagina(Proceso *p, int indicePagina, int cicloActual);
void resetarBitsR(int cicloActual);
void redimensionarMemoriaPrincipal(Lista *enEjecucion, int cicloActual);

/* Memoria Legacy */
void inicializarMemoriaPrincipal(void);
int  asignarSlotMemoria(Proceso *p);
void agregarPalabrasAlSlot(Proceso *p, int cantidad);
void liberarSlotMemoria(Proceso *p);
void crecerMemoriaProceso(Proceso *p);

/* Banco de palabras y frases */
void cargarPalabras(const char *rutaArchivo);
void cargarFrases(const char *ruta);
void procesarFraseES(Proceso *p, int cicloActual);

/* Estadísticas de memoria */
void calcularDesperdicioExterno(void);
void actualizarPromedioFinalizados(int cicloActual);
void mostrarEstadisticasMemoria(void);

/* CPU */
void procesarEntradaCPU(Proceso *p, int reloj);
void procesarTerminacion(Proceso *p, int reloj);

/* Cambio automático de algoritmo */
int  evaluarCambioAlgoritmo(Cola *colaListos, SistemaES *es);

#endif /* MODELO_H */