#include <iostream>
#include <filesystem>
#include <mpi.h>

#include "beamlookahead.hpp"
#include "def.hpp"

MPI_Datatype MPI_VARSCORE;

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    
    init_varscore_type(MPI_VARSCORE);
    
    BeamLookahead bl;
    bl.setup(atoi(argv[1]), argv[2], MPI_COMM_WORLD);
    bl.lookahead();
    
    printf("cubing variable: %d\n", bl.cubing_var);

    MPI_Finalize();
}