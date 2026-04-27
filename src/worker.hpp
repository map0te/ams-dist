#ifndef WORKER_HPP
#define WORKER_HPP

#include <vector>
#include <mpi.h>

#include "internal.hpp"

#include "def.hpp"

class DistributedSolverProcess;

class Worker {
    InstanceInfo instance;
    TaskInfo task;
    CubeInfo cube;

    DistributedSolverProcess* solver;

    int rank;
    int recv_task();
    void inline generate_new_cubes (CubeInfo new_cubes[]);
    void gather_solutions ();

public:
    Worker(InstanceInfo& instance);
    ~Worker();
    void start();
};

#endif
