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

typedef struct
{
    char id[12];          //  1
    char nombre[50];      //  2
    int tiempoLlegada;    //  3
    int ciclosTotales;    //  4
    int ciclosRestantes;  //  5
    int rafagaActual;     //  6  ciclos que ejecutara esta instancia (10-70)
    int tiempoEjecucion;  //  7
    int tiempoEspera;     //  8
    int tiempoRespuesta;  //  9
    int tiempoRetorno;    // 10
    int estado;           // 11  0=LISTO 1=EJECUTANDO 2=ESPERA_ES 3=TERMINADO 4=BLOQ_SC
    int vecesEnCPU;       // 12
    int iteraciones;      // 13
    int restanteQuantum;  // 14
    int cambiosContexto;  // 15  aleatorio 10-30, se re-sortea cada entrada a CPU
    int esApropiativo;    // 16
    int tipoProceso;      // 17  0=CPU-bound  1=ES-bound
    int aprovechamiento;  // 18
    int desperdicio;      // 19
    int dispositivoES;    // 20  -1=ninguno 0=disco 1=pantalla 2=teclado 3=impresora
    int tiempoES;         // 21
    int bloqueado;        // 22
    int variable1;        // 23
    int variable2;        // 24
    int enSeccionCritica; // 25

    // Flag: ya fue encolado en colaListos al menos una vez
    int yaIngresado;

    // Buddy System
    int memoriaUsadaKB;     // KB reales que necesita el proceso
    int bloqueMemoriaKB;    // KB que le asigno el Buddy (potencia de 2)
    int desperdicioInterno; // bloqueMemoriaKB - memoriaUsadaKB

    // Crecimiento de memoria: 20 valores, 15 ceros + 5 entre 1-50
    int crecimientoMem[20];
    int indiceCrecimiento;
} Proceso;

typedef struct
{
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

// ─────────────────────────────────────────────────────────────────────────────
// LISTA DOBLEMENTE ENLAZADA
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Nodo
{
    Proceso *proceso;
    struct Nodo *siguiente;
    struct Nodo *anterior;
} Nodo;

typedef struct
{
    Nodo *cabeza;
    Nodo *cola;
    int tamanio;
} Lista;

// ─────────────────────────────────────────────────────────────────────────────
// COLA FIFO  (colaListos y las 4 colas de E/S)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct NodoCola
{
    Proceso *proceso;
    struct NodoCola *siguiente;
} NodoCola;

typedef struct
{
    NodoCola *frente;
    NodoCola *final;
    int tamanio;
} Cola;

// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA E/S — 4 dispositivos con sus colas
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    Cola disco;     // multiplicador x2
    Cola pantalla;  // multiplicador x4
    Cola teclado;   // multiplicador x8
    Cola impresora; // multiplicador x12
} SistemaES;

// ─────────────────────────────────────────────────────────────────────────────
// BUDDY SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

typedef struct BloqueBS
{
    int tamanioKB;
    int baseDir;
    int libre;
    int indexProceso; // indice en tablaBCPs, -1 si libre
    struct BloqueBS *socio;
} BloqueBS;

typedef struct
{
    BloqueBS bloques[512];
    int numBloques;
    int memoriaLibreKB;
    int memoriaUsadaKB;
    int desperdicioInternoTotal;
    pthread_mutex_t mutex;
} MemoriaBuddy;

extern MemoriaBuddy memoriaBuddy;

// ─────────────────────────────────────────────────────────────────────────────
// CONTEXTO DE HILOS
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    Lista *procesosEnEjecucion;
    Lista *solicitudes;
    Cola *colaListos;
    SistemaES *es;
    int *reloj;
    int *terminado;

    pthread_mutex_t mutexPrincipal;
    sem_t semDisco;
    sem_t semPantalla;
    sem_t semTeclado;
    sem_t semImpresora;
} ContextoHilos;

// Argumento para cada hilo de dispositivo E/S
typedef struct
{
    Cola *colaES;
    Cola *colaListos;
    pthread_mutex_t *mutex;
    sem_t *sem;
    int *terminado;
    const char *nombre;
} ArgHiloES;

// ─────────────────────────────────────────────────────────────────────────────
// PROTOTIPOS — modelo.c
// ─────────────────────────────────────────────────────────────────────────────

// Tabla e inicializacion
void inicializarTablaSistema(void);
void poblarListas(Lista *enEjecucion, Lista *solicitudes);
void actualizarVariablesGlobales(Lista *enEjecucion, Lista *solicitudes,
                                 Cola *colaListos, SistemaES *es, int reloj);
// Lista
void inicializarLista(Lista *l);
void insertarEnLista(Lista *l, Proceso *p);
int estaVaciaLista(Lista *l);

// Cola
void inicializarCola(Cola *c);
void encolar(Cola *c, Proceso *p);
void encolarAlFrente(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int estaVaciaCola(Cola *c);

// E/S
void inicializarSistemaES(SistemaES *es);
void asignarES(Proceso *p, SistemaES *es);

// Ingreso dinamico y espera
void ingresarProcesosNuevos(Lista *solicitudes, Cola *colaListos, int reloj);
void actualizarEspera(Cola *colaListos);

// Buddy
void inicializarBuddy(void);
int asignarMemoriaBuddy(Proceso *p, int memoriaKB);
void liberarMemoriaBuddy(Proceso *p);

// Estadisticas de memoria
void calcularDesperdicioExterno(void);
void actualizarPromedioFinalizados(int cicloActual);

// ─────────────────────────────────────────────────────────────────────────────
// BANCO DE PALABRAS — palabras cargadas desde libro1.txt
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_PALABRAS 8000
#define MAX_LEN_PALABRA 64

typedef struct
{
    char palabras[MAX_PALABRAS][MAX_LEN_PALABRA];
    int totalPalabras; // cuantas se leyeron del archivo
    int cursor;        // siguiente palabra disponible para repartir
} BancoPalabras;

extern BancoPalabras bancoPalabras;

// ─────────────────────────────────────────────────────────────────────────────
// SLOT DE MEMORIA PRINCIPAL — espacio de un proceso en RAM
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_PALABRAS_POR_SLOT 512

typedef struct
{
    int ocupado;                                           // 1=tiene proceso, 0=libre
    int indiceProceso;                                     // indice en tablaBCPs
    char palabras[MAX_PALABRAS_POR_SLOT][MAX_LEN_PALABRA]; // palabras en RAM
    int numPalabras;                                       // cuantas tiene ahora
    int capacidadPalabras;                                 // cuantas caben segun Buddy
} SlotMemoria;

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA PRINCIPAL — 150 slots, uno por proceso del ciclo
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    SlotMemoria slots[150];
    int numSlotsOcupados;

    // Estadisticas
    int desperdicioInternoTotal;       // sum(bloqueKB - realKB) de todos
    int desperdicioExterno;            // memoria libre no contigua inutilizable
    int procesosEnEjecucion;           // slots ocupados activos
    int procesosTerminados;            // que ya liberaron su slot
    int tiempoTotalEjecucion;          // suma tiempoEjecucion de terminados
    float promedioFinalizadosPorCiclo; // 
} MemoriaPrincipal;

extern MemoriaPrincipal memoriaPrincipal;

// Banco de palabras
void cargarPalabras(const char *rutaArchivo);

// Memoria Principal
void inicializarMemoriaPrincipal(void);
int asignarSlotMemoria(Proceso *p);
void agregarPalabrasAlSlot(Proceso *p, int cantidad);
void liberarSlotMemoria(Proceso *p);
void crecerMemoriaProceso(Proceso *p);
void mostrarEstadisticasMemoria(void);

#endif