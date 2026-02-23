#include <cassert>
#include <sstream>

#include "cube.hpp"
#include "manager.hpp"
#include "worker.hpp"

#define ELAPSED (clock()-start_time) / (double) CLOCKS_PER_SEC

int Manager::recv_cubes(int& rank, Cube& cube1, Cube& cube2) {
    MPI_Status stat;
    MPI_Probe(MPI_ANY_SOURCE, CUBENUM, MPI_COMM_WORLD, &stat);
    rank = stat.MPI_SOURCE;
    std::string c1, c2;
    int count;
    MPI_Recv(&count, 1, MPI_INT, rank, CUBENUM, 
        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (count) {
        c1 = recv_cubestr(rank, CUBESTR);
        c2 = recv_cubestr(rank, CUBESTR);
        cube1 = Cube(c1);
        cube2 = Cube(c2);
    }
    return count;
}

void Manager::send_interrupt(int worker) {
    MPI_Send(NULL, 0, MPI_INT, worker, INTERRUPT, MPI_COMM_WORLD);
    winfo[worker].status = TERMINATED;
    num_terminated++;
}

void Manager::start() {
    Cube top_cube = Cube(order, CUBING, 0, "", top_name);
    Cube cube1, cube2;
    simp_queue.insert(top_cube);

    int rank, count, flag;
    int progress;
    MPI_Status stat;

    clock_t start_time = clock();
    printf("%lf INIT\n", ELAPSED);
    fflush(stdout);

    // Simplify Stage
    while (simp_queue.size() < nworkers) {
        // compute number of needed cubes
        int ncubes = std::min(simp_queue.size(), nworkers - 2*num_cubing - simp_queue.size());
        for (int i = 0; i < ncubes; i++) {
            for (int worker = 1; worker <= nworkers; worker++) {
                if (winfo[worker].status == IDLE) {
                    Cube temp = *(simp_queue.begin());
                    winfo[worker].cube = temp.str();
                    winfo[worker].status = CUBING;
                    isend_cubestr(worker, winfo[worker].cube);
                    simp_queue.erase(simp_queue.begin());
                    num_cubing++;
                    break;
                }
            }
        }
        int count = recv_cubes(rank, cube1, cube2);
        winfo[rank].status = IDLE;
        num_cubing--;
        if (count) {
            simp_queue.insert(cube1);
            simp_queue.insert(cube2);
        }
    }

    // When simplification queue is full, fill solve queue
    for (size_t i = 0; i < nworkers; i++) {
        // set cubes to SOLVE mode
        Cube cube = *(simp_queue.begin());
        cube.status = SOLVING;
        solve_queue.push(cube);
        simp_queue.erase(simp_queue.begin());
    }

    // Solve Stage
    while (!solve_queue.empty() || num_solving) {
        // Send cubes to available workers
        for (int worker = 1; worker <= nworkers; worker++) {
            if (winfo[worker].status != IDLE) continue;
            if (solve_queue.empty()) break;
            winfo[worker].cube = solve_queue.front().str();
            winfo[worker].status = SOLVING;
            isend_cubestr(worker, winfo[worker].cube);
            solve_queue.pop();
            num_solving++;
            printf("%lf ACTIVE SOLVERS: %d/%d\n", ELAPSED, num_solving, st.size());
            fflush(stdout);
        }
        // If idle workers, try to interrupt
        if (st.size() && num_solving + 2*num_terminated < nworkers) {
            StatEntry entry = st.pop();
            send_interrupt(entry.rank);
        }

        // Check for completed solves
        for (;;) {
            flag = true;
            MPI_Iprobe(MPI_ANY_SOURCE, CUBENUM, MPI_COMM_WORLD, &flag, &stat);
            if (!flag) { break; }
            int count = recv_cubes(rank, cube1, cube2);
            if (count) {
                solve_queue.push(cube1);
                solve_queue.push(cube2);
            }
            if (winfo[rank].status == SOLVING) {
                MPI_Send(NULL, 0, MPI_INT, rank, INTERRUPT, MPI_COMM_WORLD);
            } else if (winfo[rank].status == TERMINATED) {
                num_terminated--;
            } else {
                assert (false);
            }
            winfo[rank].status = IDLE;
            st.erase(rank);
            num_solving--;
            printf("%lf ACTIVE SOLVERS: %d/%d\n", ELAPSED, num_solving, st.size());
            fflush(stdout);
        }
        // Check for progress updates
        for (;;) {
            flag = true;
            MPI_Iprobe(MPI_ANY_SOURCE, PROGRESS, 
                    MPI_COMM_WORLD, &flag, &stat);
            if (!flag) { break; }
            rank = stat.MPI_SOURCE;
            MPI_Recv(&progress, 1, MPI_INT, rank, PROGRESS,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            //printf("c PROGRESS UPDATE: %d: %lf\n", rank, progress);
            //fflush(stdout);
            if (winfo[rank].status == SOLVING)
                st.update(rank, progress);
        }
    }
    // done
    for (int i = 1; i <= nworkers; i++) {
        std::string s = "";
        send_cubestr(i, s);
    }
    printf("%lf DONE\n", ELAPSED);
    fflush(stdout);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == 0) {
        std::string top = std::string(argv[3]) + "/top.cnf";
        std::stringstream cmd;
        cmd << "cp " << argv[1] << " " << top;
        system(cmd.str().c_str());
        Manager manager(size, atoi(argv[2]), top.c_str());
        manager.start();
    } else {
        Worker worker;
        worker.start();
    }
    MPI_Finalize();
}

