#include "../src/propagator.hpp"
#include "../src/def.hpp"
#include "internal.hpp"

#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    InstanceInfo instance;
    instance.order = atoi(argv[1]);
    CaDiCaL::Solver s;
    Propagator* propagator = new Propagator(instance, &s, true);
    bool incremental;
    std::vector<int> cube_literals;
    int max_var;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        s.set("report", 1);
    } else {
        s.set("quiet", 1);
    }
    s.read_dimacs (argv[2], max_var, true, incremental, cube_literals);
    std::srand(rank); // seed once
    bool randomBool = std::rand() % 2;
    for (int i = 1; i <= max_var; i++) {
        if (randomBool) {
            s.phase(-i);
        } else {
            s.phase(i);
        }
    }
    if (rank == 0) s.limit("conflicts", 1000);
    propagator->connect();
    s.solve();
    propagator->disconnect();
    if (rank == 0) s.write_dimacs ("test.out", max_var);
    delete propagator;
    MPI_Finalize();
}