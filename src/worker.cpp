#include <filesystem>

#include <mpi.h>

#include "internal.hpp"
#include "signal.hpp"

#include "def.hpp"
#include "symbreak.hpp"
#include "worker.hpp"

void Worker::read_file(std::string name) {
    bool incremental;
    std::vector<int> cube_literals;
    solver->read_dimacs (name.c_str(), max_var, true, incremental, cube_literals);
}

void Worker::write_file() {
    std::string output_path = current_instance + ".simp";
    solver->write_dimacs (output_path.c_str(), max_var);
}

int Worker::recv_task() {
    MPI_Recv(&task, 1, MPI_TASKINFO, 0, M_TASKINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    std::vector<CubeInfo> cubes;
    cubes.resize(task.n_cubeinfo);
    if (task.type == SIMPLIFY) {
        MPI_Recv(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cube = cubes.front();
        if (!std::strcmp(cube.id, "")) {
            current_instance = std::string(instance.top_name);
        } else {
            current_instance = std::string(instance.top_name) + "." + std::string(cube.id) + ".cnf";
        }
        simplify();
        return 1;
    }
    if (task.type == SOLVE_NOINT) {
        MPI_Recv(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cube = cubes.front();
        if (!std::strcmp(cube.id, "")) {
            current_instance = std::string(instance.top_name);
        } else {
            current_instance = std::string(instance.top_name) + "." + std::string(cube.id) + ".cnf";
        }
        solve();
        return 1;
    } 
    if (task.type == DCUBE) {
        MPI_Bcast(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, MPI_COMM_WORLD);
        cube = cubes[rank % task.n_cubeinfo];
        // Create mpi communicators per cubing task
        MPI_Comm comm;
        MPI_Comm_split(MPI_COMM_WORLD, rank % task.n_cubeinfo, rank, &comm);
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
        int dcube_rank;
        MPI_Comm_rank(comm, &dcube_rank);
        if (dcube_rank == 0) {
            int ncube = 2;
            std::string c1id = std::string(cube.id) + "1";
            std::string c2id = std::string(cube.id) + "2";
            CubeInfo c[2];
            strcpy(c[0].id, c1id.c_str());
            strcpy(c[1].id, c2id.c_str());
            MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
            MPI_Send(c, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
        }
        return 1;
    }
    return 0;
}

void Worker::send_simplify_result(int res) {
    int ncube = 1;
    MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
    MPI_Send(&cube, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
}

bool Worker::terminate() {
    return false;
}

void Worker::format_res(int res) {
    printf("[Rank %d] Result: ", rank);
    if (res == 0) {
        printf("unknown\n");
    } else if (res == 10) {
        printf("sat\n");
    } else {
        printf("unsat\n");
    }
    fflush(stdout);
}

int Worker::solve() {
    solver = new CaDiCaL::Solver ();
    solver->set("quiet", 1);
    solver->set("preprocesslight", false);
    solver->set("factor", false);
    solver->set("factorunbump", false);

    read_file(current_instance.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, instance.order, 0, 0);
    res = solver->solve ();
    cube.active = solver->active();
    cube.n_solutions = se->n_sol();
    if (res == 0) { write_file(); cube.status = UNKNOWN;} else { cube.status = UNSAT; }
    send_simplify_result(res);
    delete se;
    delete solver;
    solver = 0;
    return res;
}

int Worker::simplify() {
    // setup solver
    solver = new CaDiCaL::Solver ();
    solver->limit ("conflicts", SIMPLIMIT);
    solver->set("quiet", 1);

    solver->set("factor", false);
    solver->set("factorunbump", false);
    solver->set("preprocesslight", false);
    //solver->set("inprobing", false);
    if (strcmp(cube.id, "")) { solver->set("inprobing", false); }
    //solver->set("inprocessing", false);

    // simplify
    //printf("[Rank %d] Simplify %s\n", rank, cube.id); fflush(stdout);
    read_file(current_instance.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, instance.order, 0, 0);
    res = solver->solve ();
    cube.active = solver->active();
    cube.n_solutions = se->n_sol();
    if (res == 0) { write_file(); cube.status = UNKNOWN;} else { cube.status = UNSAT; }
    send_simplify_result(res);
    delete se;
    delete solver;
    solver = 0;
    return res;
}

/*int Worker::solve(bool interruptable) {
    // setup solver
    assert (!solver);
    solver = new CaDiCaL::Solver ();
    CaDiCaL::Signal::alarm (TIMELIMIT);
    CaDiCaL::Signal::set (this);
    solver->limit ("proofsize", PROOFSIZE);

    solver->set("factor", false);
    solver->set("factorunbump", false);
    solver->set("inprobing", false);

    // terminiator for solving
    p_flag = false;
	if (interruptable) solver->connect_terminator (this);
	
    // solve
    printf("c ----- SOLVE -----\n");
    read_file(cube.name.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, cube.order, 0);
    start_ts = clock();
    res = solver->solve ();
    if (res == 0) { write_file(); } else { std::remove(cube.name.c_str()); }
    format_res(res);
    delete se;
    delete solver;
    solver = 0;
    return res;
}*/

void Worker::start() {
    while (recv_task()) {;}
}

