#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "modelo.h"
#include "controlador.h"
#include "vista.h"

extern volatile sig_atomic_t g_salir;

int main(void)
{
    srand((unsigned)time(NULL));

    termGuardar();
    signal(SIGINT,  manejadorSenal);
    signal(SIGTERM, manejadorSenal);
    atexit(termRestaurar);

    inicializarBuddy();
    inicializarTablaSistema();

    Lista enEjecucion, solicitudes;
    inicializarLista(&enEjecucion);
    inicializarLista(&solicitudes);

    cargarPalabras("libro1.txt");
    cargarFrases("frases.txt");
    inicializarPaginacion();

    poblarListas(&enEjecucion, &solicitudes);

    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        int idx = asignarMemoriaBuddy(p, p->memoriaUsadaKB);
        if (idx >= 0) {
            p->idxsBuddy[0]    = idx;
            p->numBloquesBuddy = 1;
        }
        asignarPaginasProceso(p);
        p->yaIngresado = 1;
    }

    Cola colaListos;
    inicializarCola(&colaListos);
    for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (p->tiempoLlegada == 0) {
            encolar(&colaListos, p);
            p->estado = ESTADO_LISTO;
        }
    }

    SistemaES es;
    inicializarSistemaES(&es);

    int reloj             = 0;
    int algoritmo         = ALG_FCFS;
    int quantum           = 40;
    int procesoPrivilId   = -1;
    int cicloUltimoCambio = -300;
    int iteracionesRR     = 0;
    volatile int terminado = 0;

    static int histDesp[100];
    static int histCiclo[100];
    int histIdx = 0;
    memset(histDesp,  0, sizeof(histDesp));
    memset(histCiclo, 0, sizeof(histCiclo));

    tablaSistema.algoritmoActual = ALG_FCFS;
    tablaSistema.quantumActual   = quantum;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    vistaBienvenida();
    termRaw();

    while (!terminado && !g_salir) {

        char tecla = leerTecla();
        if (tecla != 0) {
            if (tecla == 'q' || tecla == 'Q') {
                printf("\n[Q] Saliendo...\n");
                break;
            }
            if (tecla == 'x' || tecla == 'X')
                manejarTeclaX(&algoritmo, &quantum, reloj,
                              &cicloUltimoCambio, &mutex);
            if (tecla == 'a' || tecla == 'A')
                manejarTeclaA(&colaListos, &es, &procesoPrivilId, &mutex);
        }

        pthread_mutex_lock(&mutex);
        reloj++;
        tablaSistema.cicloActual = reloj;

        resetarBitsR(reloj);

        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente) {
            Proceso *p = n->proceso;
            if (!p->yaIngresado && p->tiempoLlegada <= reloj) {
                if (p->esApropiativo) encolarAlFrente(&colaListos, p);
                else                  encolar(&colaListos, p);
                p->estado      = ESTADO_LISTO;
                p->yaIngresado = 1;
            }
        }

        ingresarProcesosNuevos(&solicitudes, &colaListos, reloj);
        actualizarEspera(&colaListos);

        procesarColaES(&es.disco,     &colaListos);
        procesarColaES(&es.pantalla,  &colaListos);
        procesarColaES(&es.teclado,   &colaListos);
        procesarColaES(&es.impresora, &colaListos);

        if (reloj % 300 == 0)
            redimensionarMemoriaPrincipal(&enEjecucion, reloj);

        if (reloj % 200 == 0 && (reloj - cicloUltimoCambio) > 800) {
            int nuevo = evaluarCambioAlgoritmo(&colaListos, &es);
            if (nuevo != algoritmo) {
                algoritmo                    = nuevo;
                tablaSistema.algoritmoActual = nuevo;
                if (nuevo == ALG_RR && quantum <= 0) {
                    quantum = 20;
                    tablaSistema.quantumActual = 20;
                }
            }
        }

        if (!estaVaciaCola(&colaListos)) {
            if (algoritmo == ALG_FCFS)
                ejecutarFCFS(&colaListos, &es, &procesoPrivilId, reloj);
            else
                ejecutarRR(&colaListos, &es, &procesoPrivilId,
                           &quantum, &iteracionesRR,
                           histDesp, histCiclo, &histIdx, reloj);
        }

        calcularDesperdicioExterno();
        actualizarPromedioFinalizados(reloj);
        actualizarVariablesGlobales(&enEjecucion, &solicitudes,
                                    &colaListos, &es, reloj);

        if (reloj % 100 == 0) {
            vistaMostrarTablaGlobal();
            vistaEstadoES(&es);
            mostrarEstadisticasMemoria();
            if (algoritmo == ALG_RR) {
                vistaBarrasAprovechamiento(&colaListos,
                                           histDesp, histCiclo, histIdx);
                mostrarEnvejecimiento(&colaListos);
                mostrarDesperdiciadores(&colaListos);
            }
            guardarBCPs(&enEjecucion, "bcps.log");
            guardarVariablesGlobales("variables.log");
        }

        int activos = 0;
        for (Nodo *n = enEjecucion.cabeza; n; n = n->siguiente)
            if (n->proceso->estado != ESTADO_TERMINADO) activos++;
        if (activos == 0 && solicitudes.tamanio == 0 && estaVaciaCola(&colaListos))
            terminado = 1;

        pthread_mutex_unlock(&mutex);
        usleep(500);
    }

    termRestaurar();
    pthread_mutex_destroy(&mutex);

    guardarBCPs(&enEjecucion, "bcps.log");
    guardarVariablesGlobales("variables.log");
    mostrarEstadisticasMemoria();
    vistaCierre(reloj, tablaSistema.procesosTerminados);
    return 0;
}