#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <chrono>
#include <queue>
#include <string>
#include <vector>

#include "beamlookahead.hpp"
#include "def.hpp"

struct WorkerInfo {
    int status;
    CubeInfo cube;
    TaskInfo task; 
    WorkerInfo() { status = IDLE; }
};

class Manager {
public:
    Manager(InstanceInfo& instance) : instance(instance) {
        MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
        n_workers = n_proc - 1;
        worker_info.resize(n_proc);
        n_cubing = 0;
        n_simplifying = 0;
        n_solving = 0;
        n_terminated = 0;
        for (int i = 1; i <= n_workers; i++) { idle_workers.push(i); }
    }
    void start();
    void printtime();
    std::chrono::steady_clock::time_point start_time;
private:
    int n_proc, n_workers;
    int n_cubing, n_solving, n_simplifying, n_terminated;

    InstanceInfo instance;
    BeamLookahead beamlookahead;
    //StatusTracker status_tracker;
    std::queue<CubeInfo> simplify_queue;
    std::vector<CubeInfo> cube_queue;
    std::queue<CubeInfo> solve_queue;
    std::queue<int> idle_workers;
    std::vector<WorkerInfo> worker_info;

    void send_simplify_task(int rank, CubeInfo& cube);
    int recv_simplify_task(int &rank, CubeInfo new_cubes[]);
    void bcast_dcube_task();
    void recv_dcube_task(int ntasks, CubeInfo new_cubes[]);
    void send_interrupt(int worker);
};

#endif