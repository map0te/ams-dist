CC = mpicxx
CFLAGS = -O3 -Icadical/src

all: ams-dist

clean:
	rm src/*.o
	rm ams-dist

src/beamlookahead.o: src/beamlookahead.cpp
	$(CC) $(CFLAGS) -c src/beamlookahead.cpp -o src/beamlookahead.o

src/symbreak.o:	src/symbreak.cpp
	$(CC) $(CFLAGS) -c src/symbreak.cpp -o src/symbreak.o

src/worker.o: src/worker.cpp
	$(CC) $(CFLAGS) -c src/worker.cpp -o src/worker.o

src/manager.o: src/manager.cpp
	$(CC) $(CFLAGS) -c src/manager.cpp -o src/manager.o
	
ams-dist: src/beamlookahead.o src/symbreak.o src/worker.o src/manager.o src/main.cpp
	$(CC) $(CFLAGS) cadical/build/*.o src/*.o src/main.cpp -o ams-dist
