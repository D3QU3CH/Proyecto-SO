#ifndef MODELO_H
#define MODELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

// ─────────────────────────────────────────────────────────────────────────────
// BCP — Bloque de Control de Proceso (25 variables)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    char id[12];           //  1
    char nombre[50];       //  2
    int  tiempoLlegada;    //  3
    int  ciclosTotales;    //  4
    int  ciclosRestantes;  //  5
    int  rafagaActual;     //  6  ciclos que ejecutara esta instancia (10-70)
    int  tiempoEjecucion;  //  7
    int  tiempoEspera;     //  8
    int  tiempoRespuesta;  //  9
    int  tiempoRetorno;    // 10
    int  estado;           // 11  0=LISTO 1=EJECUTANDO 2=ESPERA_ES 3=TERMINADO 4=BLOQ_SC
    int  vecesEnCPU;       // 12
    int  iteraciones;      // 13
    int  restanteQuantum;  // 14
    int  cambiosContexto;  // 15  aleatorio 10-30, se re-sortea cada entrada a CPU
    int  esApropiativo;    // 16
    int  tipoProceso;      // 17  0=CPU-bound  1=ES-bound
    int  aprovechamiento;  // 18
    int  desperdicio;      // 19
    int  dispositivoES;    // 20  -1=ninguno 0=disco 1=pantalla 2=teclado 3=impresora
    int  tiempoES;         // 21
    int  bloqueado;        // 22
    int  variable1;        // 23
    int  variable2;        // 24
    int  enSeccionCritica; // 25

    // Flag: ya fue encolado en colaListos al menos una vez
    int  yaIngresado;

    // Buddy System
    int  memoriaUsadaKB;      // KB reales que necesita el proceso
    int  bloqueMemoriaKB;     // KB que le asigno el Buddy (potencia de 2)
    int  desperdicioInterno;  // bloqueMemoriaKB - memoriaUsadaKB

    // Crecimiento de memoria: 20 valores, 15 ceros + 5 entre 1-50
    int  crecimientoMem[20];
    int  indiceCrecimiento;
} Proceso;

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
// COLA FIFO  (colaListos y las 4 colas de E/S)
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
// SISTEMA E/S — 4 dispositivos con sus colas
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Cola disco;       // multiplicador x2
    Cola pantalla;    // multiplicador x4
    Cola teclado;     // multiplicador x8
    Cola impresora;   // multiplicador x12
} SistemaES;

// ─────────────────────────────────────────────────────────────────────────────
// BUDDY SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

typedef struct BloqueBS {
    int  tamanioKB;
    int  baseDir;
    int  libre;
    int  indexProceso;    // indice en tablaBCPs, -1 si libre
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
// CONTEXTO DE HILOS
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Lista     *procesosEnEjecucion;
    Lista     *solicitudes;
    Cola      *colaListos;
    SistemaES *es;
    int       *reloj;
    int       *terminado;

    pthread_mutex_t mutexPrincipal;
    sem_t semDisco;
    sem_t semPantalla;
    sem_t semTeclado;
    sem_t semImpresora;
} ContextoHilos;

// Argumento para cada hilo de dispositivo E/S
typedef struct {
    Cola            *colaES;
    Cola            *colaListos;
    pthread_mutex_t *mutex;
    sem_t           *sem;
    int             *terminado;
    const char      *nombre;
} ArgHiloES;

// ─────────────────────────────────────────────────────────────────────────────
// TABLA DE PROCESOS — 20 variables globales del sistema
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Proceso tablaBCPs[250];

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
extern MemoriaBuddy  memoriaBuddy;

// ─────────────────────────────────────────────────────────────────────────────
// PROTOTIPOS — modelo.c
// ─────────────────────────────────────────────────────────────────────────────

// Tabla e inicializacion
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
void asignarES(Proceso *p, SistemaES *es);

// Buddy
void inicializarBuddy(void);
int  asignarMemoriaBuddy(Proceso *p, int memoriaKB);
void liberarMemoriaBuddy(Proceso *p);

// Ingreso dinamico y espera
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj);
void actualizarEspera(Cola *colaListos);

#endif