
INCLUDES = -Wall -Werror -g -ggdb -O2 -DDEBUG $(zeromq_CFLAGS) -Iinclude

bin_PROGRAMS = satan

if UCI_ENABLED
satan_SOURCES = main.c config.c zeromq.c superfasthash.c messages.c utils.c
else
satan_SOURCES = main.c zeromq.c superfasthash.c messages.c utils.c
endif

satan_LDADD =
