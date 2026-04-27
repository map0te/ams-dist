#include <cassert>
#include <chrono>
#include <filesystem>

#include "internal.hpp"

#include "def.hpp"
#include "manager.hpp"
#include "propagator.hpp"
#include "worker.hpp"


void Manager::send_solve_task(bool interruptable) {
    int rank = idle_workers.front();
    const CubeInfo& cube = solve_queue.back(); 
    TaskInfo task;

    task.type = interruptable ? SOLVE : SOLVE_NOINT;
    task.n_cubeinfo = 1;

    worker_info[rank].status = SOLVING;
    worker_info[rank].cube = cube;
    worker_info[rank].task = task;

    idle_workers.pop();
    solve_queue.pop_back();

    MPI_Send(&worker_info[rank].task, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    MPI_Send(&worker_info[rank].cube, 1, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD);
    n_solving++;
}

void Manager::send_simplify_task() {
    int rank = idle_workers.front();
    const CubeInfo& cube = simplify_queue.back();
    TaskInfo task;

    task.type = SIMPLIFY;
    task.n_cubeinfo = 1;

    worker_info[rank].status = SIMPLIFYING;
    worker_info[rank].cube = cube;
    worker_info[rank].task = task;

    idle_workers.pop();
    simplify_queue.pop_back();

    MPI_Send(&worker_info[rank].task, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    MPI_Send(&worker_info[rank].cube, 1, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD);
    n_simplifying++;
}

void Manager::iprobe_recv_solve_task() {
    MPI_Status status;
    int count, rank;
    int flag = true;
    std::vector<CubeInfo> new_cubes;

    while (flag) {
        MPI_Iprobe(MPI_ANY_SOURCE, M_NUMCUBE, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            rank = status.MPI_SOURCE;
            MPI_Recv(&count, 1, MPI_INT, rank, M_NUMCUBE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            new_cubes.resize(count);

            MPI_Recv(new_cubes.data(), count, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < count; i++) {
                if (new_cubes[i].status == UNKNOWN) {
                    solve_queue.push_back(new_cubes[i]);
                }
                n_solutions += new_cubes[i].n_solutions;
            }

            statustracker.erase(rank);

            if (worker_info[rank].status == SOLVING) {
                send_interrupt(rank);
            }
            n_terminated--;
            worker_info[rank].status = IDLE;
            idle_workers.push(rank);
        }
    }
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
    MPI_Bcast(cube_queue.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, MPI_COMM_WORLD);
}

void Manager::bcast_psimp_task () {
    TaskInfo task;
    task.type = PSIMPLIFY;
    task.n_cubeinfo = simplify_queue.size();
    for (int rank = 1; rank <= n_workers; rank++) {
        worker_info[rank].status = PSIMPLIFYING;
        worker_info[rank].task = task;
        MPI_Send(&task, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    }
    MPI_Bcast(simplify_queue.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, MPI_COMM_WORLD);
}

inline void generate_new_cubes (CubeInfo& cube, CubeInfo new_cubes[]) {
    std::string c1id = std::string(cube.id) + "1";
    std::string c2id = std::string(cube.id) + "2";
    strcpy(new_cubes[0].id, c1id.c_str());
    strcpy(new_cubes[1].id, c2id.c_str());
    new_cubes[0].status = UNKNOWN;
    new_cubes[1].status = UNKNOWN;
}

void Manager::exec_dcube_task() {
    MPI_Comm comm;
    MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &comm);
    CubeInfo cube = cube_queue[0];
    CubeInfo new_cubes[2];
    solver->set_cube (&cube);
    solver->distributed_cube (comm);
    generate_new_cubes (cube, new_cubes);
    simplify_queue.push_back(new_cubes[0]);
    simplify_queue.push_back(new_cubes[1]);
}

void Manager::exec_psimp_task() {
    MPI_Comm comm;
    MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &comm);
    CubeInfo cube = simplify_queue[0];
    solver->set_cube (&cube);
    solver->portfolio_simplify (comm);
    if (cube.status == UNKNOWN) {   
        cube_queue.push_back(cube);
    }
}

void Manager::recv_dcube_task() {
    MPI_Status status;
    int count, rank;
    CubeInfo new_cubes[2];
    for (int i = 0; i < (int) cube_queue.size() - 1; i++) {
        MPI_Probe(MPI_ANY_SOURCE, M_NUMCUBE, MPI_COMM_WORLD, &status);
        rank = status.MPI_SOURCE;
        MPI_Recv(&count, 1, MPI_INT, rank, M_NUMCUBE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (count) {
            MPI_Recv(new_cubes, count, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int j = 0; j < count; j++) {
                simplify_queue.push_back(new_cubes[j]);
            }
        }
    }
    for (rank = 1; rank <= n_workers; rank++) {
        worker_info[rank].status = IDLE;
    }
    cube_queue.clear();
}

void Manager::recv_psimp_task() {
    MPI_Status status;
    int count, rank;
    CubeInfo new_cube;
    for (int i = 0; i < (int) simplify_queue.size() - 1; i++) {
        MPI_Probe(MPI_ANY_SOURCE, M_NUMCUBE, MPI_COMM_WORLD, &status);
        rank = status.MPI_SOURCE;
        MPI_Recv(&count, 1, MPI_INT, rank, M_NUMCUBE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&new_cube, count, MPI_CUBEINFO, rank, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (new_cube.status == UNKNOWN) {
            cube_queue.push_back(new_cube);
        }
    }
    for (rank = 1; rank <= n_workers; rank++) {
        worker_info[rank].status = IDLE;
    }
    simplify_queue.clear();
}

void Manager::iprobe_recv_active() {
    MPI_Status status;
    int rank;
    int flag = true;
    while (flag) {
        MPI_Iprobe(MPI_ANY_SOURCE, M_ACTIVE, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int active;
            rank = status.MPI_SOURCE;
            MPI_Recv(&active, 1, MPI_INT, rank, M_ACTIVE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (worker_info[rank].status == SOLVING) {
                statustracker.update(rank, active);
            }
        }
    }
}

void Manager::send_interrupt(int rank) {
    MPI_Send(NULL, 0, MPI_INT, rank, M_INTERRUPT, MPI_COMM_WORLD);
    worker_info[rank].status = TERMINATED;
    n_solving--;
    n_terminated++;
}

void Manager::print_time() {
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

void Manager::init_time() {
    start_time = std::chrono::steady_clock::now();
}

void Manager::start() {
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    CubeInfo top;
    top.active = 0;
    std::strcpy(top.id, "");
    simplify_queue.push_back(top);

    log("----- Cubing -----\n");

    int cube_s = 0;
    int simp_s = 0;

    bool printflag = false;

    // ------- Distributed Cubing ------- //
    while ((int) simplify_queue.size() < size) {
        // Simplify
        auto simp_start = std::chrono::steady_clock::now();

        bcast_psimp_task();
        exec_psimp_task();
        recv_psimp_task();

        auto simp_end = std::chrono::steady_clock::now();
        simp_s = std::chrono::duration_cast<std::chrono::milliseconds>(simp_end - simp_start).count();
        total_simplifying_time += std::chrono::duration_cast<std::chrono::milliseconds>(simp_end - simp_start);

        if ((int) cube_queue.size() == 0) {
            log("Cubes: %ld (d-cube: %d.%03ds, p-simplify: %d.%03ds)\n",
                simplify_queue.size(), cube_s / 1000, cube_s % 1000, simp_s / 1000, simp_s % 1000);
            break;
        }

        // Cube
        auto cube_start = std::chrono::steady_clock::now();
        bcast_dcube_task();
        exec_dcube_task();
        recv_dcube_task();
        auto cube_end = std::chrono::steady_clock::now();

        cube_s = std::chrono::duration_cast<std::chrono::milliseconds>(cube_end - cube_start).count();
        total_cubing_time += std::chrono::duration_cast<std::chrono::milliseconds>(cube_end - cube_start);

        log("Cubes: %ld (d-cube: %d.%03ds, p-simplify: %d.%03ds)\n",
            simplify_queue.size(), cube_s / 1000, cube_s % 1000, simp_s / 1000, simp_s % 1000);

        cube_s = 0;
    }

    log("Generated %ld cubes\n", simplify_queue.size());

    while (!simplify_queue.empty()) {
        solve_queue.push_back(simplify_queue.back());
        simplify_queue.pop_back();
    }

    // ------- Solving ------- //
    if (!solve_queue.empty()) {
        log("----- Solving ----\n");
    }

    auto solve_start = std::chrono::steady_clock::now();

    int prev_n_solving = 0;
    while (!solve_queue.empty() || n_solving || n_terminated) {
        // send work to free cores
        while (!solve_queue.empty() && ! idle_workers.empty()) {
            send_solve_task(true);
        }
        // recv completed/interrupted solves
        iprobe_recv_solve_task();
        // check for active var updates
        iprobe_recv_active();
        while (!solve_queue.empty() && ! idle_workers.empty()) {
            send_solve_task(true);
        }
        // interrupt solvers
        int to_interrupt = n_workers - n_solving - 2*n_terminated;
        for (int i = 0; i < to_interrupt; i++) {
            if (statustracker.size()) {
                StatEntry stat = statustracker.pop();
                send_interrupt(stat.rank);
            }
        }
        if (prev_n_solving != n_solving && instance.verbose) {
            log("Active solvers: %d/%d/%d\n", n_solving, n_terminated, n_workers);
        }
        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        if (!printflag && !instance.verbose && (total_seconds % 60 == 0)) {
            log("Active solvers: %d/%d\n", n_solving, n_workers);
            printflag = true;
        }
        if (total_seconds % 60 == 1) { printflag = false; }
        prev_n_solving = n_solving;
    }


    // ------- Finish ------- //
    TaskInfo end;
    end.type = END;
    end.n_cubeinfo = 0;
    for (int rank = 1; rank <= n_workers; rank++) {
        MPI_Send(&end, 1, MPI_TASKINFO, rank, M_TASKINFO, MPI_COMM_WORLD);
    }

    auto solve_end = std::chrono::steady_clock::now();
    total_solving_time += std::chrono::duration_cast<std::chrono::milliseconds>(solve_end - solve_start);

    log("----- Unsatisfiable -----\n");

    std::vector<int> local_serialized_solutions;
    for (const auto& solution : solver->solutions()) {
        for (auto lit : solution) {
            local_serialized_solutions.push_back (lit);
        }
        local_serialized_solutions.push_back (0);
    }
    int local_serialized_count = local_serialized_solutions.size();

    std::vector<int> serialized_solutions;
    int* sol_counts = new int [size];
    int* sol_counts_displs = new int [size];
    int sol_counts_total;
    int local_count = local_serialized_count;
    sol_counts_displs[0] = 0;

    MPI_Gather(&local_count, 1, MPI_INT, sol_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int i = 1; i < size; i++) {
        sol_counts_displs[i] = sol_counts_displs[i-1] + sol_counts[i-1];
    }
    sol_counts_total = sol_counts_displs[size-1] + sol_counts[size-1];

    serialized_solutions.resize(sol_counts_total);
    MPI_Gatherv(
        local_serialized_solutions.data(),
        local_serialized_count,
        MPI_INT,
        serialized_solutions.data(),
        sol_counts,
        sol_counts_displs,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    std::vector<int> temp_solution;
    std::set<std::vector<int>> final_solutions;

    for (std::size_t i = 0; i < serialized_solutions.size(); i++) {
        if (serialized_solutions[i] == 0) {
            final_solutions.insert(temp_solution);
            temp_solution.clear();
        } else {
            temp_solution.push_back(serialized_solutions[i]);
        }
    }

    auto total_time = total_cubing_time + total_simplifying_time + total_solving_time;

    log("Found %ld solutions\n", final_solutions.size());
    log("----- Statistics -----\n");
    log("Runtime: %3.2f%% cubing, %3.2f%% simplifying, %3.2f%% solving\n",
        total_cubing_time.count() / (double) total_time.count() * 100,
        total_simplifying_time.count() / (double) total_time.count() * 100,
        total_solving_time.count() / (double) total_time.count() * 100);
    log("----- Solutions -----\n");
    for (auto solution : final_solutions) {
        for (auto lit : solution) {
            printf("%d ", lit);
        }
        printf("\n");
    }
    fflush(stdout);
    delete [] sol_counts;
    delete [] sol_counts_displs;
    return;
}

