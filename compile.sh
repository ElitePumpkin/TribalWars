#!/bin/bash
#Run this in terminal
gcc server.c -Wall -lncurses -o server.out
gcc client.c -Wall -lncurses -o player.out
echo "Compilation Successful, run ./server.out to begin"
exit 0