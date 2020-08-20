# rdp2tcp 0.1 (open tcp tunnel through rdp)

 ![Diagram](http://i.imgur.com/7xTFXeq.png)

## clone
```sh
git clone https://github.com/gerardo-junior/rdp2tcp.git [RDP2TCP FOLDER]
cd [RDP2TCP FOLDER]
```

## build
```sh
make client
make server-mingw32
```
#### on local host linux

```sh
xfreerdp /u:[USER] /p:[PASSWORD] /v:[HOST] /rdp2tcp:[RDP2TCP FOLDER]/client/rdp2tcp 
```

#### on remote host windows 

Upload [RDP2TCP FOLDER]/server/rdp2tcp.exe file to remote host

open cmd and run rdp2tcp.exe (it is not necessary to run with administrator privilege)

### Controller:

after you see the message "virtual channel connected" on your terminal. you can perform port forwarding using the tool: [RDP2TCP FOLDER]/tools/rdp2tcp.py

> ./tools/rdp2tcp.py add forward [local addr] [local port] [remote addr] [remote port]


```sh
./tools/rdp2tcp.py help # for more info
./tools/rdp2tcp.py add forward 127.0.0.1 10001 127.0.0.1 8000
curl 127.0.0.1:10001
```


#### if you have problems in the build with 32-bit compiler try:

```sh
wget http://archive.ubuntu.com/ubuntu/pool/universe/m/mingw32/mingw32_4.2.1.dfsg-2ubuntu1_amd64.deb;
wget http://archive.ubuntu.com/ubuntu/pool/universe/m/mingw32-binutils/mingw32-binutils_2.20-0.2ubuntu1_amd64.deb;
wget http://archive.ubuntu.com/ubuntu/pool/universe/m/mingw32-runtime/mingw32-runtime_3.15.2-0ubuntu1_all.deb;
sudo dpkg -i mingw32*.deb
sudo apt-get install -f
```

read more: http://rdp2tcp.sourceforge.net/

### how this works:
<pre>
rdp2tcp is a tunneling tool on top of remote desktop protocol (RDP).
It uses RDP virtual channel capabilities to multiplex several ports
forwarding over an already established rdesktop session.

Available features:
 - tcp port forwarding
 - reverse tcp port forwarding
 - process stdin/out forwarding
 - SOCKS5 minimal support

The code is splitted into 2 parts:
 - the client running on the rdesktop client side
 - the server running on the Terminal Server side

Once both rdp2tcp client and server are running, tunnels management is
performed by the controller (on client side). The controller typically
listen on localhost (port 8477) waiting for new tunnel registrations.


-[ client (rdesktop side) ]--------------------

First of all, rdesktop must be compiled with OOP patch (see INSTALL).
The OOP patch comes with a additional rdesktop command line option.

  -r addin:NAME:HANDLER[:OPT1[:OPTN]]

  NAME:    the name of the RDP virtual channel
  HANDLER: the path of the executable which handle
           the virtual channel.
  OPT:     argument passed to HANDLER executable

The rdp2tcp client must be initialized when the rdesktop client starts.

  rdesktop -r addin:rdp2tcp:/path/to/rdp2tcp <ip>

rdp2tcp client usage:

  rdp2tcp [[HOST] PORT]

  HOST: rdp2tcp controller hostname or IP address (default is 127.0.0.1).
  PORT: rdp2tcp controller port (default is 8477).

Several instances of rdp2tcp client can be run on a single rdesktop session:

  rdesktop -r addin:rdp2tcp-1:/path/to/rdp2tcp:8477 \
           -r addin:rdp2tcp-2:/path/to/rdp2tcp:8478 <ip>

After rdesktop is started with rdp2tcp channel configured, port forwarding
can be configured by connecting to the controller and sending commands.
All commands are ASCII and ends with a CR "\n".

  * List rdp2tcp managed sockets:
      "l\n"

  * Remove tunnel  
      "- LHOST LPORT\n"

      LHOST: tunnel local host
      LPORT: tunnel local port

  * Start SOCKS5 proxy
      "s LHOST LPORT\n"

      LHOST: proxy local host
      LPORT: proxy local port

  * stdin/stdout forwarding tunnel (bind on rdesktop)
      "x LHOST LPORT CMD\n"

      LHOST: local listener host
      LPORT: local listener port
      CMD:   command line to execute on Terminal Server host

  * TCP forwarding tunnel (bind on rdesktop)
      "t LHOST LPORT RHOST RPORT\n"

      LHOST: local listener host
      LPORT: local listener port
      RHOST: remote target host
      RPORT: remote target port

  * TCP reverse-connect tunnel (bind on Terminal Server)
      "r LHOST LPORT RHOST RPORT\n"

      LHOST: local target host
      LPORT: local target port
      RHOST: remote listener host
      RPORT: remote listener port

rdp2tcp.py (located in "tools" folder) can be used to manage tunnels with
simple command lines.
ex: "rdp2tcp.py add forward LHOST LPORT RHOST RPORT"


-[ server (Terminal Server side) ]-------------

Before starting the rdp2tcp server, you must be logged on the Terminal Server
with one or more rdp2tcp clients attached to rdesktop.

The rdp2tcp server won't magically appear on the Terminal Server. So the
rdp2tcp.exe executable must be first uploaded.

rdp2tcp.exe doesn't require to be run with a privileged Windows account.

Terminal Server policy may block file sharing through the RDP session.
Thus you may have to find a way to upload the .exe binary on the remote
system. The binary can be uploaded by scripting the TS input.

Uploading binary data to the server can be automated by encoding data to
key stroke sequences that will be given to rdesktop as keyboard input.

The rdpupload script (located in "tools" folder) generates a X11 script.
xte (http://hoopajoo.net/projects/xautomation.html) run the X11 script.

  1) start rdesktop with rdp2tcp client
  2) tools/rdpupload -x -f vb server/rdp2tcp.exe | xte"
  3) focus on the rdesktop window within 5 seconds
  4) xte will feed rdesktop with keyboard input. focused window must
     not change or you may get some trouble :)
  5) run the Visual Basic script uploaded by xte.
  6) run rdp2tcp server by using the executable generated by the
     Visual Basic script.


-[ dev ]---------------------------------------

 - edit Makefile / enable -DDEBUG
 - use client/memcheck.sh to use valgrind as a RDP channel wrapper
 - doxygen can be used to generate the project documentation
     "doxygen Doxyfile-client" --> docs/client/html
     "doxygen Doxyfile-server" --> docs/server/html
 - export DEBUG (-1 to 2) environment variable to print debug statements
 - export TRACE (00 to ff) environment variable to print function traces

	bit 0: I/O buffer management
       1: network socket  
       2: RDP virtual channel
       3: events loop
       4: process 
       5: rdp2tcp controller
       6: tunnel management
       7: SOCKS5 protocol

</pre>
