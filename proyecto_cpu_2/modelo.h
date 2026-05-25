#ifndef MODELO_H
#define MODELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

// ── Configuracion del sistema ─────────────────────────────────────────────────
#define TOTAL_PROCESOS      250
#define EN_EJECUCION        150
#define EN_SOLICITUDES      100
#define TIEMPO_LLEGADA_MAX  800
#define CICLOS_MIN          5000
#define CICLOS_MAX          85000
#define CC_MIN              10
#define CC_MAX              30

// ── E/S multiplicadores ───────────────────────────────────────────────────────
#define MULT_DISCO       2
#define MULT_PANTALLA    4
#define MULT_TECLADO     8
#define MULT_IMPRESORA  12

// ── Buddy System ──────────────────────────────────────────────────────────────
#define BUDDY_BASE_KB     4
#define MEMORIA_TOTAL_KB  1024

// ── NRU ───────────────────────────────────────────────────────────────────────
#define MARCOS_MIN        8
#define MARCOS_MAX        20
#define PALABRAS_POR_PAG  20
#define MAX_PALABRAS      5000
#define MAX_LEN_PALABRA   64

// ─────────────────────────────────────────────────────────────────────────────
// MARCO NRU
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    int  numeroPagina;
    int  bitR;
    int  bitM;
    int  valido;
    char palabras[PALABRAS_POR_PAG][MAX_LEN_PALABRA];
} MarcoNRU;

// ─────────────────────────────────────────────────────────────────────────────
// BCP — 25 variables oficiales
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
    int  estado;          // 11  0=LISTO 1=EJEC 2=ES 3=FIN 4=BLOQ_SC
    int  vecesEnCPU;      // 12
    int  iteraciones;     // 13
    int  restanteQuantum; // 14
    int  cambiosContexto; // 15
    int  esApropiativo;   // 16
    int  tipoProceso;     // 17  0=CPU 1=ES
    int  aprovechamiento; // 18
    int  desperdicio;     // 19
    int  dispositivoES;   // 20  -1=ninguno
    int  tiempoES;        // 21
    int  bloqueado;       // 22
    int  variable1;       // 23
    int  variable2;       // 24
    int  enSeccionCritica;// 25

    // Extras memoria
    int  bloqueMemoriaKB;
    int  memoriaUsadaKB;
    int  desperdicioInterno;
    int  crecimientoMem[20];
    int  indiceCrecimiento;

    // Extras NRU
    int       fallosPagina;
    int       reemplazosNRU;
    MarcoNRU *marcos;
    int       numMarcos;
    int       numPaginas;
    int      *paginasSwap;
    int       numSwap;
} Proceso;

// ─────────────────────────────────────────────────────────────────────────────
// NODO de lista doble
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Nodo {
    Proceso     *proceso;
    struct Nodo *siguiente;
    struct Nodo *anterior;
} Nodo;

// Lista doblemente enlazada (procesosEnEjecucion / nuevasSolicitudes)
// Los procesos NUNCA se eliminan; solo cambia su estado.
typedef struct {
    Nodo *cabeza;
    Nodo *cola;
    int   tamanio;
} Lista;

// ─────────────────────────────────────────────────────────────────────────────
// COLA FIFO (colaListos + colas E/S)
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
    int  tamanioKB;
    int  baseDir;
    int  libre;
    int  indexProceso;
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

// ─────────────────────────────────────────────────────────────────────────────
// LIBRO DE PALABRAS
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    char palabras[MAX_PALABRAS][MAX_LEN_PALABRA];
    int  totalPalabras;
} LibroPalabras;

// ─────────────────────────────────────────────────────────────────────────────
// CONTEXTO COMPARTIDO ENTRE HILOS
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Lista    *procesosEnEjecucion;
    Lista    *nuevasSolicitudes;
    Cola     *colaListos;
    SistemaES *es;
    int      *reloj;
    int      *terminado;

    pthread_mutex_t mutexPrincipal;
    pthread_mutex_t mutexMemoria;

    sem_t semDisco;
    sem_t semPantalla;
    sem_t semTeclado;
    sem_t semImpresora;
} ContextoHilos;

// ─────────────────────────────────────────────────────────────────────────────
// TABLA DE PROCESOS (variables globales del sistema)
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
    int algoritmoActual;         //  9
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

// ─────────────────────────────────────────────────────────────────────────────
// GLOBALES
// ─────────────────────────────────────────────────────────────────────────────

extern TablaProcesos tablaSistema;
extern LibroPalabras libroPalabras;
extern MemoriaBuddy  memoriaBuddy;

// ─────────────────────────────────────────────────────────────────────────────
// PROTOTIPOS
// ─────────────────────────────────────────────────────────────────────────────

// BCP
void inicializarProceso(Proceso *p, int index);
void liberarProceso(Proceso *p);

// Lista
void   inicializarLista(Lista *l);
void   insertarEnLista(Lista *l, Proceso *p);
int    estaVaciaLista(Lista *l);

// Cola
void     inicializarCola(Cola *c);
void     encolar(Cola *c, Proceso *p);
void     encolarAlFrente(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int      estaVaciaCola(Cola *c);

// E/S
void inicializarSistemaES(SistemaES *es);
int  contarES(SistemaES *es);
void asignarES(Proceso *p, SistemaES *es);

// Tabla
void inicializarTablaSistema(void);
void poblarListas(Lista *enEjecucion, Lista *solicitudes);
void actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                  Cola *colaListos, SistemaES *es, int reloj);

// Libro
int  cargarLibro(const char *ruta);
void obtenerPalabras(int inicio, int cantidad,
                     char destino[][MAX_LEN_PALABRA], int *obtenidas);

// Buddy
void inicializarBuddy(void);
int  asignarMemoriaBuddy(Proceso *p, int memoriaKB);
void liberarMemoriaBuddy(Proceso *p);

// NRU
void inicializarNRU(Proceso *p, int numMarcos);
void redimensionarNRU(Proceso *p, int nuevosMarcos);
void accederPaginaNRU(Proceso *p, int pagVirtual);
void limpiarBitsR(Proceso *p);

// Crecimiento de memoria
void generarCrecimientoMem(Proceso *p);
int  siguienteCrecimiento(Proceso *p);

// Redimension masiva
void redimensionarMitadProcesos(Lista *enEjecucion);

#endif