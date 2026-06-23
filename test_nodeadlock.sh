#!/bin/bash

# ==============================================================================
# SCRIPT DE PRUEBA: HAPPY PATH (ORQUESTACIÓN GRANULAR)
# No modifica el código fuente. Usa las funciones base de Erlang.
# ==============================================================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0;0m'

echo -e "${BLUE}====================================================================${NC}"
echo -e "${BLUE} INICIANDO TEST HAPPY PATH (Sin carga aleatoria)                    ${NC}"
echo -e "${BLUE}====================================================================${NC}"

PORT_A=12000
PORT_B=12001
TIMEOUT_LIMIT=30

# --- LIMPIEZA INICIAL ---
echo -e "${YELLOW}[*] Limpiando entorno y liberando puertos...${NC}"
killall -9 agente_c_1 agente_c_2 erl beam.smp 2>/dev/null || true
fuser -k -n tcp $PORT_A 2>/dev/null || true
fuser -k -n tcp $PORT_B 2>/dev/null || true
sleep 2
lsof -ti tcp:${PORT_A} | xargs kill -9 2>/dev/null || true
lsof -ti tcp:${PORT_B} | xargs kill -9 2>/dev/null || true
rm -f logErl.txt log_erl_*.txt log_c_*.txt
sleep 1

# --- COMPILAR LIBRERÍA DE INTERCEPTACIÓN (si no existe) ---
if [ ! -f fix_bind.so ]; then
    echo -e "${YELLOW}[*] Compilando fix_bind.so para forzar IP de origen...${NC}"
    cat > fix_bind.c << 'EOF'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    static int (*real_connect)(int, const struct sockaddr*, socklen_t) = NULL;
    if (!real_connect) real_connect = dlsym(RTLD_NEXT, "connect");

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        char *ip = inet_ntoa(sin->sin_addr);
        if (strncmp(ip, "127.", 4) == 0) {
            char *src_ip = getenv("BIND_SRC_IP");
            if (src_ip) {
                struct sockaddr_in local_addr;
                memset(&local_addr, 0, sizeof(local_addr));
                local_addr.sin_family = AF_INET;
                local_addr.sin_port = 0;
                inet_aton(src_ip, &local_addr.sin_addr);
                bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr));
            }
        }
    }
    return real_connect(sockfd, addr, addrlen);
}
EOF
    gcc -shared -fPIC -o fix_bind.so fix_bind.c -ldl || {
        echo -e "${RED}[FAIL] No se pudo compilar fix_bind.so${NC}"
        exit 1
    }
    echo -e "${GREEN}[OK] fix_bind.so compilado.${NC}"
fi

# --- DESPLIEGUE DE RED ---
echo -e "\n${BLUE}[Fase 1] Lanzando la topología de red...${NC}"

echo -e "[*] Levantando Agente C - Nodo A (127.0.0.2:${PORT_A})..."
BIND_SRC_IP=127.0.0.2 LD_PRELOAD=./fix_bind.so stdbuf -oL \
    ./server 127.0.0.2 ${PORT_A} 127.255.255.255 127.0.0.1 > log_c_1.txt 2>&1 &
PID_C1=$!

echo -e "[*] Levantando Agente C - Nodo B (127.0.0.3:${PORT_B})..."
BIND_SRC_IP=127.0.0.3 LD_PRELOAD=./fix_bind.so stdbuf -oL \
    ./server 127.0.0.3 ${PORT_B} 127.255.255.255 127.0.0.1 > log_c_2.txt 2>&1 &
PID_C2=$!

echo "[*] Esperando inicialización de los agentes..."
sleep 3

kill -0 $PID_C1 || { echo "Nodo A murió"; exit 1; }
kill -0 $PID_C2 || { echo "Nodo B murió"; exit 1; }

echo "================ TABLA DE PUERTOS ================="
ss -lntp | grep -E "12000|12001"
echo "==================================================="

# --- INYECCIÓN DE CARGA CONTROLADA ---
echo -e "\n${BLUE}[Fase 2] Orquestando funciones Erlang para Happy Path...${NC}"

# Lanzamos el primer Erlang (A) que pide cpu a B (127.0.0.3)
erl -sname nodoA -noshell -eval "
    logF:crear_error_managment(),
    send_recv_manager:send_recv_init(${PORT_A}),
    job_server:iniciar_job_server(),
    job_server:crear_job(\"@127.0.0.3:cpu:2\"),
    timer:sleep(20000),
    init:stop().
" > log_erl_A.txt 2>&1 &
PID_ERL_A=$!

# Pequeña pausa para que el primer Erlang genere un JobId diferente
sleep 1

# Segundo Erlang (B) pide mem a A (127.0.0.2) para evitar el recurso inexistente GPU
erl -sname nodoB -noshell -eval "
    logF:crear_error_managment(),
    send_recv_manager:send_recv_init(${PORT_B}),
    job_server:iniciar_job_server(),
    job_server:crear_job(\"@127.0.0.2:mem:1024\"),
    timer:sleep(20000),
    init:stop().
" > log_erl_B.txt 2>&1 &
PID_ERL_B=$!

# --- MONITOREO ---
echo -e "\n[Fase 3] Monitoreando ejecución (Límite: ${TIMEOUT_LIMIT}s)..."
TIEMPO=0
DEADLOCK=false

while [ $TIEMPO -lt $TIMEOUT_LIMIT ]; do
    ALIVE_A=0
    ALIVE_B=0

    if [ -n "$PID_ERL_A" ] && kill -0 "$PID_ERL_A" 2>/dev/null; then ALIVE_A=1; fi
    if [ -n "$PID_ERL_B" ] && kill -0 "$PID_ERL_B" 2>/dev/null; then ALIVE_B=1; fi

    if [ $ALIVE_A -eq 0 ] && [ $ALIVE_B -eq 0 ]; then
        echo -e "${GREEN}[+] Erlang finalizó la orquestación correctamente.${NC}"
        break
    fi

    sleep 1
    ((TIEMPO++))
done

if [ $TIEMPO -eq $TIMEOUT_LIMIT ]; then
    echo -e "${RED}[-] ALERTA: Tiempo límite alcanzado. Posible Deadlock o Livelock.${NC}"
    DEADLOCK=true
    [ -n "$PID_ERL_A" ] && kill -9 "$PID_ERL_A" 2>/dev/null
    [ -n "$PID_ERL_B" ] && kill -9 "$PID_ERL_B" 2>/dev/null
fi

# --- DIAGNÓSTICO ---
echo -e "\n${BLUE}[Fase 4] Diagnóstico de la prueba${NC}"

# Buscar "EXITO" en los logs de Erlang (indica jobGranted real)
EXITOS_ERL=$(cat log_erl_*.txt 2>/dev/null | grep -c "EXITO" || true)
# Buscar "GRANTED" en los logs de los agentes C (concesiones a nivel de recurso)
GRANTED_C=$(cat log_c_*.txt 2>/dev/null | grep -c "GRANTED" || true)

if [ "$DEADLOCK" = true ]; then
    echo -e "${RED}====================================================================${NC}"
    echo -e "${RED} [FAIL] DETECCIÓN DE DEADLOCK: EL SISTEMA QUEDÓ CONGELADO           ${NC}"
    echo -e "${RED}====================================================================${NC}"
elif [ "$EXITOS_ERL" -eq 0 ]; then
    echo -e "${RED}====================================================================${NC}"
    echo -e "${RED} [FAIL] NINGÚN TRABAJO SE EJECUTÓ COMPLETO EN ERLANG.               ${NC}"
    echo -e "${RED}====================================================================${NC}"
else
    echo -e "${GREEN}====================================================================${NC}"
    echo -e "${GREEN} [SUCCESS] HAPPY PATH COMPLETADO. EJECUCIÓN LIMPIA.                 ${NC}"
    echo -e "${GREEN}====================================================================${NC}"
    echo -e "${YELLOW} -> Trabajos completados en Erlang: ${EXITOS_ERL}${NC}"
    echo -e "${YELLOW} -> Concesiones registradas por agentes C: ${GRANTED_C}${NC}"
fi

echo "===== PROCESOS SERVER ====="
ps -fp $PID_C1
ps -fp $PID_C2

echo "===== LOG C1 ====="
cat log_c_1.txt
echo
echo "===== LOG C2 ====="
cat log_c_2.txt
echo
echo "===== LOG ERLANG A ====="
cat log_erl_A.txt
echo
echo "===== LOG ERLANG B ====="
cat log_erl_B.txt

# Limpieza final
kill -9 $PID_C1 $PID_C2 $PID_ERL_A $PID_ERL_B 2>/dev/null || true
echo -e "${BLUE}====================================================================${NC}"