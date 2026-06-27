# Manejador-de-recursos-distribuidos-para-HPC
Trabajo practico de la materia Sistemas Operativos I

# Integrantes
Lucas Lamberti,
Agustin Jaffre,
Franco Bramucci,
Ayrton Cuffaro


## Requisitos

- **Compilador C** (gcc) con soporte para pthreads.
- **Erlang/OTP** (erlc, erl).
- **Sistema Linux** con loopback (`lo`) y permisos para `ip route`, `ldconfig` (si se usa `fix_bind.so`).  
- **Herramientas estándar**: `bash`, `ss`, `lsof`, `killall`, `sudo`.

## Compilación

Para construir todo (agente C + módulos Erlang):
> make
# Compilar C
> make all_c  
# Compilar Erlang
> make all_erl

# Ejecución:
> ./server <MI_IP> <MI_PUERTO_TCP> <IP_BROADCAST> <DIR_LOCAL> # Ejecución del servidor de c
> erl -> job_scheduler:scheduler_init(MI_PUERTO_TCP).

## Limpieza:
> make clean      # Elimina objetos, ejecutable y .beam
> make clean_c    # Solo limpia C
> make clean_erl  # Solo limpia Erlang


