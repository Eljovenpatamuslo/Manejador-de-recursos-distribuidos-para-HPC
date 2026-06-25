# ==============================================================================
# Makefile del proyecto completo (servidor C + módulos Erlang)
# ==============================================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -g -I. -Iestructuras
LDFLAGS  = -lpthread

# Fuentes C (raíz)
SRC_ROOT = server.c manejadores.c sockets.c utils.c

# Fuentes C (estructuras)
SRC_EST  = estructuras/utils.c \
           estructuras/cola.c \
           estructuras/recursos.c \
           estructuras/tablajobs.c \
           estructuras/tablanodos.c

# Todos los fuentes
SRCS     = $(SRC_ROOT) $(SRC_EST)

# Los objetos se generan junto a los fuentes (misma ruta, extensión .o)
OBJS     = $(SRCS:.c=.o)

# Ejecutable del servidor C
TARGET_C = server

# --- Erlang ---
ERL_SRCS = job_scheduler.erl job_server.erl logF.erl manejador_recursos.erl send_recv_manager.erl
ERL_DIR  = job-scheduler
ERL_SRCS_PATH = $(addprefix $(ERL_DIR)/, $(ERL_SRCS))

# Los .beam se generan en la raíz del proyecto (fuera de job-scheduler/)
ERL_BEAMS = $(ERL_SRCS:.erl=.beam)

# --- Reglas ---

.PHONY: all all_c all_erl clean clean_erl clean_c

all: all_c all_erl

all_c: $(TARGET_C)

all_erl: $(ERL_BEAMS)

# Enlazado del servidor C
$(TARGET_C): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Regla genérica: compila cualquier .c y produce el .o en el mismo directorio
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compilación de cada .erl → .beam en el directorio actual
%.beam: job-scheduler/%.erl
	erlc -o . $<

# Limpieza completa: elimina objetos C, ejecutable C y .beam
clean:
	rm -f $(OBJS) $(TARGET_C) $(ERL_BEAMS)

# Limpieza solo de Erlang
clean_erl:
	rm -f $(ERL_BEAMS)

# Limpieza solo de C (objetos + ejecutable)
clean_c:
	rm -f $(OBJS) $(TARGET_C)
