#ifndef MODELO_H
#define MODELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


// CONSTANTES
#define TOTAL_PROCESOS        250
#define PROCESOS_EN_CICLO     150
#define PROCESOS_EN_SOLICITUD 100

#define ALG_FCFS 0
#define ALG_RR   1

#define ESTADO_LISTO      0
#define ESTADO_EJECUTANDO 1
#define ESTADO_ESPERA_ES  2
#define ESTADO_TERMINADO  3

// Buddy: memoria total 8 MB, bloques minimo 4 KB
#define BUDDY_MEMORIA_TOTAL_KB  8192
#define BUDDY_MIN_KB            4
#define BUDDY_MAX_BLOQUES       2048

// Paginacion NRU
#define PALABRAS_POR_PAGINA    20
#define MARCOS_MIN  2
#define MARCOS_MAX  6
#define MAX_PAGINAS_PROCESO    32
#define MAX_PAGINAS_SWAP      4096
#define MAX_BLOQUES_BUDDY_PROC  8

// Banco de palabras
#define MAX_PALABRAS       8000
#define MAX_LEN_PALABRA      64
#define MAX_FRASES          200
#define MAX_LEN_FRASE       256

// Ciclos acumulados antes de solicitar E/S
#define CICLOS_PARA_ES     100


// BCP - 25 variables obligatorias + soporte
typedef struct {
    // 25 obligatorias
    char id[12];            /*  1 */
    char nombre[50];        /*  2 */
    int  tiempoLlegada;     /*  3 */
    int  ciclosTotales;     /*  4 */
    int  ciclosRestantes;   /*  5 */
    int  rafagaActual;      /*  6 */
    int  tiempoEjecucion;   /*  7 */
    int  tiempoEspera;      /*  8 */
    int  tiempoRespuesta;   /*  9 */
    int  tiempoRetorno;     /* 10 */
    int  estado;            /* 11 */
    int  vecesEnCPU;        /* 12 */
    int  iteraciones;       /* 13 */
    int  restanteQuantum;   /* 14 */
    int  cambiosContexto;   /* 15 */
    int  esApropiativo;     /* 16 */
    int  tipoProceso;       /* 17  0=CPU-bound 1=IO-bound */
    int  aprovechamiento;   /* 18 */
    int  desperdicio;       /* 19 */
    int  dispositivoES;     /* 20 */
    int  tiempoES;          /* 21 */
    int  bloqueado;         /* 22 */
    int  variable1;         /* 23 */
    int  variable2;         /* 24 */
    int  fallosPagina;      /* 25 */

    // Soporte
    int  ciclosEnEjecucion;     // acumulador para trigger E/S cada 200
    int  yaIngresado;

    // Buddy
    int  memoriaUsadaKB;
    int  bloqueMemoriaKB;
    int  desperdicioInterno;
    int  idxsBuddy[MAX_BLOQUES_BUDDY_PROC];
    int  numBloquesBuddy;

    // Crecimiento: 15 ceros + 5 valores 1-50
    int  crecimientoMem[20];
    int  indiceCrecimiento;

    // Paginacion NRU
    int  numMarcos;
    int  numPaginas;
    int  paginasEnRAM[MAX_PAGINAS_PROCESO];
    int  bitReferencia[MAX_PAGINAS_PROCESO];
    int  bitModificado[MAX_PAGINAS_PROCESO];
} Proceso;


// Tabla del sistema - 20 variables obligatorias
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


// Lista doblemente enlazada
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


// Cola FIFO
typedef struct NodoCola {
    Proceso         *proceso;
    struct NodoCola *siguiente;
} NodoCola;

typedef struct {
    NodoCola *frente;
    NodoCola *final;
    int       tamanio;
} Cola;


// Sistema E/S - 4 dispositivos
typedef struct {
    Cola disco;      // x2
    Cola pantalla;   // x4
    Cola teclado;    // x8
    Cola impresora;  // x12
} SistemaES;


// Buddy System
typedef struct {
    int tamanioKB;
    int baseDir;
    int libre;
    int indexProceso;
    int socioIdx;
} BloqueBS;

typedef struct {
    BloqueBS        bloques[BUDDY_MAX_BLOQUES];
    int             numBloques;
    int             memoriaLibreKB;
    int             memoriaUsadaKB;
    int             desperdicioInternoTotal;
    pthread_mutex_t mutex;
} MemoriaBuddy;

extern MemoriaBuddy memoriaBuddy;


// Banco de palabras y frases
typedef struct {
    char palabras[MAX_PALABRAS][MAX_LEN_PALABRA];
    int  totalPalabras;
    int  cursor;
} BancoPalabras;

extern BancoPalabras bancoPalabras;


// Paginacion NRU + Swap
typedef struct {
    char palabras[PALABRAS_POR_PAGINA][MAX_LEN_PALABRA];
    int  numPalabras;
    int  indiceProceso;
    int  indicePagina;
    int  bitR;
    int  bitM;
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


// Estadisticas generales
typedef struct {
    int   procesosTerminados;
    int   procesosEnEjecucion;
    int   tiempoTotalEjecucion;
    int   desperdicioExterno;
    float promedioFinalizadosPorCiclo;
} EstadisticasMem;

extern EstadisticasMem estadMem;


// Prototipos - modelo.c

// Inicializacion
void    inicializarTablaSistema(void);
void    poblarListas(Lista *enEjecucion, Lista *solicitudes);
void    actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                    Cola *colaListos, SistemaES *es, int reloj);

// Lista
void    inicializarLista(Lista *l);
void    insertarEnLista(Lista *l, Proceso *p);
void    eliminarDeLista(Lista *l, Proceso *p);
int     estaVaciaLista(Lista *l);

// Cola
void     inicializarCola(Cola *c);
void     encolar(Cola *c, Proceso *p);
void     encolarAlFrente(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int      estaVaciaCola(Cola *c);
void     moverAlFrenteCola(Cola *c, Proceso *p);

// E/S
void inicializarSistemaES(SistemaES *es);
void asignarES(Proceso *p, SistemaES *es, int cicloActual);
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj);
void actualizarEspera(Cola *colaListos);

// CPU
void procesarEntradaCPU(Proceso *p, int reloj);
void procesarTerminacion(Proceso *p, int reloj);

// Buddy System
void inicializarBuddy(void);
int  asignarMemoriaBuddy(Proceso *p, int memoriaKB);
void liberarTodosLosBloquesBuddy(Proceso *p);

// Paginacion NRU
void inicializarPaginacion(void);
void asignarPaginasProceso(Proceso *p);
void liberarPaginasProceso(Proceso *p);
int  manejarFalloPagina(Proceso *p, int indicePagina, int cicloActual);
void resetarBitsR(int cicloActual);
void redimensionarMemoriaPrincipal(Lista *enEjecucion, int cicloActual);
void crecerMemoriaProceso(Proceso *p);

// Banco de palabras
void cargarPalabras(const char *ruta);
void cargarFrases(const char *ruta);
void procesarFraseES(Proceso *p, int cicloActual);

// Estadisticas
void calcularDesperdicioExterno(void);
void actualizarPromedioFinalizados(int cicloActual);
void mostrarEstadisticasMemoria(void);

// Cambio automatico de algoritmo
int  evaluarCambioAlgoritmo(Cola *colaListos, SistemaES *es);

#endif