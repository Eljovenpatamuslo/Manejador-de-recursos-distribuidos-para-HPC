# ==============================================================================
# Makefile del proyecto completo (servidor C + módulos Erlang)
# ==============================================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -g -I. -Iestructuras
LDFLAGS  = -lpthread

# Directorio para objetos C
OBJDIR   = obj

# Fuentes C (raíz)
SRC_ROOT = server.c manejadores.c sockets.c utils.c

# Fuentes C (estructuras)
SRC_EST  = estructuras/utils.c \
           estructuras/cola.c \
           estructuras/recursos.c \
           estructuras/tablajobs.c \
           estructuras/tablanodos.c

SRCS     = $(SRC_ROOT) $(SRC_EST)
OBJS     = $(addprefix $(OBJDIR)/, $(SRCS:.c=.o))

# Ejecutable del servidor C
TARGET_C = server

# --- Erlang ---
ERL_SRCS = job_scheduler.erl job_server.erl logF.erl manejador_recursos.erl send_recv_manager.erl
ERL_DIR  = job-scheduler
ERL_SRCS_PATH = $(addprefix $(ERL_DIR)/, $(ERL_SRCS))

# Directorio de salida para los .beam
ERL_OUT  = $(ERL_DIR)

# --- Reglas ---

.PHONY: all all_c all_erl clean clean_erl clean_c

all: all_c all_erl

all_c: $(TARGET_C)

all_erl: $(ERL_OUT)/.compiled

$(TARGET_C): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ERL_OUT)/.compiled: $(ERL_SRCS_PATH)
	@mkdir -p $(ERL_OUT)
	erlc -o $(ERL_OUT) $^
	@touch $@

# Limpieza completa: elimina objetos C, ejecutables C y compiled-code Erlang
clean:
	rm -rf $(OBJDIR) $(TARGET_C) $(ERL_OUT)

# Limpieza solo de Erlang
clean_erl:
	rm -rf $(ERL_OUT)

# Limpieza solo de C (objetos + ejecutable)
clean_c:
	rm -rf $(OBJDIR) $(TARGET_C)
