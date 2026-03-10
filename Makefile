CC = mpicxx
CFLAGS = -std=c++20 -O3

all: ams-dist

ams-dist: src/beamlookahead.cpp src/worker.cpp src/symbreak.cpp src/manager.cpp
	$(CC) $(CFLAGS) -Icadical/src cadical/build/*.o src/beamlookahead.cpp src/worker.cpp src/symbreak.cpp src/manager.cpp -o ams-dist
