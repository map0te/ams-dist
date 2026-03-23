#include "../src/clauseshare.hpp"
#include "internal.hpp"

#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    CaDiCaL::Solver s;
    ClauseShare* cs = new ClauseShare(&s, 20, NULL, MPI_COMM_WORLD);
    bool incremental;
    std::vector<int> cube_literals;
    int max_var;
    s.read_dimacs ("/storage/home/hcoda1/1/mzhulin3/scratch/instances/20/ks_20.cnf", max_var, true, incremental, cube_literals);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::srand(rank); // seed once
    bool randomBool = std::rand() % 2;
    if (!rank) s.set("report", 1);
    for (int i = 1; i <= max_var; i++) {
        if (randomBool) {
            s.phase(-i);
        } else {
            s.phase(i);
        }
    }
    s.limit("conflicts", 1000);
    int res = s.solve();
    int multiplier = 1;
    while (multiplier < 7) {
        cs->share();
        s.limit("conflicts", 1000 * multiplier);
        res = s.solve();
        multiplier++;
    }
    if (!rank) { s.write_dimacs (argv[1], max_var); } 
    delete cs;
    MPI_Finalize();
}