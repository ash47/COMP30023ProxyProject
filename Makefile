CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -pipe -std=c99 -Wno-packed-bitfield-compat -Wpointer-arith -Wformat-nonliteral -Winit-self -Wshadow -Wcast-qual -Wmissing-prototypes
EXE = proxy
LIBS = -lsocket -lnsl -lpthread
PSRC = proxy.c

compile: $(PSRC)
	$(CC) $(PSRC) $(CFLAGS) -o $(EXE) $(LIBS)
