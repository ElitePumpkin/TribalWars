# Tribal Wars 

Collect resources, train troops and send them to battle. 
Your goal is to win five battles.

## Prerequisites

ncurses library present on your Unix machine

## How to compile

Example:

```
gcc server.c -Wall -lncurses -o server
gcc client.c -Wall -lncurses -o player
```

Or use bash script provided: 

```
./compile.sh
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

Keep in mind that no more than one server process may be running at any time.

## How to play

Upper bar shows resources, income per second and amount of troops available to you at the moment.
L stands for Light Infantry, H for Heavy Infantry, C for Cavalry and W for Workers.
Use up/down buttons to move in main menu. Hit 'Enter' to insert number of units.
Choose unit you want to train then hit amount on your keyboard. Confirm your choice with
ENTER. There will be no prompt message, just press the keys. 
Your order will be fulfilled if you have enough resources.
Choose player to attack. The Game asks you to enter amount of units to be sent. 
Insert number of Light Infantry, hit ENTER, number of Heavy Infantry, hit ENTER then
insert number of Cavalry and hit ENTER.
First player to carry out 5 succesfull attacks wins the Game.
Use of CTRL-C is disabled.



