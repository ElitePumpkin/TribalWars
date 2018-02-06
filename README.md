# Tribal Wars 

Collect resources, train troops and send them to battle. 
Your goal is to win five battles against other players.

## Prerequisites

ncurses library present on your Unix machine

## How to compile

Example: 

```
gcc server.c -Wall -lncurses -o server
gcc client.c -Wall -lncurses -o player
```

## How to run

First start off with server module

```
./server
```

Then run player module three times

```
./player
```

## How to play

Upper bar shows resources, income per second and amount of troops available to you at the moment.
L stands for Light Infantry, H for Heavy Infantry, C for Cavalry and W for Workers.
Use up/down buttons to move in main menu. Hit 'Enter' to insert number of units.
Choose unit you want to train then hit amount on keyboard [0-9]. Your order will be fulfilled if you have enough resources.
Choose player to attack. The Game asks you to enter three-digit sequence e.g 245. 
It is equal to "Send 2 units of light infantry, 4 units of heavy infantry and 5 units of cavalry if available"
Only 0-9 keys are allowed.


