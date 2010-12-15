BIN=rdp2tcp
CC=gcc
CFLAGS=-Wall -g -I../common
#CFLAGS=-Wall -g -I../common -DDEBUG
LDFLAGS=
OBJS=main.o netsock.o tunnel.o channel.o commands.o controller.o socks5.o \
	  ../common/nethelper.o \
	  ../common/netaddr.o \
	  ../common/iobuf.o \
	  ../common/print.o \
	  ../common/msgparser.o

all: clean_common $(BIN)

clean_common:
	$(MAKE) -C ../common clean

$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) 

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS) $(BIN)
