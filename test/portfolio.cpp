#include "../src/propagator.hpp"
#include "../src/def.hpp"
#include "internal.hpp"

#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    InstanceInfo instance;
    instance.order = 17;
    CaDiCaL::Solver s;
    Propagator* propagator = new Propagator(instance, &s, true);
    bool incremental;
    std::vector<int> cube_literals;
    int max_var;
    s.read_dimacs ("/storage/home/hcoda1/1/mzhulin3/scratch/instances/17/ks_17.cnf", max_var, true, incremental, cube_literals);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::srand(rank); // seed once
    bool randomBool = std::rand() % 2;
    if (rank == 0) s.set("report", 1);
    for (int i = 1; i <= max_var; i++) {
        if (randomBool) {
            s.phase(-i);
        } else {
            s.phase(i);
        }
    }
    propagator->connect();
    s.solve();
    propagator->terminate_all();
    propagator->disconnect();
    delete propagator;
    MPI_Finalize();
}