CC = mpicxx
CFLAGS = -O3

all: simple_mcts ams-dist

ams-dist: simple_mcts src/worker.cpp src/cube.cpp src/symbreak.cpp src/manager.cpp
	$(CC) $(CFLAGS) -Icadical/src cadical/build/*.o src/worker.cpp src/symbreak.cpp src/cube.cpp src/manager.cpp -o ams-dist

simple_mcts: src/simple_mcts.cpp
	$(CC) $(CFLAGS) src/simple_mcts.cpp -o simple_mcts
