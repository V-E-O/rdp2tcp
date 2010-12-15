all: client

client: client/rdp2tcp
client/rdp2tcp:
	make -C client

server-mingw32: server/rdp2tcp.exe
server/rdp2tcp.exe:
	make -C server -f Makefile.mingw32

clean:
	make -C client clean
	make -C server -f Makefile.mingw32 clean
	make -C tools clean
