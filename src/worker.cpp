#include <climits>

#include <mpi.h>

#include "internal.hpp"
#include "signal.hpp"

#include "def.hpp"
#include "propagator.hpp"
#include "solverprocess.hpp"
#include "worker.hpp"

Worker::Worker (InstanceInfo& instance) {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    this->instance = instance;
    solver = new DistributedSolverProcess (instance);
};

Worker::~Worker () {
    delete solver;
}

int Worker::recv_task() {
    int ncube, job_rank;
    MPI_Comm comm;
    std::vector<CubeInfo> cubes;
    CubeInfo new_cubes[2];

    MPI_Recv(&task, 1, MPI_TASKINFO, 0, M_TASKINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    cubes.resize(task.n_cubeinfo);

    switch (task.type) {
    case SOLVE:
        //MPI_Recv()
        return 1;
        break;
    case SIMPLIFY:
        MPI_Recv(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cube = cubes.front();
        solver->set_cube (&cube);
        solver->simplify ();
        ncube = 1;
        MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
        MPI_Send(&cube, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
        return 1;
    case DCUBE:
        MPI_Bcast(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, MPI_COMM_WORLD);
        MPI_Comm_split(MPI_COMM_WORLD, rank % task.n_cubeinfo, rank, &comm);
        MPI_Comm_rank(comm, &job_rank);
        cube = cubes[rank % task.n_cubeinfo];
        solver->set_cube (&cube);
        solver->distributed_cube (comm);
        if (job_rank == 0) {
            ncube = 2;
            generate_new_cubes (new_cubes);
            MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
            MPI_Send(new_cubes, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
        }
        return 1;
    case PSIMPLIFY:
        MPI_Bcast(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, MPI_COMM_WORLD);
        MPI_Comm_split(MPI_COMM_WORLD, rank % task.n_cubeinfo, rank, &comm);
        MPI_Comm_rank(comm, &job_rank);
        cube = cubes[rank % task.n_cubeinfo];
        solver->set_cube (&cube);
        solver->portfolio_simplify (comm);
        if (job_rank == 0) {
            ncube = 1;
            MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
            MPI_Send(&cube, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
        }
        return 1;
    default:
        
        return 0;
    }
}

void inline Worker::generate_new_cubes (CubeInfo new_cubes[]) {
    std::string c1id = std::string(cube.id) + "1";
    std::string c2id = std::string(cube.id) + "2";
    strcpy(new_cubes[0].id, c1id.c_str());
    strcpy(new_cubes[1].id, c2id.c_str());
}

void Worker::gather_solutions () {
    // serialize solutions
    std::vector<int> serialized_solutions;
    for (const auto& solution : solver->solutions()) {
        for (auto lit : solution) {
            serialized_solutions.push_back (lit);
        }
        serialized_solutions.push_back (0);
    }
    std::size_t count = serialized_solutions.size();
    MPI_Gather(
        &count, 
        1, 
        MPI_INT, 
        NULL,
        0,
        MPI_INT,
        0, 
        MPI_COMM_WORLD
    );
    MPI_Gatherv(
        serialized_solutions.data(),
        count,
        MPI_INT,
        NULL,
        NULL,
        NULL,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );
}

void Worker::start() {
    while (recv_task()) {;}
    gather_solutions ();
    return;
}

