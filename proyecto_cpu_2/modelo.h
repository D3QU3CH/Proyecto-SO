#ifndef MODELO_H
#define MODELO_H

/* =====================================================================
 * modelo.h  –  Estructuras de datos del simulador
 *
 * Contiene TODO lo que es "dato":
 *   - BCP  (Bloque de Control de Proceso, 25 variables)
 *   - Nodo / Cola  (estructura enlazada)
 *   - SistemaES    (4 colas de E/S)
 *   - Memoria      (20 recursos compartidos tipo fruta)
 *   - Constantes globales del sistema
 *
 * COMPILAR:  gcc modelo.c controlador.c vista.c main.c -o simulador
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * CONSTANTES GLOBALES
 * ================================================================ */
#define TOTAL_PROCESOS   250   /* total de BCPs en la simulacion      */
#define EN_SISTEMA       150   /* inician en la cola de listos        */
                               /* los 100 restantes esperan en cola   */
#define TAM_MEM           20   /* recursos compartidos (frutas)       */

/* Multiplicadores de tiempo por dispositivo de E/S */
#define MULT_DISCO        2
#define MULT_PANTALLA     4
#define MULT_TECLADO      8
#define MULT_IMPRESORA   12

/* ================================================================
 * BCP – BLOQUE DE CONTROL DE PROCESO (25 variables)
 * ================================================================ */
typedef struct {

    /* --- 1-2  Identificacion --- */
    char id[10];            /* 1.  identificador unico  (ej. "A-0")  */
    char nombre[50];        /* 2.  nombre descriptivo                */

    /* --- 3-10  Tiempo y ciclos --- */
    int tiempoLlegada;      /* 3.  ciclo en que llega al sistema     */
    int ciclosTotales;      /* 4.  total de ciclos que necesita      */
    int ciclosRestantes;    /* 5.  ciclos pendientes por ejecutar    */
    int rafagaActual;       /* 6.  ciclos ejecutados en la iteracion */
    int tiempoEjecucion;    /* 7.  tiempo total acumulado en CPU     */
    int tiempoEspera;       /* 8.  tiempo acumulado en cola Listos   */
    int tiempoRespuesta;    /* 9.  ciclos hasta primera ejecucion    */
    int tiempoRetorno;      /* 10. ciclos hasta finalizacion         */

    /* --- 11  Estado ---
       0=listo  1=ejecutando  2=espera E/S
       3=terminado  4=bloqueado (seccion critica)                    */
    int estado;             /* 11. estado actual                     */

    /* --- 12-17  Planificacion --- */
    int vecesEnCPU;         /* 12. cuantas veces entro al CPU        */
    int iteraciones;        /* 13. veces procesado en el ciclo       */
    int restanteQuantum;    /* 14. quantum sobrante en RR            */
    int cambiosContexto;    /* 15. cantidad de cambios de contexto   */
    int esApropiativo;      /* 16. 1 = tiene prioridad               */
    int tipoProceso;        /* 17. 0=CPU-intensivo  1=ES-intensivo   */

    /* --- 18-19  Metricas de calidad --- */
    int aprovechamiento;    /* 18. % de uso del quantum asignado     */
    int desperdicio;        /* 19. ciclos desperdiciados del quantum  */

    /* --- 20-22  E/S --- */
    int dispositivoES;      /* 20. dispositivo asignado (-1=ninguno) */
    int tiempoES;           /* 21. ciclos restantes en E/S           */
    int bloqueado;          /* 22. 1 = en cola de E/S o SC           */

    /* --- 23-25  Seccion critica --- */
    int variable1;          /* 23. indice del recurso 1 asignado     */
    int variable2;          /* 24. indice del recurso 2 asignado     */
    int enSeccionCritica;   /* 25. 1 = usando recursos criticos      */

    /* Extra (no cuenta como BCP oficial) */
    int usoMemoria;         /* uso de memoria simulado (%)           */

} Proceso;

/* ================================================================
 * COLA ENLAZADA
 * ================================================================ */
typedef struct Nodo {
    Proceso     *proceso;
    struct Nodo *siguiente;
} Nodo;

typedef struct {
    Nodo *frente;
    Nodo *final;
    int   tamanio;
} Cola;

/* ================================================================
 * SISTEMA DE E/S (4 dispositivos)
 * ================================================================ */
typedef struct {
    Cola disco;
    Cola pantalla;
    Cola teclado;
    Cola impresora;
} SistemaES;

/* ================================================================
 * MEMORIA PRINCIPAL (frutas compartidas)
 * ================================================================ */
extern char memoria[TAM_MEM][20];    /* nombres de los recursos      */
extern int  recursoOcupado[TAM_MEM]; /* 0=libre  1=ocupado           */

/* ================================================================
 * TABLA GLOBAL DE BCPS
 * ================================================================ */
extern Proceso tablaProcesos[TOTAL_PROCESOS];

/* ================================================================
 * PROTOTIPOS – MODELO
 * ================================================================ */

/* BCP */
void     inicializarProceso(Proceso *p, int index);

/* Cola */
void     inicializarCola(Cola *c);
void     liberarCola(Cola *c);
void     encolar(Cola *c, Proceso *p);
Proceso *desencolar(Cola *c);
int      estaVacia(Cola *c);
void     insertarOrdenado(Cola *c, Proceso *p);

/* SistemaES */
void inicializarES(SistemaES *es);
int  contarES(SistemaES *es);

/* Memoria */
void inicializarMemoria(void);
int  usarRecurso(int index);
void liberarRecurso(int index);

/* Sistema (tabla de procesos) */
void inicializarSistema(void);
void cargarProcesosEnCola(Cola *procesosEnCiclo, Cola *nuevasSolicitudes);
void ingresarProcesosNuevos(Cola *procesosEnCiclo, int reloj);

#endif