# Lumac0
Lumac0 chess engine

## General Information
Lumac0 is a chess engine written from scratch in C++ by [@dbenson42](https://github.com/dbenson42).

It is also avalaible on [Lichess](https://lichess.org/@/Lumac0), and it's rated around 1850 points.

The engine is under continous improvement, and still needs a lot of optimizations.

## How to compile
Lumac0 doesn't require any third party library, all you need in order to compile it is a c++ compiler, preferably g++.

Here is the command to compile the engine using g++ once you're in the directory:

```
g++ -std=c++11 -Ofast -olumac0.exe lumac0.cpp
```

## Features
Lumac0 is a traditional chess engine without any kind of NN-related code.

### Search
 - Alpha-Beta negamax search
 - Transposition tables using zobrist hashing
 - Quiscence search
 - Iterative deepening
 - Gradually widening aspiration windows

### Pruning and Reductions/Extensions
 - Late move reductions
 - Static null move pruning
 - Null move pruning
 - Futility pruning
 - Razoring
 - Check extensions

### Move ordering
 - Q-search only has MVV-LVA
 - Hash move first
 - MVV-LVA
 - Countermove Heuristic
 - History Heuristic
 - Killer Moves

### Evaluation
 - Piece-square tables
 - Passed, doubled, isolated pawns

## How to use
The engine uses the UCI interface.
It has the following options (excluding almost useless ones):

 - Threads: Number of threads used by the engine, minimum (and suggested) value is 2: 1 for the search and 1 for the input cehcking.
 - Hash: How many Megabytes of RAM the engine is allowed to use for the TT.
 - Ponder: Whether pondering is activated or not. (Pondering = thinking during the opponent's time)

## Credits
Lumac0 is completely based upon information found on the [Chess Programming Wiki](https://www.chessprogramming.org).
