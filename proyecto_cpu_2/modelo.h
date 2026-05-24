#ifndef MODELO_H
#define MODELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

// ─────────────────────────────────────────────────────────────────────────────
// CONSTANTES GLOBALES
// ─────────────────────────────────────────────────────────────────────────────

#define TOTAL_PROCESOS 250 // total de BCPs en la simulacion
#define EN_SISTEMA 150     // inician en la cola de listos
                           // los 100 restantes esperan en cola
#define TAM_MEM 20         // recursos compartidos (frutas)

// Multiplicadores de tiempo por dispositivo de E/S

#define MULT_DISCO 2
#define MULT_PANTALLA 4
#define MULT_TECLADO 8
#define MULT_IMPRESORA 12

// ─── NRU ─────────────────────────────────────────────────────────────────────

#define NRU_NUM_MARCOS 8   // marcos de pagina simulados por proceso
#define NRU_NUM_PAGINAS 16 // paginas virtuales por proceso

// ─────────────────────────────────────────────────────────────────────────────
// PAGINA NRU (Not Recently Used)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    int numeroPagina; // pagina virtual que ocupa este marco
    int bitR;         // referenciado en el intervalo actual
    int bitM;         // modificado (dirty)
    int valido;       // 1 = marco ocupado
} MarcoNRU;

// ─────────────────────────────────────────────────────────────────────────────
// BCP – BLOQUE DE CONTROL DE PROCESO (25 variables oficiales + extras)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{

    char id[10];     //  1. identificador unico (ej. "A-0")
    char nombre[50]; //  2. nombre descriptivo

    int tiempoLlegada;   //  3. ciclo en que llega al sistema
    int ciclosTotales;   //  4. total de ciclos que necesita
    int ciclosRestantes; //  5. ciclos pendientes por ejecutar
    int rafagaActual;    //  6. ciclos ejecutados en la iteracion
    int tiempoEjecucion; //  7. tiempo total acumulado en CPU
    int tiempoEspera;    //  8. tiempo acumulado en cola Listos
    int tiempoRespuesta; //  9. ciclos hasta primera ejecucion
    int tiempoRetorno;   // 10. ciclos hasta finalizacion

    /* 11. Estado
       0=listo  1=ejecutando  2=espera E/S
       3=terminado  4=bloqueado (seccion critica)
    */
    int estado;

    int vecesEnCPU;      // 12. cuantas veces entro al CPU
    int iteraciones;     // 13. veces procesado en el ciclo
    int restanteQuantum; // 14. quantum sobrante en RR
    int cambiosContexto; // 15. cambios de contexto
    int esApropiativo;   // 16. 1 = tiene prioridad
    int tipoProceso;     // 17. 0=CPU-intensivo  1=ES-intensivo

    int aprovechamiento; // 18. % de uso del quantum asignado
    int desperdicio;     // 19. quantum desperdiciado

    int dispositivoES; // 20. dispositivo asignado (-1=ninguno)
    int tiempoES;      // 21. ciclos restantes en E/S
    int bloqueado;     // 22. 1 = en cola de E/S o SC

    int variable1;        // 23. recurso asignado 1
    int variable2;        // 24. recurso asignado 2
    int enSeccionCritica; // 25. usando recursos

    // ── Extras (no cuentan como BCP oficial) ─────────────────────────────────

    int usoMemoria; // uso de memoria simulado (%)

    // ── Socios ───────────────────────────────────────────────────────────────
    int socioIndex;           // indice del proceso socio (-1 = sin socio)
    int socioTerminado;       // 1 = el socio ya finalizo
    int reporteSocioGenerado; // 1 = reporte conjunto ya escrito

    // ── NRU ──────────────────────────────────────────────────────────────────
    MarcoNRU marcosNRU[NRU_NUM_MARCOS]; // tabla de marcos del proceso
    int fallosPagina;                   // contador de page faults
    int reemplazosNRU;                  // veces que se ejecuto NRU

} Proceso;

// ─────────────────────────────────────────────────────────────────────────────
// COLA ENLAZADA
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Nodo
{
    Proceso *proceso;
    struct Nodo *siguiente;
} Nodo;

typedef struct
{
    Nodo *frente;
    Nodo *final;
    int tamanio;
} Cola;

// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA DE E/S (4 dispositivos)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    Cola disco;
    Cola pantalla;
    Cola teclado;
    Cola impresora;
} SistemaES;

// ─────────────────────────────────────────────────────────────────────────────
// CONTEXTO COMPARTIDO ENTRE HILOS
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    Cola *procesosEnCiclo;
    Cola *nuevasSolicitudes;
    SistemaES *es;
    int *algoritmo;
    int *quantum;
    int *ciclos;
    int *terminado; // bandera de fin de simulacion

    // Mutex principal que protege toda la estructura de colas
    pthread_mutex_t mutexPrincipal;

    // Semaforos: un sem por dispositivo E/S para despertar al hilo
    sem_t *semDisco;
    sem_t *semPantalla;
    sem_t *semTeclado;
    sem_t *semImpresora;

    // Mutex para socios (reporte conjunto)
    pthread_mutex_t mutexSocios;
} ContextoHilos;

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA PRINCIPAL (frutas compartidas)
// ─────────────────────────────────────────────────────────────────────────────

extern char memoria[TAM_MEM][20];
extern int recursoOcupado[TAM_MEM];

// ─────────────────────────────────────────────────────────────────────────────
// TABLA GLOBAL DE BCPS
// ─────────────────────────────────────────────────────────────────────────────

extern Proceso tablaProcesos[TOTAL_PROCESOS];

// ─────────────────────────────────────────────────────────────────────────────
// PROTOTIPOS – MODELO
// ─────────────────────────────────────────────────────────────────────────────

// BCP
void inicializarProceso(Proceso *p, int index);

// Cola
void inicializarCola(Cola *c);
void liberarCola(Cola *c);
void encolar(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int estaVacia(Cola *c);
void insertarOrdenado(Cola *c, Proceso *p);

// SistemaES
void inicializarES(SistemaES *es);
int contarES(SistemaES *es);

// Memoria
void inicializarMemoria(void);
int usarRecurso(int index);
void liberarRecurso(int index);

// Sistema (tabla de procesos)
void inicializarSistema(void);
void cargarProcesosEnCola(Cola *procesosEnCiclo, Cola *nuevasSolicitudes);
void ingresarProcesosNuevos(Cola *procesosEnCiclo, int reloj);

// ── Socios ────────────────────────────────────────────────────────────────────
// Asigna socios al azar entre los TOTAL_PROCESOS BCPs
void asignarSocios(void);

// Notifica que p termino; si su socio tambien termino genera reporte
void notificarTerminacion(Proceso *p, pthread_mutex_t *mtx);

// ── NRU ───────────────────────────────────────────────────────────────────────
// Inicializa los marcos NRU de un proceso (todos invalidos)
void inicializarNRU(Proceso *p);

// Simula acceso a pagina virtual; si hay fallo ejecuta reemplazo NRU
void accederPaginaNRU(Proceso *p);

// Limpia bits R de todos los marcos (se llama periodicamente)
void limpiarBitsR(Proceso *p);

#endif