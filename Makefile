all: client

client: client/rdp2tcp
client/rdp2tcp:
	make -C client

server-mingw32: server/mingw32/rdp2tcp.exe
server/mingw32/rdp2tcp.exe:
	make -C server -f Makefile.mingw32

server-mingw64: server/mingw64/rdp2tcp.exe
server/mingw64/rdp2tcp.exe:
	make -C server -f Makefile.mingw64

clean:
	make -C client clean
	make -C server -f Makefile.mingw32 clean
	make -C server -f Makefile.mingw64 clean
	make -C tools clean
