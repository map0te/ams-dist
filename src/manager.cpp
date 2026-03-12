#include <cassert>
#include <chrono>

#include "def.hpp"
#include "manager.hpp"
#include "worker.hpp"

#define ELAPSED (clock()-start_time) / (double) CLOCKS_PER_SEC
#define INFO(info) printf("%lf %s\n", ELAPSED, info); fflush(stdout);

MPI_Datatype MPI_TASKINFO;
MPI_Datatype MPI_CUBEINFO;
MPI_Datatype MPI_VARSCORE;

void Manager::send_solve_task(int rank, CubeInfo& cube, bool interruptable) {
    TaskInfo task;
    if (interruptable) {
        task.type = SOLVE;
    } else {
        task.type = SOLVE_NOINT;
    }
    task.n_cubeinfo = 1;
    worker_info[rank].status = SOLVING;
    worker_info[rank].cube = cube;
    worker_info[rank].task = task;
    MPI_Send(&worker_info[rank].task, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    MPI_Send(&worker_info[rank].cube, 1, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD);
    return;
}

int Manager::recv_solve_task(CubeInfo new_cubes[]) {
    MPI_Status status;
    int count, rank;
    MPI_Probe(MPI_ANY_SOURCE, M_NUMCUBE, MPI_COMM_WORLD, &status);
    rank = status.MPI_SOURCE;
    MPI_Recv(&count, 1, MPI_INT, rank, M_NUMCUBE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(new_cubes, count, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    worker_info[rank].status = IDLE;
    idle_workers.push(rank);
    for (int i = 0; i < count; i++) {
        if (new_cubes[i].status == UNKNOWN) {
            solve_queue.push_back(new_cubes[i]);
        }
        n_solutions += new_cubes[i].n_solutions;
    }
    n_solving--;
    return count;
}

void Manager::send_simplify_task() {
    int rank = idle_workers.front();
    const CubeInfo& cube = simplify_queue.front();
    TaskInfo task;
    
    task.type = SIMPLIFY;
    task.n_cubeinfo = 1;

    worker_info[rank].status = SIMPLIFYING;
    worker_info[rank].cube = cube;
    worker_info[rank].task = task;

    idle_workers.pop();
    simplify_queue.pop();

    MPI_Send(&worker_info[rank].task, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    MPI_Send(&worker_info[rank].cube, 1, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD);
    n_simplifying++;
    return;
}

int Manager::recv_simplify_task() {
    MPI_Status status;
    int count, rank;
    std::vector<CubeInfo> new_cubes;

    MPI_Probe(MPI_ANY_SOURCE, M_NUMCUBE, MPI_COMM_WORLD, &status);
    rank = status.MPI_SOURCE;
    worker_info[rank].status = IDLE;
    idle_workers.push(rank);

    MPI_Recv(&count, 1, MPI_INT, rank, M_NUMCUBE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    new_cubes.resize(count);

    MPI_Recv(new_cubes.data(), count, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    for (int i = 0; i < count; i++) {
        if (new_cubes[i].status == UNKNOWN) {
            cube_queue.push_back(new_cubes[i]);
        }
        n_solutions += new_cubes[i].n_solutions;
    }
    n_simplifying--;
    return count;
}

void Manager::bcast_dcube_task() {
    TaskInfo task;
    task.type = DCUBE;
    task.n_cubeinfo = cube_queue.size();
    for (int rank = 1; rank <= n_workers; rank++) {
        worker_info[rank].status = DCUBING;
        worker_info[rank].task = task;
        MPI_Send(&task, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    }
    MPI_Bcast(cube_queue.data(), task.n_cubeinfo, MPI_CUBEINFO, ROOT, MPI_COMM_WORLD);
    return;
}

void Manager::recv_dcube_task(int ntasks, CubeInfo new_cubes[]) {
    MPI_Status status;
    int count, rank;
    for (int i = 0; i < ntasks-1; i++) {
        MPI_Probe(MPI_ANY_SOURCE, M_NUMCUBE, MPI_COMM_WORLD, &status);
        rank = status.MPI_SOURCE;
        MPI_Recv(&count, 1, MPI_INT, rank, M_NUMCUBE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (count) {
            MPI_Recv(new_cubes, count, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int j = 0; j < count; j++) {
                simplify_queue.push(new_cubes[j]);
            }
        }
    }
    for (rank = 1; rank <= n_workers; rank++) {
        worker_info[rank].status = IDLE;
    }
    return;
}

void Manager::printtime() {
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    int days = total_seconds / 86400;
    total_seconds %= 86400;
    int hours = total_seconds / 3600;
    total_seconds %= 3600;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    printf("[%d-%02d:%02d:%02d] ", days, hours, minutes, seconds);
}

void Manager::start() {
    CubeInfo top;
    top.active = 0;
    std::strcpy(top.id, "");
    simplify_queue.push(top);
    int n_prev_cubes = 0;
    bool force_solving = false;

    printtime();
    printf("Beginning Cubing...\n"); fflush(stdout);

    CubeInfo new_cubes[2];
    int cube_s = 0;

    // Distributed Cubing
    while ( !simplify_queue.empty() ) {
        auto simp_start = std::chrono::steady_clock::now();
        int simp_s = 0;
        // Simplify
        while (!simplify_queue.empty()) {
            if (idle_workers.empty()) {
                recv_simplify_task();
            }
            send_simplify_task();
        }
        while (n_simplifying) {
            recv_simplify_task();
        }

        auto simp_end = std::chrono::steady_clock::now();
        simp_s = std::chrono::duration_cast<std::chrono::milliseconds>(simp_end - simp_start).count();
        printtime();
        printf("Cubes: %d (d-cube: %d.%03ds, simplify: %d.%03ds)\n", cube_queue.size(), cube_s / 1000, cube_s % 1000, simp_s / 1000, simp_s % 1000); fflush(stdout);

        if (cube_queue.size() == 0) { break; };
        if (cube_queue.size() >= n_workers) { break; };
        if (cube_queue.size() < n_prev_cubes) {
            printtime();
            printf("Number of cubes has decreased. Solving without interruption\n");
            force_solving = true;
            break; 
        };

        n_prev_cubes = cube_queue.size();

        // Cube
        bcast_dcube_task();
        MPI_Comm comm;
        MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &comm);
        CubeInfo cube = cube_queue[0];
        std::string current_instance;
        if (!std::strcmp(cube.id, "")) {
            current_instance = std::string(instance.top_name) + ".simp";
        } else {
            current_instance = std::string(instance.top_name) + "." + std::string(cube.id) + ".cnf.simp";
        }
        std::string out1 = std::string(instance.top_name) + "." + std::string(cube.id) + "1.cnf";
        std::string out2 = std::string(instance.top_name) + "." + std::string(cube.id) + "2.cnf";
        beamlookahead.setup(instance.order, current_instance.c_str(), comm);
        beamlookahead.lookahead();
        beamlookahead.write_cubes(current_instance.c_str(), out1.c_str(), out2.c_str());
        MPI_Barrier(MPI_COMM_WORLD);
        int ncube = 2;
        std::string c1id = std::string(cube.id) + "1";
        std::string c2id = std::string(cube.id) + "2";
        CubeInfo c[2];
        strcpy(c[0].id, c1id.c_str());
        strcpy(c[1].id, c2id.c_str());
        simplify_queue.push(c[0]);
        simplify_queue.push(c[1]);
        recv_dcube_task(cube_queue.size(), new_cubes);
        cube_queue.clear();
        cube_s = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - simp_end).count();
    }

    printtime();
    printf("Generated %d cubes\n", cube_queue.size()); fflush(stdout);
    while (!cube_queue.empty()) {
        CubeInfo cube_to_solve = cube_queue.back();
        solve_queue.push_back(cube_queue.back());
        cube_queue.pop_back();
    }

    if (!solve_queue.empty()) {
        printtime();
        printf("Beginning Solving...\n"); fflush(stdout);
    }

    if (force_solving && !solve_queue.empty()) {
        while ( !solve_queue.empty() ) {
            int worker_rank = idle_workers.front();
            idle_workers.pop();
            send_solve_task(worker_rank, solve_queue.back(), false);
            solve_queue.pop_back();
            n_solving++;
        }
        while (n_solving) {
            recv_solve_task(new_cubes);
        }
    } else {
        ;
    }

    printtime();
    printf("Found %ld solutions\n", n_solutions); fflush(stdout);
    TaskInfo end;
    end.type = END;
    end.n_cubeinfo = 0;
    cube_queue.clear();
    for (int rank = 1; rank <= n_workers; rank++) {
        MPI_Send(&end, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    }
    while (!simplify_queue.empty()) { simplify_queue.pop(); }
}

