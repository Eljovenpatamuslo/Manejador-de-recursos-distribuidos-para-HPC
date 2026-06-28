#!/bin/bash

# ==============================================================================
# SCRIPT DE PRUEBA: DETECCIÓN Y RESOLUCIÓN DE DEADLOCK
# Genera dependencias circulares que causarían deadlock sin las medidas
# apropiadas (timeout + backoff). Tu código DEBE resolverlo.
# ==============================================================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0;0m'

echo -e "${BLUE}====================================================================${NC}"
echo -e "${BLUE} INICIANDO TEST DE DEADLOCK (DEPENDENCIAS CRUZADAS)                 ${NC}"
echo -e "${BLUE}====================================================================${NC}"

PORT_A=12000
PORT_B=12001
TIMEOUT_LIMIT=60  # Más tiempo porque los reintentos toman tiempo

# --- LIMPIEZA ---
echo -e "${YELLOW}[*] Limpiando entorno...${NC}"
killall -9 agente_c_1 agente_c_2 erl beam.smp 2>/dev/null || true
fuser -k -n tcp $PORT_A 2>/dev/null || true
fuser -k -n tcp $PORT_B 2>/dev/null || true
sleep 2
lsof -ti tcp:${PORT_A} | xargs kill -9 2>/dev/null || true
lsof -ti tcp:${PORT_B} | xargs kill -9 2>/dev/null || true
rm -f logErl.txt log_erl_*.txt log_c_*.txt
sleep 1

# --- DESPLIEGUE ---
echo -e "\n${BLUE}[Fase 1] Lanzando topología...${NC}"

echo -e "[*] Agente C - Nodo A (127.0.0.2:${PORT_A})..."
stdbuf -oL ./server 127.0.0.2 ${PORT_A} 127.255.255.255 127.0.0.1 > log_c_1.txt 2>&1 &
PID_C1=$!

echo -e "[*] Agente C - Nodo B (127.0.0.3:${PORT_B})..."
stdbuf -oL ./server 127.0.0.3 ${PORT_B} 127.255.255.255 127.0.0.1 > log_c_2.txt 2>&1 &
PID_C2=$!

echo "[*] Esperando inicialización..."
sleep 3
kill -0 $PID_C1 || { echo -e "${RED}Nodo A murió${NC}"; exit 1; }
kill -0 $PID_C2 || { echo -e "${RED}Nodo B murió${NC}"; exit 1; }

echo "================ TABLA DE PUERTOS ================="
ss -lntp | grep -E "12000|12001"
echo "==================================================="

# --- ORQUESTACIÓN (dependencias circulares) ---
echo -e "\n${BLUE}[Fase 2] Lanzando jobs con dependencias circulares...${NC}"
echo -e "${YELLOW}[!] Cada nodo pide CPUs a AMBOS nodos simultáneamente${NC}"
echo -e "${YELLOW}[!] Esto crea una dependencia circular: A espera a B, B espera a A${NC}"

# Nodo A: pide 4 CPUs a A y 4 CPUs a B (total 8 CPUs)
erl -sname nodoA -noshell -eval "
    logF:crear_error_managment(),
    send_recv_manager:send_recv_init(${PORT_A}),
    job_server:iniciar_job_server(),

    F = fun Loop() -> receive _ -> Loop() end end,
    register(scheduler, spawn(F)),

    io:format('~n[NODO A] Lanzando job con dependencias circulares...~n'),
    job_server:crear_job(\"@127.0.0.2:cpu:4 @127.0.0.3:cpu:4\"),
    
    % Dar tiempo para que se resuelva (con reintentos)
    timer:sleep(45000),
    
    io:format('~n[NODO A] Finalizando después de esperar resolución...~n'),
    init:stop().
" > log_erl_A.txt 2>&1 &
PID_ERL_A=$!

# Pequeña pausa para asegurar que A empiece primero
sleep 1

# Nodo B: pide 4 CPUs a A y 4 CPUs a B (total 8 CPUs) - mismas dependencias
erl -sname nodoB -noshell -eval "
    logF:crear_error_managment(),
    send_recv_manager:send_recv_init(${PORT_B}),
    job_server:iniciar_job_server(),

    F = fun Loop() -> receive _ -> Loop() end end,
    register(scheduler, spawn(F)),

    io:format('~n[NODO B] Lanzando job con dependencias circulares...~n'),
    job_server:crear_job(\"@127.0.0.2:cpu:4 @127.0.0.3:cpu:4\"),
    
    % Dar tiempo para que se resuelva (con reintentos)
    timer:sleep(45000),
    
    io:format('~n[NODO B] Finalizando después de esperar resolución...~n'),
    init:stop().
" > log_erl_B.txt 2>&1 &
PID_ERL_B=$!

# --- MONITOREO ---
echo -e "\n${BLUE}[Fase 3] Monitoreando (límite ${TIMEOUT_LIMIT}s)...${NC}"
echo -e "${YELLOW}Esperando que los timeouts + backoff resuelvan el deadlock...${NC}"
TIEMPO=0
RESUELTO=false

while [ $TIEMPO -lt $TIMEOUT_LIMIT ]; do
    ALIVE_A=0; ALIVE_B=0
    [ -n "$PID_ERL_A" ] && kill -0 "$PID_ERL_A" 2>/dev/null && ALIVE_A=1
    [ -n "$PID_ERL_B" ] && kill -0 "$PID_ERL_B" 2>/dev/null && ALIVE_B=1
    
    # Verificar si alguno ya tuvo éxito
    if grep -q "EXITO" log_erl_A.txt 2>/dev/null || grep -q "EXITO" log_erl_B.txt 2>/dev/null; then
        echo -e "${GREEN}[+] ¡Al menos un job ha tenido éxito! El sistema está resolviendo el deadlock.${NC}"
        RESUELTO=true
    fi
    
    if [ $ALIVE_A -eq 0 ] && [ $ALIVE_B -eq 0 ]; then
        echo -e "${GREEN}[+] Procesos Erlang finalizados.${NC}"
        break
    fi
    
    # Mostrar progreso cada 10 segundos
    if [ $((TIEMPO % 10)) -eq 0 ] && [ $TIEMPO -gt 0 ]; then
        echo -e "${YELLOW}[*] Tiempo transcurrido: ${TIEMPO}s...${NC}"
        if [ "$RESUELTO" = false ]; then
            echo -e "${YELLOW}[*] Esperando que el backoff exponencial haga efecto...${NC}"
        fi
    fi
    
    sleep 1
    ((TIEMPO++))
done

if [ $TIEMPO -ge $TIMEOUT_LIMIT ]; then
    echo -e "${RED}[-] ALERTA: Tiempo límite alcanzado.${NC}"
    echo -e "${RED}[-] Si no hay éxitos, el deadlock NO se resolvió.${NC}"
    [ -n "$PID_ERL_A" ] && kill -9 "$PID_ERL_A" 2>/dev/null
    [ -n "$PID_ERL_B" ] && kill -9 "$PID_ERL_B" 2>/dev/null
fi

# --- DIAGNÓSTICO ---
echo -e "\n${BLUE}[Fase 4] Diagnóstico${NC}"

EXITOS_A=$(grep -c "EXITO" log_erl_A.txt 2>/dev/null || echo "0")
EXITOS_B=$(grep -c "EXITO" log_erl_B.txt 2>/dev/null || echo "0")
DENIED_A=$(grep -c "ALERTA: job denegado" log_erl_A.txt 2>/dev/null || echo "0")
DENIED_B=$(grep -c "ALERTA: job denegado" log_erl_B.txt 2>/dev/null || echo "0")
TIMEOUTS_A=$(grep -c "ALERTA: Timeout en red" log_erl_A.txt 2>/dev/null || echo "0")
TIMEOUTS_B=$(grep -c "ALERTA: Timeout en red" log_erl_B.txt 2>/dev/null || echo "0")

echo -e "${BLUE}--- Resultados ---${NC}"
echo -e "Jobs exitosos:      A=${EXITOS_A}, B=${EXITOS_B}"
echo -e "Jobs denegados:     A=${DENIED_A}, B=${DENIED_B}"
echo -e "Timeouts de red:    A=${TIMEOUTS_A}, B=${TIMEOUTS_B}"

if [ "$EXITOS_A" -gt 0 ] || [ "$EXITOS_B" -gt 0 ]; then
    echo -e "${GREEN}====================================================================${NC}"
    echo -e "${GREEN} [SUCCESS] EL SISTEMA RESOLVIÓ EL DEADLOCK CORRECTAMENTE.           ${NC}"
    echo -e "${GREEN} Al menos un job se completó a pesar de las dependencias circulares.${NC}"
    echo -e "${GREEN}====================================================================${NC}"
else
    echo -e "${RED}====================================================================${NC}"
    echo -e "${RED} [FAIL] DEADLOCK NO RESUELTO.                                      ${NC}"
    echo -e "${RED} Ningún job se completó. Revisar timeouts y backoff.               ${NC}"
    echo -e "${RED}====================================================================${NC}"
fi

echo -e "\n${YELLOW}[*] Si ves denied/timeout seguidos de EXITO, el backoff funcionó:${NC}"
echo -e "${YELLOW}    1. Ambos jobs intentan reservar todos los recursos.${NC}"
echo -e "${YELLOW}    2. Se produce deadlock (cada uno retiene sus CPUs locales).${NC}"
echo -e "${YELLOW}    3. Uno (o ambos) sufren timeout/denied.${NC}"
echo -e "${YELLOW}    4. Liberan sus recursos locales.${NC}"
echo -e "${YELLOW}    5. El backoff hace que reintenten en momentos distintos.${NC}"
echo -e "${YELLOW}    6. El que reintenta primero obtiene TODO y completa.${NC}"

# Mostrar logs relevantes (últimas 30 líneas de cada uno)
echo -e "\n${BLUE}===== ÚLTIMAS LÍNEAS DE LOG C1 =====${NC}"
tail -n 30 log_c_1.txt 2>/dev/null || echo "(vacío)"

echo -e "\n${BLUE}===== ÚLTIMAS LÍNEAS DE LOG C2 =====${NC}"
tail -n 30 log_c_2.txt 2>/dev/null || echo "(vacío)"

echo -e "\n${BLUE}===== LOG ERLANG A (completo) =====${NC}"
cat log_erl_A.txt 2>/dev/null || echo "(vacío)"

echo -e "\n${BLUE}===== LOG ERLANG B (completo) =====${NC}"
cat log_erl_B.txt 2>/dev/null || echo "(vacío)"

# Limpieza final
kill -9 $PID_C1 $PID_C2 $PID_ERL_A $PID_ERL_B 2>/dev/null || true
echo -e "\n${BLUE}====================================================================${NC}"
echo -e "${BLUE} TEST FINALIZADO                                                   ${NC}"
echo -e "${BLUE}====================================================================${NC}"
