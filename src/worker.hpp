#ifndef WORKER_HPP
#define WORKER_HPP

#include <chrono>
#include <vector>
#include <mpi.h>

#include "internal.hpp"

#include "def.hpp"

class DistributedSolverProcess;

class Worker {
    /*--------- Instance ----------*/
    InstanceInfo instance;
    TaskInfo task;
    CubeInfo cube;

    /*--------- Solver ----------*/
    DistributedSolverProcess* solver;

    /*--------- Message Handler ----------*/
    int rank;
    int recv_task();
    void inline generate_new_cubes (CubeInfo new_cubes[]);
    void gather_solutions ();

public:
    std::chrono::steady_clock::time_point start_time;
    Worker(InstanceInfo& instance);
    ~Worker();
    void start();
};

#endif
