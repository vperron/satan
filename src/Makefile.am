if DEBUG_ENABLED
AM_CFLAGS = -Wall -Werror -g -ggdb -DDEBUG -std=c99 -Wno-unused-function
else
AM_CFLAGS = -Wall -Werror -Os -s -std=c99 -DNDEBUG
endif

bin_PROGRAMS = satan

if UCI_ENABLED
satan_SOURCES = main.c config.c zeromq.c superfasthash.c messages.c utils.c
else
satan_SOURCES = main.c zeromq.c superfasthash.c messages.c utils.c
endif
