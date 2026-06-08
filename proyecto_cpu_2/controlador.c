#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include "controlador.h"
#include "vista.h"

#define TICKS_ES 10

// TERMINAL

static struct termios g_termOrig;
static int g_termGuardado = 0;
volatile sig_atomic_t g_salir = 0;

void termGuardar(void)
{
    tcgetattr(STDIN_FILENO, &g_termOrig);
    g_termGuardado = 1;
}

void termRestaurar(void)
{
    if (!g_termGuardado)
        return;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termOrig);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
}

void termRaw(void)
{
    struct termios t = g_termOrig;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f | O_NONBLOCK);
}

void termBloqueo(void)
{
    int f = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, f & ~O_NONBLOCK);
    struct termios t = g_termOrig;
    t.c_lflag |= (ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

char leerTecla(void)
{
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;
    return 0;
}

int leerLinea(char *buf, int maxlen)
{
    termBloqueo();
    fflush(stdout);
    int len = 0;
    while (len < maxlen - 1)
    {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0)
            break;
        if (c == '\n' || c == '\r')
            break;
        if ((c == 127 || c == '\b') && len > 0)
        {
            len--;
            write(STDOUT_FILENO, "\b \b", 3);
            continue;
        }
        buf[len++] = c;
        write(STDOUT_FILENO, &c, 1);
    }
    buf[len] = '\0';
    write(STDOUT_FILENO, "\n", 1);
    termRaw();
    return len;
}

void manejadorSenal(int sig)
{
    (void)sig;
    g_salir = 1;
    termRestaurar();
}

// TECLAS DE CONTROL

void manejarTeclaX(int *algoritmo, int *quantum, int reloj,
                   int *cicloUltimoCambio, pthread_mutex_t *mutex)
{
    char buf[64] = {0};
    if (*algoritmo == ALG_FCFS)
    {
        printf("\n[X] Cambiar a Round Robin\n    Ingrese Quantum (>0): ");
        fflush(stdout);
        leerLinea(buf, sizeof(buf));
        int q = atoi(buf);
        if (q <= 0)
            q = 20;

        pthread_mutex_lock(mutex);
        *algoritmo = ALG_RR;
        *quantum = q;
        tablaSistema.algoritmoActual = ALG_RR;
        tablaSistema.quantumActual = q;
        *cicloUltimoCambio = reloj;
        pthread_mutex_unlock(mutex);
        printf("[X] Algoritmo -> Round Robin (Q=%d)\n", q);
    }
    else
    {
        printf("\n[X] Cambiar a FCFS? (s/n): ");
        fflush(stdout);
        leerLinea(buf, sizeof(buf));
        if (buf[0] == 's' || buf[0] == 'S' || buf[0] == 'y' || buf[0] == 'Y')
        {
            pthread_mutex_lock(mutex);
            *algoritmo = ALG_FCFS;
            tablaSistema.algoritmoActual = ALG_FCFS;
            *cicloUltimoCambio = reloj;
            pthread_mutex_unlock(mutex);
            printf("[X] Algoritmo -> FCFS\n");
        }
        else
        {
            printf("[X] Cancelado. Sigue en RR (Q=%d)\n", *quantum);
        }
    }
    {
        char tmp;
        while (read(STDIN_FILENO, &tmp, 1) == 1)
            ;
    }
}

void manejarTeclaA(Cola *colaListos, SistemaES *es,
                   int *procesoPrivilId, pthread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
    vistaMostrarMasRezagados(colaListos);
    pthread_mutex_unlock(mutex);

    char idBuf[32] = {0};
    printf("    Ingrese ID del proceso a privilegiar (ej: A-0): ");
    fflush(stdout);
    leerLinea(idBuf, sizeof(idBuf));

    pthread_mutex_lock(mutex);
    int encontrado = 0;
    for (int i = 0; i < TOTAL_PROCESOS; i++)
    {
        Proceso *p = &tablaSistema.tablaBCPs[i];
        if (strcmp(p->id, idBuf) == 0 && p->estado != ESTADO_TERMINADO)
        {
            if (*procesoPrivilId >= 0 && *procesoPrivilId != i)
                tablaSistema.tablaBCPs[*procesoPrivilId].esApropiativo = 0;
            p->esApropiativo = 1;
            *procesoPrivilId = i;
            moverAlFrenteCola(colaListos, p);
            moverAlFrenteCola(&es->disco, p);
            moverAlFrenteCola(&es->pantalla, p);
            moverAlFrenteCola(&es->teclado, p);
            moverAlFrenteCola(&es->impresora, p);
            printf("[A] %s ahora es apropiativo (al frente de listos y E/S)\n", p->id);
            encontrado = 1;
            break;
        }
    }
    if (!encontrado)
        printf("[A] Proceso '%s' no encontrado o ya termino\n", idBuf);
    pthread_mutex_unlock(mutex);
    {
        char tmp;
        while (read(STDIN_FILENO, &tmp, 1) == 1)
            ;
    }
}

// SCHEDULING

static void ponerApropiativoAlFrente(Cola *colaListos)
{
    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
    {
        if (n->proceso->esApropiativo)
        {
            moverAlFrenteCola(colaListos, n->proceso);
            break;
        }
    }
}

static void postEjecucion(Proceso *p, Cola *colaListos, SistemaES *es,
                          int *procesoPrivilId, int reloj, int ciclosEjecutados)
{
    if (p->ciclosRestantes <= 0)
    {
        if (p->esApropiativo)
            *procesoPrivilId = -1;
        procesarTerminacion(p, reloj);
    }
    else if (p->ciclosEnEjecucion >= CICLOS_PARA_ES)
    {
        asignarES(p, es, reloj);
    }
    else
    {
        p->estado = ESTADO_LISTO;
        if (p->esApropiativo)
            encolarAlFrente(colaListos, p);
        else
            encolar(colaListos, p);
    }
}

void ejecutarFCFS(Cola *colaListos, SistemaES *es, int *procesoPrivilId, int reloj)
{
    if (estaVaciaCola(colaListos))
        return;

    ponerApropiativoAlFrente(colaListos);
    Proceso *p = desencolar(colaListos);
    if (!p)
        return;

    if (p->ciclosRestantes <= 0)
    {
        if (p->esApropiativo)
            *procesoPrivilId = -1;
        procesarTerminacion(p, reloj);
        tablaSistema.totalCambiosContexto++;
        return;
    }

    procesarEntradaCPU(p, reloj);
    tablaSistema.totalCambiosContexto++;

    postEjecucion(p, colaListos, es, procesoPrivilId, reloj, p->rafagaActual);
}

void ejecutarRR(Cola *colaListos, SistemaES *es, int *procesoPrivilId,
                int *quantum, int *iteracionesRR,
                int histDesp[], int histCiclo[], int *histIdx, int reloj)
{
    if (estaVaciaCola(colaListos))
        return;

    ponerApropiativoAlFrente(colaListos);
    Proceso *p = desencolar(colaListos);
    if (!p)
        return;

    if (p->ciclosRestantes <= 0)
    {
        if (p->esApropiativo)
            *procesoPrivilId = -1;
        procesarTerminacion(p, reloj);
        tablaSistema.totalCambiosContexto++;
        return;
    }

    procesarEntradaCPU(p, reloj);
    tablaSistema.totalCambiosContexto++;
    (*iteracionesRR)++;

    int q = tablaSistema.quantumActual;
    int ejecuta, desp;

    if (p->rafagaActual > q)
    {
        int sobrante = p->rafagaActual - q;
        p->ciclosRestantes += sobrante;
        p->tiempoEjecucion -= sobrante;
        p->ciclosEnEjecucion -= sobrante;
        ejecuta = q;
        desp = 0;
    }
    else
    {
        ejecuta = p->rafagaActual;
        desp = q - ejecuta;
    }

    p->aprovechamiento = (q > 0) ? (ejecuta * 100) / q : 0;
    p->desperdicio = desp;

    histDesp[*histIdx] = (q > 0) ? (desp * 100) / q : 0;
    histCiclo[*histIdx] = reloj;
    *histIdx = (*histIdx + 1) % 100;

    ajustarQuantumAutomatico(colaListos, es, *iteracionesRR);
    *quantum = tablaSistema.quantumActual;

    postEjecucion(p, colaListos, es, procesoPrivilId, reloj, ejecuta);
}

// COLA E/S

void procesarColaES(Cola *colaES, Cola *colaListos)
{
    int n = colaES->tamanio;
    while (n--)
    {
        Proceso *p = desencolar(colaES);
        if (!p)
            continue;

        p->tiempoES -= TICKS_ES;

        if (p->tiempoES <= 0)
        {
            p->tiempoES = 0;
            p->dispositivoES = -1;
            p->estado = ESTADO_LISTO;
            if (p->esApropiativo)
                encolarAlFrente(colaListos, p);
            else
                encolar(colaListos, p);
        }
        else
        {
            encolar(colaES, p);
        }
    }
}

// AJUSTE DE QUANTUM

void ajustarQuantumAutomatico(Cola *colaListos, SistemaES *es, int iteracionesRR)
{
    if (iteracionesRR % 20 != 0)
        return;

    int enListos = colaListos->tamanio;
    int enES = es->disco.tamanio + es->pantalla.tamanio +
               es->teclado.tamanio + es->impresora.tamanio;
    int total = enListos + enES;
    if (total == 0)
        return;

    float propListos = (float)enListos / total;
    float propES = (float)enES / total;
    int q = tablaSistema.quantumActual;

    static int ciclosSinReporte = 0;

    if (propListos > 0.75f)
    {
        if (q > 20)
        {
            q -= 5;
            printf("[RR] Desbalance listos>75%% -> Q reducido a %d\n", q);
            ciclosSinReporte = 0;
        }
        else if (ciclosSinReporte == 0)
        {
            printf("[RR] Q en minimo (%d). Sistema con carga alta estructural.\n", q);
            ciclosSinReporte = 5;
        }
        else
        {
            ciclosSinReporte--;
        }
    }
    else if (propES > 0.75f)
    {
        if (q < 100)
        {
            q += 5;
            printf("[RR] Desbalance ES>75%% -> Q aumentado a %d\n", q);
        }
        ciclosSinReporte = 0;
    }
    else
    {
        printf("[RR] *** ALERTA: Colas balanceadas (listos=%.0f%% ES=%.0f%%) Q=%d ***\n",
               propListos * 100, propES * 100, q);
        ciclosSinReporte = 0;
    }

    tablaSistema.quantumActual = q;
}

// REPORTES

void mostrarEnvejecimiento(Cola *colaListos)
{
    Proceso *top[5] = {NULL};

    printf("\nTOP 5 ENVEJECIMIENTO (mas iteraciones en CPU)\n");

    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
    {
        Proceso *p = n->proceso;
        for (int i = 0; i < 5; i++)
        {
            if (!top[i] || p->vecesEnCPU > top[i]->vecesEnCPU)
            {
                for (int j = 4; j > i; j--)
                    top[j] = top[j - 1];
                top[i] = p;
                break;
            }
        }
    }

    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | vecesEnCPU: %3d | ciclosRest: %5d | espera: %d\n",
               i + 1, top[i]->id, top[i]->vecesEnCPU,
               top[i]->ciclosRestantes, top[i]->tiempoEspera);

    if (!top[0])
        printf("  (cola vacia)\n");
}

void mostrarDesperdiciadores(Cola *colaListos)
{
    Proceso *top[5] = {NULL};

    printf("\nTOP 5 DESPERDICIADORES DE CPU\n");

    for (NodoCola *n = colaListos->frente; n; n = n->siguiente)
    {
        Proceso *p = n->proceso;
        if (p->vecesEnCPU == 0)
            continue;

        int desp = p->desperdicio;
        for (int i = 0; i < 5; i++)
        {
            if (!top[i])
            {
                top[i] = p;
                break;
            }
            if (desp > top[i]->desperdicio)
            {
                for (int j = 4; j > i; j--)
                    top[j] = top[j - 1];
                top[i] = p;
                break;
            }
        }
    }

    int q = tablaSistema.quantumActual;
    for (int i = 0; i < 5 && top[i]; i++)
        printf("  %d. %-8s | rafaga: %3d | Q: %3d | desp: %3d | aprov: %3d%%\n",
               i + 1, top[i]->id, top[i]->rafagaActual,
               q, top[i]->desperdicio, top[i]->aprovechamiento);

    if (!top[0])
        printf("  (cola vacia)\n");
}

// PERSISTENCIA

void guardarBCPs(Lista *enEjecucion, const char *ruta)
{
    FILE *f = fopen(ruta, "w");
    if (!f)
        return;

    time_t ahora = time(NULL);
    fprintf(f, "=== CHECKPOINT %s", ctime(&ahora));

    // Encabezado
    fprintf(f, "%-5s %-10s %-22s %-10s %-9s %-9s %-9s %-9s %-9s %-9s %-11s %-9s %-9s %-7s %-8s %-8s %-8s %-7s %-7s %-8s %-9s %-8s %-7s %-7s %-8s\n",
            "#", "ID", "Nombre", "Estado", "Llegada", "CicTot", "CicRest",
            "Rafaga", "TEjec", "TEspera", "TRespuesta", "TRetorno",
            "VecCPU", "Iter", "ResQ", "CambCtx", "Aprov%", "Desp",
            "DisES", "TiemES", "FallosPg", "MemKB", "Var1", "Var2", "Tipo");

    fprintf(f, "%-5s %-10s %-22s %-10s %-9s %-9s %-9s %-9s %-9s %-9s %-11s %-9s %-9s %-7s %-8s %-8s %-8s %-7s %-7s %-8s %-9s %-8s %-7s %-7s %-8s\n",
            "-----", "----------", "----------------------", "----------", "--------", "--------", "--------",
            "--------", "--------", "--------", "----------", "--------", "--------", "------", "-------",
            "-------", "-------", "------", "------", "-------", "--------", "-------", "------", "------", "-------");

    const char *estados[] = {"LISTO", "EJEC", "ESP_ES", "TERMINADO"};

    // Iterar los 250 procesos directamente de la tabla
    for (int i = 0; i < TOTAL_PROCESOS; i++)
    {
        Proceso *p = &tablaSistema.tablaBCPs[i];
        const char *est = (p->estado >= 0 && p->estado <= 3) ? estados[p->estado] : "?";
        const char *tipo = p->tipoProceso == 0 ? "CPU" : "IO";

        fprintf(f, "%-5d %-10s %-22s %-10s %-9d %-9d %-9d %-9d %-9d %-9d %-11d %-9d %-9d %-7d %-8d %-8d %-8d %-7d %-7d %-8d %-9d %-8d %-7d %-7d %-8s\n",
                i + 1, p->id, p->nombre, est,
                p->tiempoLlegada, p->ciclosTotales, p->ciclosRestantes,
                p->rafagaActual, p->tiempoEjecucion, p->tiempoEspera,
                p->tiempoRespuesta, p->tiempoRetorno,
                p->vecesEnCPU, p->iteraciones, p->restanteQuantum,
                p->cambiosContexto, p->aprovechamiento, p->desperdicio,
                p->dispositivoES, p->tiempoES, p->fallosPagina,
                p->memoriaUsadaKB, p->variable1, p->variable2, tipo);
    }
    fclose(f);
}

void guardarVariablesGlobales(const char *ruta)
{
    FILE *f = fopen(ruta, "w");
    if (!f)
        return;

    TablaProcesos *t = &tablaSistema;
    time_t ahora = time(NULL);
    fprintf(f, "\n=== VARIABLES GLOBALES %s", ctime(&ahora));
    fprintf(f, " 1.  Total            : %d\n", t->totalProcesos);
    fprintf(f, " 2.  En ciclo         : %d\n", t->procesosEnCiclo);
    fprintf(f, " 3.  En solicitudes   : %d\n", t->procesosEnSolicitud);
    fprintf(f, " 4.  Cola listos      : %d\n", t->procesosEnColaListos);
    fprintf(f, " 5.  Ejecutando       : %d\n", t->procesosEjecutando);
    fprintf(f, " 6.  En E/S           : %d\n", t->procesosEnES);
    fprintf(f, " 7.  Terminados       : %d\n", t->procesosTerminados);
    fprintf(f, " 8.  Bloqueados       : %d\n", t->procesosBloqueados);
    fprintf(f, " 9.  Algoritmo        : %s\n", t->algoritmoActual == ALG_FCFS ? "FCFS" : "RR");
    fprintf(f, "10.  Quantum          : %d\n", t->quantumActual);
    fprintf(f, "11.  Ciclo actual     : %d\n", t->cicloActual);
    fprintf(f, "12.  Cambios ctx      : %d\n", t->totalCambiosContexto);
    fprintf(f, "13.  Fallos pagina    : %d\n", t->totalFallosPagina);
    fprintf(f, "14.  Suma espera      : %d\n", t->sumaEspera);
    fprintf(f, "15.  Suma ciclos      : %d\n", t->sumaCiclosRestantes);
    fprintf(f, "16.  Prom. espera     : %d\n", t->promedioEspera);
    fprintf(f, "17.  Prom. ciclos     : %d\n", t->promedioCiclos);
    fprintf(f, "18.  Ingr. dinamico   : %d\n", t->procesosIngresadosDinam);
    fprintf(f, "19.  Mem. libre (KB)  : %d\n", t->memoriaLibreKB);
    fprintf(f, "20.  Desperdicio (KB) : %d\n", t->desperdicioTotal);
    fclose(f);
}