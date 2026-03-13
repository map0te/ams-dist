#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <chrono>
#include <queue>
#include <string>
#include <vector>

#include "beamlookahead.hpp"
#include "def.hpp"
#include "statustracker.hpp"

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
    void print_time();
    void init_time();
private:
    std::chrono::steady_clock::time_point start_time;

    int n_proc, n_workers;
    int n_cubing, n_solving, n_simplifying, n_terminated;
    long n_solutions = 0;

    InstanceInfo instance;
    BeamLookahead beamlookahead;
    StatusTracker statustracker;

    std::queue<CubeInfo> simplify_queue;
    std::vector<CubeInfo> cube_queue;
    std::vector<CubeInfo> solve_queue;

    std::queue<int> idle_workers;
    std::vector<WorkerInfo> worker_info;

    void send_simplify_task();
    int recv_simplify_task();

    void send_solve_task(bool interruptable);
    void iprobe_recv_solve_task();

    void bcast_dcube_task();
    void exec_dcube_task();
    void recv_dcube_task(int ntasks, CubeInfo new_cubes[]);

    void send_interrupt(int rank);
    void iprobe_recv_active();
};

#endif