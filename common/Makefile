CC=gcc
CFLAGS=-Wall -g 
#		 -DDEBUG
OBJS=	iobuf.o print.o msgparser.o nethelper.o netaddr.o

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS) $(BIN)
