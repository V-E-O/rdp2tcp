#!/bin/sh
valgrind --tool=memcheck \
	-v --log-file=/tmp/rdp2tcp.log \
	--leak-check=full --show-reachable=yes \
	--leak-resolution=med --track-origins=yes \
	./rdp2tcp
