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

#define TOTAL_PROCESOS 250  // total de BCPs en la simulacion
#define EN_SISTEMA     150  // entran directo al ciclo (listaProcesosEnEjecucion)
#define EN_ESPERA      100  // van a listaNuevasSolicitudes (250 - 150)

#define TAM_MEM 20          // recursos compartidos (frutas)

// Multiplicadores de tiempo por dispositivo de E/S
#define MULT_DISCO      2
#define MULT_PANTALLA   4
#define MULT_TECLADO    8
#define MULT_IMPRESORA  12

// ─── NRU ─────────────────────────────────────────────────────────────────────
#define NRU_NUM_MARCOS  8   // marcos de pagina simulados por proceso
#define NRU_NUM_PAGINAS 16  // paginas virtuales por proceso

// ─────────────────────────────────────────────────────────────────────────────
// PAGINA NRU (Not Recently Used)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    int numeroPagina;   // pagina virtual que ocupa este marco
    int bitR;           // referenciado en el intervalo actual
    int bitM;           // modificado (dirty)
    int valido;         // 1 = marco ocupado
} MarcoNRU;

// ─────────────────────────────────────────────────────────────────────────────
// BCP – BLOQUE DE CONTROL DE PROCESO (25 variables oficiales + extras)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    // ── Variables oficiales BCP (25) ─────────────────────────────────────────

    char id[10];        //  1. identificador unico (ej. "A-0")
    char nombre[50];    //  2. nombre descriptivo

    int tiempoLlegada;      //  3. ciclo en que llega al sistema
    int ciclosTotales;      //  4. total de ciclos que necesita (0-85000)
    int ciclosRestantes;    //  5. ciclos pendientes por ejecutar
    int rafagaActual;       //  6. ciclos ejecutados en la iteracion actual
    int tiempoEjecucion;    //  7. tiempo total acumulado en CPU
    int tiempoEspera;       //  8. tiempo acumulado esperando en cola Listos
    int tiempoRespuesta;    //  9. ciclos hasta primera vez en CPU
    int tiempoRetorno;      // 10. ciclos desde llegada hasta fin

    /* 11. Estado del proceso:
       0 = LISTO        (en cola de listos, esperando CPU)
       1 = EJECUTANDO   (usando CPU ahora mismo)
       2 = ESPERA_ES    (bloqueado en alguna cola de E/S)
       3 = TERMINADO    (completo, BCP se conserva en la lista)
       4 = BLOQUEADO_SC (esperando recursos de seccion critica)
    */
    int estado;

    int vecesEnCPU;         // 12. cuantas veces entro al CPU
    int iteraciones;        // 13. veces procesado en el ciclo principal
    int restanteQuantum;    // 14. quantum sobrante cuando rafaga > quantum (RR)
    int cambiosContexto;    // 15. cambios de contexto acumulados
    int esApropiativo;      // 16. 1 = tiene prioridad, va al frente de listos
    int tipoProceso;        // 17. 0=CPU-intensivo  1=ES-intensivo

    int aprovechamiento;    // 18. % del quantum que realmente uso
    int desperdicio;        // 19. ciclos de quantum desperdiciados acumulados

    int dispositivoES;      // 20. dispositivo asignado (-1=ninguno, 0-3=tipo)
    int tiempoES;           // 21. ciclos restantes en cola E/S
    int bloqueado;          // 22. 1 = en cola E/S o en seccion critica

    int variable1;          // 23. indice del primer recurso tomado (frutas)
    int variable2;          // 24. indice del segundo recurso tomado (frutas)
    int enSeccionCritica;   // 25. 1 = dentro de seccion critica ahora mismo

    // ── Extras (no cuentan como BCP oficial) ─────────────────────────────────

    int usoMemoria;         // uso de memoria simulado (%)

    // Indica en cual de las dos listas vive este proceso:
    // 0 = listaProcesosEnEjecucion   1 = listaNuevasSolicitudes
    int listaOrigen;

    // ── Socios (Buddy System logico entre procesos) ───────────────────────────
    int socioIndex;             // indice en tablaProcesos del socio (-1=sin socio)
    int socioTerminado;         // 1 = el socio ya termino
    int reporteSocioGenerado;   // 1 = ya se genero el reporte conjunto

    // ── NRU ──────────────────────────────────────────────────────────────────
    MarcoNRU marcosNRU[NRU_NUM_MARCOS]; // tabla de marcos del proceso
    int fallosPagina;           // contador de page faults
    int reemplazosNRU;          // veces que se ejecuto el algoritmo NRU

} Proceso;

// ─────────────────────────────────────────────────────────────────────────────
// COLA ENLAZADA (usada para: colaListos, colaES x4, colaTerminados)
// Los nodos guardan punteros al BCP real en tablaProcesos[].
// Desencolar elimina el nodo de la cola pero el BCP permanece intacto.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Nodo
{
    Proceso    *proceso;
    struct Nodo *siguiente;
} Nodo;

typedef struct
{
    Nodo *frente;
    Nodo *final;
    int   tamanio;
} Cola;

// ─────────────────────────────────────────────────────────────────────────────
// LISTAS FIJAS DE PROCESOS
//
// Estas son las dos listas permanentes que pide el enunciado.
// Son arreglos de punteros a Proceso: los procesos NUNCA se eliminan de aqui.
// El estado del proceso (campo `estado`) es lo que cambia, no su posicion.
//
//   listaProcesosEnEjecucion[EN_SISTEMA]  -> 150 procesos que arrancan en ciclo
//   listaNuevasSolicitudes[EN_ESPERA]     -> 100 procesos que esperan su turno
//
// El flujo correcto:
//   1. Proceso en lista con estado=0 -> se encola en colaListos
//   2. Planificador desencola de colaListos -> estado=1 (ejecutando)
//   3. Sale de CPU -> estado=2, entra a cola E/S (proceso sigue en su lista)
//   4. Sale de E/S -> estado=0, vuelve a colaListos
//   5. Termina del todo -> estado=3, NO se encola mas, BCP queda en la lista
//   6. nuevasSolicitudes ingresa a colaListos segun tiempoLlegada <= reloj
// ─────────────────────────────────────────────────────────────────────────────

extern Proceso *listaProcesosEnEjecucion[EN_SISTEMA];
extern Proceso *listaNuevasSolicitudes[EN_ESPERA];

// ─────────────────────────────────────────────────────────────────────────────
// SISTEMA DE E/S (4 colas, una por dispositivo)
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
    Cola      *colaListos;          // la unica cola dinamica del planificador
    SistemaES *es;
    int       *algoritmo;
    int       *quantum;
    int       *ciclos;
    int       *terminado;           // bandera de fin de simulacion

    // Mutex principal que protege colaListos y colas de E/S
    pthread_mutex_t mutexPrincipal;

    // Semaforos: uno por dispositivo E/S para despertar al hilo correspondiente
    sem_t *semDisco;
    sem_t *semPantalla;
    sem_t *semTeclado;
    sem_t *semImpresora;

    // Mutex exclusivo para el reporte de socios
    pthread_mutex_t mutexSocios;
} ContextoHilos;

// ─────────────────────────────────────────────────────────────────────────────
// MEMORIA PRINCIPAL (frutas = recursos compartidos de seccion critica)
// ─────────────────────────────────────────────────────────────────────────────

extern char memoria[TAM_MEM][20];
extern int  recursoOcupado[TAM_MEM];

// ─────────────────────────────────────────────────────────────────────────────
// TABLA GLOBAL DE BCPs (arreglo fijo, fuente de verdad de todos los procesos)
// ─────────────────────────────────────────────────────────────────────────────

extern Proceso tablaProcesos[TOTAL_PROCESOS];

// ─────────────────────────────────────────────────────────────────────────────
// PROTOTIPOS – MODELO
// ─────────────────────────────────────────────────────────────────────────────

// ── BCP ───────────────────────────────────────────────────────────────────────
void inicializarProceso(Proceso *p, int index);

// ── Cola ──────────────────────────────────────────────────────────────────────
void     inicializarCola(Cola *c);
void     liberarCola(Cola *c);
void     encolar(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int      estaVacia(Cola *c);
void     insertarOrdenado(Cola *c, Proceso *p);

// ── SistemaES ─────────────────────────────────────────────────────────────────
void inicializarES(SistemaES *es);
int  contarES(SistemaES *es);

// ── Memoria (frutas) ──────────────────────────────────────────────────────────
void inicializarMemoria(void);
int  usarRecurso(int index);
void liberarRecurso(int index);

// ── Sistema: inicializacion y carga de listas ─────────────────────────────────

// Crea los 250 BCPs, asigna IDs, tiempoLlegada y ciclosTotales
void inicializarSistema(void);

// Llena listaProcesosEnEjecucion[] y listaNuevasSolicitudes[] con punteros,
// y encola en colaListos los 150 que arrancan en estado LISTO
void cargarListas(Cola *colaListos);

// Revisa listaNuevasSolicitudes[] y encola en colaListos los que ya llegaron
void ingresarNuevosSegunReloj(Cola *colaListos, int reloj);

// ── Socios ────────────────────────────────────────────────────────────────────
void asignarSocios(void);
void notificarTerminacion(Proceso *p, pthread_mutex_t *mtx);

// ── NRU ───────────────────────────────────────────────────────────────────────
void inicializarNRU(Proceso *p);
void accederPaginaNRU(Proceso *p);
void limpiarBitsR(Proceso *p);

// ── Utilidades ────────────────────────────────────────────────────────────────

// Cuenta cuantos procesos en listaProcesosEnEjecucion NO han terminado
int contarActivosEnEjecucion(void);

// Cuenta cuantos procesos en listaNuevasSolicitudes NO han terminado ni ingresado
int contarPendientesNuevas(void);

#endif