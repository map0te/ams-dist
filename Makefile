CC = mpicxx
CFLAGS = -O3 -Wall -Icadical/src

all: ams-dist

clean:
	cd cadical; make clean; cd ..
	rm -rf build
	rm -f ams-dist

build:
	mkdir -p build

build/beamlookahead.o: src/beamlookahead.cpp | build
	$(MAKEDIR) $(CC) $(CFLAGS) -c src/beamlookahead.cpp -o build/beamlookahead.o

build/symbreak.o: src/symbreak.cpp | build
	$(MAKEDIR) $(CC) $(CFLAGS) -c src/symbreak.cpp -o build/symbreak.o

build/worker.o: src/worker.cpp | build
	$(MAKEDIR) $(CC) $(CFLAGS) -c src/worker.cpp -o build/worker.o

build/manager.o: src/manager.cpp | build
	$(MAKEDIR) $(CC) $(CFLAGS) -c src/manager.cpp -o build/manager.o

build/statustracker.o: src/statustracker.cpp | build
	$(MAKEDIR) $(CC) $(CFLAGS) -c src/statustracker.cpp -o build/statustracker.o

cadical/build/libcadical.a: cadical/src/*.cpp
	cd cadical; ./configure && make; cd ..

ams-dist: cadical/build/libcadical.a build/beamlookahead.o build/symbreak.o build/worker.o build/manager.o build/statustracker.o src/main.cpp
	$(CC) $(CFLAGS)	build/*.o src/main.cpp -o ams-dist -Lcadical/build -lcadical
