#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "controlador.h"
#include "vista.h"

/* =====================================================================
 * controlador.c  –  Logica de negocio completa del simulador
 *
 * Fusion de: planificador.c + control.c + es.c (logica) +
 *            sistema.c (teclado) + log.c
 * ===================================================================== */

/* ================================================================
 * GLOBALES
 * ================================================================ */
Cola colaTerminados;
int  totalTerminados = 0;

/* ================================================================
 * SECCION 1 – E/S
 * ================================================================ */

void asignarTiempoES(Proceso *p, int tipo)
{
    int base = rand() % 100 + 1;
    int mult[4] = {MULT_DISCO, MULT_PANTALLA, MULT_TECLADO, MULT_IMPRESORA};
    const char *nombres[4] = {"Disco", "Pantalla", "Teclado", "Impresora"};

    p->tiempoES      = base * mult[tipo];
    p->dispositivoES = tipo;

    printf("  [E/S] %s -> %s | base=%d | total=%d ciclos\n",
           p->id, nombres[tipo], base, p->tiempoES);
}

/* Procesa UNA cola de E/S */
static void procesarColaES(Cola *colaES, Cola *colaListos)
{
    int size = colaES->tamanio;
    while (size--) {
        Proceso *p = desencolar(colaES);
        if (!p) continue;

        p->tiempoES--;

        if (p->tiempoES <= 0) {
            p->bloqueado     = 0;
            p->dispositivoES = -1;
            p->estado        = 0;
            printf("  [E/S] %s SALE de E/S -> Listos\n", p->id);

            if (p->esApropiativo)
                moverAlFrente(colaListos, p);
            else
                encolar(colaListos, p);
        } else {
            encolar(colaES, p);
        }
    }
}

void procesarES(SistemaES *es, Cola *colaListos)
{
    procesarColaES(&es->disco,     colaListos);
    procesarColaES(&es->pantalla,  colaListos);
    procesarColaES(&es->teclado,   colaListos);
    procesarColaES(&es->impresora, colaListos);
}

/* ================================================================
 * SECCION 2 – METRICAS DE COLA
 * ================================================================ */

void actualizarEspera(Cola *colaEnCiclo)
{
    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente)
        if (n->proceso->estado == 0)
            n->proceso->tiempoEspera++;
}

void evaluarColas(int *quantum, Cola *colaEnCiclo, SistemaES *es)
{
    int listos = 0;
    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente)
        if (n->proceso->estado == 0)
            listos++;

    int espera = contarES(es);
    int total  = listos + espera;
    if (total == 0) return;

    int pct = (listos * 100) / total;

    if (pct > 75) {
        *quantum += 5;
        printf("  [BALANCE] Aumenta quantum -> %d\n", *quantum);
    } else if (pct < 25) {
        if (*quantum > 5) {
            *quantum -= 5;
            printf("  [BALANCE] Disminuye quantum -> %d\n", *quantum);
        }
    } else {
        printf("  [BALANCE] Colas balanceadas (%d%% listos / %d%% E/S)\n",
               pct, 100 - pct);
    }
}

/* ================================================================
 * SECCION 3 – APROPIATIVIDAD
 * ================================================================ */

void moverAlFrente(Cola *cola, Proceso *p)
{
    if (cola->frente == NULL || p == NULL) return;
    if (cola->frente->proceso == p)        return;

    Nodo *anterior = NULL;
    Nodo *actual   = cola->frente;

    while (actual != NULL && actual->proceso != p) {
        anterior = actual;
        actual   = actual->siguiente;
    }
    if (actual == NULL) return;

    anterior->siguiente = actual->siguiente;
    if (actual == cola->final)
        cola->final = anterior;

    actual->siguiente = cola->frente;
    cola->frente      = actual;

    printf("  [OK] Proceso %s movido al frente.\n", p->id);
}

Proceso *seleccionarProcesoCritico(Cola *procesosEnCiclo)
{
    if (estaVacia(procesosEnCiclo)) {
        printf("  No hay procesos en la cola de listos.\n");
        return NULL;
    }

    /* Limpiar marcas anteriores */
    for (Nodo *n = procesosEnCiclo->frente; n != NULL; n = n->siguiente)
        n->proceso->esApropiativo = 0;

    /* TOP-5 por ciclosRestantes DESC */
    Proceso *top5[5] = {NULL};
    int encontrados  = 0;

    for (Nodo *n = procesosEnCiclo->frente; n != NULL; n = n->siguiente) {
        Proceso *p = n->proceso;

        int yaEsta = 0;
        for (int i = 0; i < encontrados; i++)
            if (top5[i] == p) { yaEsta = 1; break; }
        if (yaEsta) continue;

        if (encontrados < 5) {
            top5[encontrados++] = p;
            for (int i = encontrados - 1; i > 0; i--) {
                if (top5[i]->ciclosRestantes > top5[i-1]->ciclosRestantes) {
                    Proceso *tmp = top5[i]; top5[i] = top5[i-1]; top5[i-1] = tmp;
                } else break;
            }
        } else if (p->ciclosRestantes > top5[4]->ciclosRestantes) {
            top5[4] = p;
            for (int i = 4; i > 0; i--) {
                if (top5[i]->ciclosRestantes > top5[i-1]->ciclosRestantes) {
                    Proceso *tmp = top5[i]; top5[i] = top5[i-1]; top5[i-1] = tmp;
                } else break;
            }
        }
    }

    printf("\n  --- 5 PROCESOS MAS REZAGADOS ---\n");
    for (int i = 0; i < encontrados; i++)
        printf("  %d. ID: %-8s | Ciclos rest.: %6d | VecesEnCPU: %d\n",
               i + 1, top5[i]->id, top5[i]->ciclosRestantes, top5[i]->vecesEnCPU);

    char idElegido[20];
    printf("\n  Ingrese el ID del proceso a privilegiar: ");
    if (scanf("%19s", idElegido) != 1) return NULL;

    for (int i = 0; i < encontrados; i++) {
        if (strcmp(top5[i]->id, idElegido) == 0) {
            top5[i]->esApropiativo = 1;
            logEvento("Proceso privilegiado por apropiatividad");
            vistaMensajeProcesoCritico(top5[i]);
            return top5[i];
        }
    }

    printf("  ID no encontrado en la lista.\n");
    return NULL;
}

/* ================================================================
 * SECCION 4 – CONTROL AUTOMATICO DE ALGORITMO
 * ================================================================ */

int decidirCambio(Cola *colaListos, int algoritmoActual)
{
    if (estaVacia(colaListos)) return algoritmoActual;

    int esperaT = 0, desperdT = 0, ciclosT = 0;
    int rafagaT = 0, vecesT = 0, bloqT = 0, quantumT = 0;
    int total   = 0;

    for (Nodo *n = colaListos->frente; n != NULL; n = n->siguiente) {
        Proceso *p = n->proceso;
        esperaT  += p->tiempoEspera;
        desperdT += p->desperdicio;
        ciclosT  += p->ciclosRestantes;
        rafagaT  += p->rafagaActual;
        vecesT   += p->vecesEnCPU;
        bloqT    += p->bloqueado;
        quantumT += p->restanteQuantum;
        total++;
    }
    if (total == 0) return algoritmoActual;

    int pE = esperaT / total, pD = desperdT / total;
    int pC = ciclosT / total, pR = rafagaT  / total;
    int pV = vecesT  / total, pB = bloqT    / total;
    int pQ = quantumT / total;

    /* FCFS -> RR: mucha espera, rafagas largas, pocos ingresos al CPU */
    if (algoritmoActual == 1 &&
        pE > 120 && pD > 25 && pC > 8000 &&
        pR > 50  && pV < 5  && pB > 1    && pQ < 15)
    {
        printf("\n  [AUTO] FCFS -> RR | E:%d D:%d C:%d R:%d V:%d B:%d Q:%d\n",
               pE, pD, pC, pR, pV, pB, pQ);
        logEvento("Cambio automatico FCFS -> RR");
        vistaMensajeCambioAutomatico(1, 2);
        return 2;
    }

    /* RR -> FCFS: carga baja, sin bloqueos, procesos cortos */
    if (algoritmoActual == 2 &&
        pE < 80  && pD < 20 && pC < 6000 &&
        pR < 40  && pV >= 5 && pB == 0   && pQ > 5)
    {
        printf("\n  [AUTO] RR -> FCFS | E:%d D:%d C:%d R:%d V:%d B:%d Q:%d\n",
               pE, pD, pC, pR, pV, pB, pQ);
        logEvento("Cambio automatico RR -> FCFS");
        vistaMensajeCambioAutomatico(2, 1);
        return 1;
    }

    return algoritmoActual;
}

/* ================================================================
 * SECCION 5 – PLANIFICACION FCFS
 * ================================================================ */

void ejecutarFCFS(Cola *colaListos, SistemaES *es,
                  int *algoritmo, int reloj)
{
    (void)algoritmo;
    if (estaVacia(colaListos)) return;

    actualizarEspera(colaListos);

    /* Proceso privilegiado va al frente */
    for (Nodo *n = colaListos->frente; n != NULL; n = n->siguiente)
        if (n->proceso->esApropiativo) {
            moverAlFrente(colaListos, n->proceso);
            break;
        }

    Proceso *p = desencolar(colaListos);
    if (!p) return;

    p->estado = 1;
    p->vecesEnCPU++;
    p->iteraciones++;

    if (p->vecesEnCPU == 1)
        p->tiempoRespuesta = reloj - p->tiempoLlegada;

    int cc = rand() % 21 + 10;
    p->cambiosContexto++;
    usleep(cc * 1000);

    int rafaga = rand() % 61 + 10;
    if (rafaga > p->ciclosRestantes) rafaga = p->ciclosRestantes;

    p->rafagaActual     = rafaga;
    p->ciclosRestantes -= rafaga;
    p->tiempoEjecucion += rafaga;

    usleep(20000);

    printf("  [FCFS] %s | Rafaga: %d | Restantes: %d | VecesCPU: %d\n",
           p->id, rafaga, p->ciclosRestantes, p->vecesEnCPU);

    if (p->ciclosRestantes <= 0) {
        p->estado        = 3;
        p->tiempoRetorno = reloj - p->tiempoLlegada;
        totalTerminados++;
        encolar(&colaTerminados, p);
        printf("  [FCFS] %s TERMINO | Retorno: %d\n", p->id, p->tiempoRetorno);
        logEvento("Proceso terminado (FCFS)");
    } else {
        p->estado = 2;
        int tipo  = rand() % 4;
        asignarTiempoES(p, tipo);
        p->bloqueado = 1;

        if (tipo == 0) encolar(&es->disco,     p);
        if (tipo == 1) encolar(&es->pantalla,  p);
        if (tipo == 2) encolar(&es->teclado,   p);
        if (tipo == 3) encolar(&es->impresora, p);

        encolar(colaListos, p);
    }
}

/* ================================================================
 * SECCION 6 – PLANIFICACION ROUND ROBIN
 * ================================================================ */

void ejecutarRR(Cola *colaEnCiclo, Cola *nuevasSolicitudes,
                SistemaES *es, int *algoritmo,
                int *quantum, int reloj)
{
    static int contador = 0;
    if (estaVacia(colaEnCiclo)) return;

    actualizarEspera(colaEnCiclo);
    contador++;

    /* Intentar desbloquear procesos en seccion critica */
    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente) {
        Proceso *p = n->proceso;
        if (p->estado == 4) {
            int r1 = rand() % TAM_MEM;
            int r2 = rand() % TAM_MEM;
            while (r2 == r1) r2 = rand() % TAM_MEM;
            if (usarRecurso(r1) && usarRecurso(r2)) {
                p->estado = 0; p->bloqueado = 0; p->enSeccionCritica = 0;
                liberarRecurso(r1); liberarRecurso(r2);
            }
        }
    }

    /* Proceso privilegiado al frente */
    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente)
        if (n->proceso->esApropiativo) {
            moverAlFrente(colaEnCiclo, n->proceso);
            break;
        }

    /* Saltar bloqueados (estado==4) */
    Nodo *actual = colaEnCiclo->frente, *anterior = NULL;
    while (actual != NULL && actual->proceso->estado == 4) {
        anterior = actual;
        actual   = actual->siguiente;
    }

    if (actual == NULL) {
        vistaPushHistorial(0);
        vistaMostrarHistorialCPU();
        return;
    }

    if (actual != colaEnCiclo->frente) {
        anterior->siguiente = actual->siguiente;
        if (actual == colaEnCiclo->final)
            colaEnCiclo->final = anterior;
        actual->siguiente   = colaEnCiclo->frente;
        colaEnCiclo->frente = actual;
    }

    Proceso *p = desencolar(colaEnCiclo);
    if (!p) return;

    p->estado = 1;
    p->vecesEnCPU++;
    p->iteraciones++;

    if (p->vecesEnCPU == 1)
        p->tiempoRespuesta = reloj - p->tiempoLlegada;

    /* Seccion critica: tomar 2 recursos en orden para evitar livelock */
    int ok1 = 0, ok2 = 0, intentos = 0, r1 = -1, r2 = -1;
    while (intentos < 5) {
        int a = rand() % TAM_MEM;
        int b = rand() % TAM_MEM;
        while (b == a) b = rand() % TAM_MEM;
        r1 = (a < b) ? a : b;
        r2 = (a < b) ? b : a;
        ok1 = usarRecurso(r1);
        ok2 = ok1 ? usarRecurso(r2) : 0;
        if (ok1 && ok2) break;
        if (ok1) liberarRecurso(r1);
        if (ok2) liberarRecurso(r2);
        intentos++;
    }

    if (!(ok1 && ok2)) {
        p->estado = 4; p->bloqueado = 1; p->enSeccionCritica = 1;
        encolar(colaEnCiclo, p);
        vistaPushHistorial(0);
        vistaMostrarHistorialCPU();
        return;
    }

    p->variable1 = r1; p->variable2 = r2; p->enSeccionCritica = 1;
    printf("  [RR ] %s | Recurso: %s & %s\n",
           p->id, memoria[r1], memoria[r2]);

    int cc = rand() % 21 + 10;
    p->cambiosContexto++;
    usleep(cc * 1000);

    int rafaga  = rand() % 61 + 10;
    int ejecuta = rafaga;
    if (ejecuta > *quantum)           ejecuta = *quantum;
    if (ejecuta > p->ciclosRestantes) ejecuta = p->ciclosRestantes;

    p->rafagaActual     = ejecuta;
    p->ciclosRestantes -= ejecuta;
    p->tiempoEjecucion += ejecuta;
    p->tiempoRetorno    = reloj - p->tiempoLlegada;
    p->aprovechamiento  = (*quantum == 0) ? 0 : (ejecuta * 100) / *quantum;
    if (ejecuta < *quantum) p->desperdicio += (*quantum - ejecuta);

    usleep(20000);

    liberarRecurso(p->variable1); liberarRecurso(p->variable2);
    p->variable1 = -1; p->variable2 = -1; p->enSeccionCritica = 0;

    printf("  [RR ] %s | QUsado: %d/%d | Restantes: %d | Aprov: %d%%\n",
           p->id, ejecuta, *quantum, p->ciclosRestantes, p->aprovechamiento);

    if (p->ciclosRestantes <= 0) {
        p->restanteQuantum = 0;
        p->estado          = 3;
        totalTerminados++;
        if (p->esApropiativo) {
            printf("  [RR ] PROCESO PRIORITARIO %s FINALIZADO\n", p->id);
            logEvento("Proceso prioritario finalizado (RR)");
        }
        encolar(&colaTerminados, p);
        printf("  [RR ] %s TERMINO | Retorno: %d\n", p->id, p->tiempoRetorno);
        logEvento("Proceso terminado (RR)");
        vistaPushHistorial(p->aprovechamiento);
        vistaMostrarHistorialCPU();

    } else if (rafaga > *quantum) {
        p->estado          = 0;
        p->restanteQuantum = rafaga - ejecuta;
        encolar(colaEnCiclo, p);
        vistaPushHistorial(p->aprovechamiento);
        vistaMostrarHistorialCPU();

    } else {
        p->estado = 2;
        int tipo  = rand() % 4;
        asignarTiempoES(p, tipo);
        p->bloqueado = 1;
        if (tipo == 0) encolar(&es->disco,     p);
        if (tipo == 1) encolar(&es->pantalla,  p);
        if (tipo == 2) encolar(&es->teclado,   p);
        if (tipo == 3) encolar(&es->impresora, p);
        encolar(colaEnCiclo, p);
        vistaPushHistorial(p->aprovechamiento);
        vistaMostrarHistorialCPU();
    }

    /* Checkpoint cada 20 iteraciones RR */
    if (contador % 20 == 0) {
        evaluarColas(quantum, colaEnCiclo, es);
        vistaMostrarBalanceColas(colaEnCiclo, es);
        vistaMostrarEnvejecimiento(colaEnCiclo);
        vistaMostrarTopDesperdicio(colaEnCiclo);
        guardarTablaProcesos(colaEnCiclo, nuevasSolicitudes);
        guardarVariablesGlobales(colaEnCiclo, nuevasSolicitudes,
                                 *algoritmo, *quantum, contador, 0);
        logEvento("Checkpoint RR");
    }
}

/* ================================================================
 * SECCION 7 – TECLADO
 * ================================================================ */

static int hayTecla(void)
{
    struct termios oldt, newt;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    int c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (c != EOF) { ungetc(c, stdin); return 1; }
    return 0;
}

static char leerTecla(void)
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}

void manejarEntrada(Cola *colaListos, int *algoritmo,
                    int quantum, int terminados)
{
    if (!hayTecla()) return;
    char tecla = leerTecla();

    if (tecla == 'x' || tecla == 'X') {
        int opcion;
        printf("\n  --- CAMBIO DE ALGORITMO ---\n");
        printf("  1. FCFS\n  2. Round Robin\n  Seleccione: ");
        if (scanf("%d", &opcion) == 1 && (opcion == 1 || opcion == 2)) {
            *algoritmo = opcion;
            logEvento("Cambio manual de algoritmo");
            vistaMensajeAlgoritmo(opcion);
        } else {
            printf("  Opcion invalida.\n");
        }
        return;
    }

    if ((tecla == 'a' || tecla == 'A') && *algoritmo == 2) {
        Proceso *critico = seleccionarProcesoCritico(colaListos);
        if (critico) {
            moverAlFrente(colaListos, critico);
            logEvento("Proceso movido al frente por apropiatividad");
        }
        return;
    }

    if ((tecla == 'a' || tecla == 'A') && *algoritmo != 2) {
        printf("  La apropiatividad solo esta disponible en Round Robin.\n");
        return;
    }

    if (tecla == 'm' || tecla == 'M') {
        vistaMostrarMemoria();
        return;
    }

    if (tecla == 's' || tecla == 'S') {
        vistaMostrarEstadoSistema(colaListos, *algoritmo, quantum, terminados);
        return;
    }
}

/* ================================================================
 * SECCION 8 – LOGS / PERSISTENCIA
 * ================================================================ */

static const char *nombreEstado(int e)
{
    switch (e) {
        case 0: return "LISTO";
        case 1: return "EJECUTANDO";
        case 2: return "ESPERA_ES";
        case 3: return "TERMINADO";
        case 4: return "BLOQUEADO_SC";
        default: return "DESCONOCIDO";
    }
}

static const char *nombreDisp(int d)
{
    switch (d) {
        case 0: return "Disco";
        case 1: return "Pantalla";
        case 2: return "Teclado";
        case 3: return "Impresora";
        default: return "Ninguno";
    }
}

static void escribirBCP(FILE *f, Proceso *p, int num)
{
    fprintf(f, "\n===== BCP #%d =====\n", num);
    fprintf(f, " 1. ID:               %s\n",  p->id);
    fprintf(f, " 2. Nombre:           %s\n",  p->nombre);
    fprintf(f, " 3. TiempoLlegada:    %d\n",  p->tiempoLlegada);
    fprintf(f, " 4. CiclosTotales:    %d\n",  p->ciclosTotales);
    fprintf(f, " 5. CiclosRestantes:  %d\n",  p->ciclosRestantes);
    fprintf(f, " 6. RafagaActual:     %d\n",  p->rafagaActual);
    fprintf(f, " 7. TiempoEjecucion:  %d\n",  p->tiempoEjecucion);
    fprintf(f, " 8. TiempoEspera:     %d\n",  p->tiempoEspera);
    fprintf(f, " 9. TiempoRespuesta:  %d\n",  p->tiempoRespuesta);
    fprintf(f, "10. TiempoRetorno:    %d\n",  p->tiempoRetorno);
    fprintf(f, "11. Estado:           %s\n",  nombreEstado(p->estado));
    fprintf(f, "12. VecesEnCPU:       %d\n",  p->vecesEnCPU);
    fprintf(f, "13. Iteraciones:      %d\n",  p->iteraciones);
    fprintf(f, "14. RestanteQuantum:  %d\n",  p->restanteQuantum);
    fprintf(f, "15. CambiosContexto:  %d\n",  p->cambiosContexto);
    fprintf(f, "16. EsApropiativo:    %s\n",  p->esApropiativo ? "SI" : "NO");
    fprintf(f, "17. TipoProceso:      %s\n",
            p->tipoProceso == 0 ? "CPU-intensivo" : "ES-intensivo");
    fprintf(f, "18. Aprovechamiento:  %d%%\n", p->aprovechamiento);
    fprintf(f, "19. Desperdicio:      %d\n",  p->desperdicio);
    fprintf(f, "20. DispositivoES:    %s\n",  nombreDisp(p->dispositivoES));
    fprintf(f, "21. TiempoES:         %d\n",  p->tiempoES);
    fprintf(f, "22. Bloqueado:        %s\n",  p->bloqueado ? "SI" : "NO");
    fprintf(f, "23. Variable1:        %d\n",  p->variable1);
    fprintf(f, "24. Variable2:        %d\n",  p->variable2);
    fprintf(f, "25. EnSeccionCritica: %s\n",  p->enSeccionCritica ? "SI" : "NO");
}

void guardarTablaProcesos(Cola *colaEnCiclo, Cola *nuevasSolicitudes)
{
    FILE *f = fopen("tabla_procesos.log", "a");
    if (!f) return;
    time_t ahora = time(NULL);
    fprintf(f, "\n\n========================================\n");
    fprintf(f, " CHECKPOINT – %s", ctime(&ahora));
    fprintf(f, "========================================\n");

    int num = 1;
    fprintf(f, "\n-- Cola En Ciclo (%d) --\n", colaEnCiclo->tamanio);
    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente)
        escribirBCP(f, n->proceso, num++);

    fprintf(f, "\n-- Nuevas Solicitudes (%d) --\n", nuevasSolicitudes->tamanio);
    for (Nodo *n = nuevasSolicitudes->frente; n != NULL; n = n->siguiente)
        escribirBCP(f, n->proceso, num++);

    fclose(f);
}

void guardarVariablesGlobales(Cola *colaEnCiclo, Cola *nuevasSolicitudes,
                              int algoritmo, int quantum,
                              int iteracionCPU, int procesosNuevos)
{
    FILE *f = fopen("variables_globales.log", "a");
    if (!f) return;

    int sumE = 0, sumD = 0, sumC = 0, sumV = 0, cant = 0;
    for (Nodo *n = colaEnCiclo->frente; n != NULL; n = n->siguiente) {
        Proceso *p = n->proceso;
        sumE += p->tiempoEspera;  sumD += p->desperdicio;
        sumC += p->ciclosRestantes; sumV += p->vecesEnCPU;
        cant++;
    }
    int pE = cant ? sumE/cant : 0, pD = cant ? sumD/cant : 0;
    int pC = cant ? sumC/cant : 0, pV = cant ? sumV/cant : 0;

    time_t ahora = time(NULL);
    fprintf(f, "\n========================================\n");
    fprintf(f, " VARIABLES GLOBALES – %s", ctime(&ahora));
    fprintf(f, "========================================\n");
    fprintf(f, " 1. IteracionCPU:         %d\n",  iteracionCPU);
    fprintf(f, " 2. Algoritmo:            %s\n",  algoritmo == 1 ? "FCFS" : "RR");
    fprintf(f, " 3. Quantum:              %d\n",  quantum);
    fprintf(f, " 4. ProcesosEnCiclo:      %d\n",  colaEnCiclo->tamanio);
    fprintf(f, " 5. NuevasSolicitudes:    %d\n",  nuevasSolicitudes->tamanio);
    fprintf(f, " 6. ProcesosTerminados:   %d\n",  totalTerminados);
    fprintf(f, " 7. ProcesosNuevos:       %d\n",  procesosNuevos);
    fprintf(f, " 8. PromedioEspera:       %d\n",  pE);
    fprintf(f, " 9. PromedioDesperdicio:  %d\n",  pD);
    fprintf(f, "10. PromedioCiclosRest.:  %d\n",  pC);
    fprintf(f, "11. PromedioVecesEnCPU:   %d\n",  pV);
    fprintf(f, "12. TamColaEnCiclo:       %d\n",  colaEnCiclo->tamanio);
    fprintf(f, "13. TamNuevasSolicitudes: %d\n",  nuevasSolicitudes->tamanio);
    fprintf(f, "14. TamColaTerminados:    %d\n",  totalTerminados);
    fprintf(f, "15. SumaEsperaTotal:      %d\n",  sumE);
    fprintf(f, "16. SumaDesperdicioTotal: %d\n",  sumD);
    fprintf(f, "17. SumaCiclosRestantes:  %d\n",  sumC);
    fprintf(f, "18. CantProcesosActivos:  %d\n",  cant);
    fprintf(f, "19. AlgoritmoAnterior:    %s\n",  algoritmo == 1 ? "RR"   : "FCFS");
    fprintf(f, "20. AlgoritmoActual:      %s\n",  algoritmo == 1 ? "FCFS" : "RR");

    fclose(f);
}

void logEvento(const char *msg)
{
    FILE *f = fopen("eventos.log", "a");
    if (!f) return;
    time_t ahora = time(NULL);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&ahora));
    fprintf(f, "[%s] %s\n", buf, msg);
    fclose(f);
}