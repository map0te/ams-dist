#include <filesystem>
#include <climits>

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

void Worker::write_file(bool temp) {
    std::string output_path;
    if (!temp) {
        output_path = current_instance + ".simp";
    } else {
        output_path = current_instance + ".temp";
    }
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
    if (task.type == SOLVE_NOINT || task.type == SOLVE) {
        MPI_Recv(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cube = cubes.front();
        if (!std::strcmp(cube.id, "")) {
            current_instance = std::string(instance.top_name) + ".simp";
        } else {
            current_instance = std::string(instance.top_name) + "." + std::string(cube.id) + ".cnf.simp";
        }
        solve(task.type == SOLVE);
        return 1;
    } 
    if (task.type == DCUBE) {
        MPI_Bcast(cubes.data(), task.n_cubeinfo, MPI_CUBEINFO, 0, MPI_COMM_WORLD);
        cube = cubes[rank % task.n_cubeinfo];
        if (!std::strcmp(cube.id, "")) {
            current_instance = std::string(instance.top_name) + ".simp";
        } else {
            current_instance = std::string(instance.top_name) + "." + std::string(cube.id) + ".cnf.simp";
        }
        dcube();
        return 1;
    }
    return 0;
}

void Worker::send_simplify_result(int res) {
    int ncube = 1;
    MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
    MPI_Send(&cube, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
}

int Worker::solve(bool interruptable) {
    cube.active = INT_MAX;
    start_time = std::chrono::steady_clock::now();

    solver = new CaDiCaL::Solver ();
    solver->set("quiet", 1);

    solver->set("preprocesslight", false);
    solver->set("factor", false);
    solver->set("factorunbump", false);

    read_file(current_instance.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, instance.order, 0, 0);
    if (interruptable) solver->connect_terminator(this);

    res = solver->solve ();
    cube.active = solver->active();
    cube.n_solutions = se->n_sol();

    if (res == 0) { 
        write_file(true); 
        cube.status = UNKNOWN;
        scube();
        int ncube = 2;
        std::string c1id = std::string(cube.id) + "1";
        std::string c2id = std::string(cube.id) + "2";
        CubeInfo c[2];
        strcpy(c[0].id, c1id.c_str());
        strcpy(c[1].id, c2id.c_str());
        c[0].status = UNKNOWN;
        c[1].status = UNKNOWN;
        c[0].n_solutions = cube.n_solutions;
        c[1].n_solutions = 0;
        MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
        MPI_Send(c, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
    } else { 
        int ncube = 1;
        cube.status = UNSAT; 
        MPI_Send(&ncube, 1, MPI_INT, 0, M_NUMCUBE, MPI_COMM_WORLD);
        MPI_Send(&cube, ncube, MPI_CUBEINFO, 0, M_CUBEINFO, MPI_COMM_WORLD);
    }

    if (res) {
        MPI_Recv(NULL, 0, MPI_INT, 0, M_INTERRUPT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

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
    if (instance.inprobing == 0) {
        solver->set("inprobing", false);
    } else if (instance.inprobing == 1) {
        if (strcmp(cube.id, "")) { solver->set("inprobing", false); }
    }

    read_file(current_instance.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, instance.order, 0, 0);

    res = solver->solve ();
    cube.active = solver->active();
    cube.n_solutions = se->n_sol();

    if (res == 0) { 
        write_file(false); 
        cube.status = UNKNOWN;
    } else { 
        cube.status = UNSAT; 
    }
    send_simplify_result(res);

    delete se;
    delete solver;
    solver = 0;
    return res;
}

int Worker::scube() {
    MPI_Comm comm = MPI_COMM_SELF;
    std::string out1 = std::string(instance.top_name) + "." + std::string(cube.id) + "1.cnf.simp";
    std::string out2 = std::string(instance.top_name) + "." + std::string(cube.id) + "2.cnf.simp";
    std::string infile = current_instance + ".temp";
    beamlookahead.setup(instance.order, infile.c_str(), comm);
    beamlookahead.lookahead();
    beamlookahead.write_cubes(infile.c_str(), out1.c_str(), out2.c_str());
    return 0;
}

int Worker::dcube() {
    MPI_Comm comm;
    MPI_Comm_split(MPI_COMM_WORLD, rank % task.n_cubeinfo, rank, &comm);
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
    return 0;
}

bool Worker::terminate() {
    auto time_stamp = std::chrono::steady_clock::now();
    auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(time_stamp - start_time).count();
    if (seconds_elapsed < instance.twarmup) { return false; }
    // send active var update
    MPI_Request req;
    if (solver->active() < cube.active) {
        cube.active = solver->active();
        MPI_Isend(&cube.active, 1, MPI_INT, 0, M_ACTIVE, MPI_COMM_WORLD, &req);
        MPI_Request_free(&req);
    }
    // probe for interrupt request
    MPI_Status status;
    int flag;
    MPI_Iprobe(0, M_INTERRUPT, MPI_COMM_WORLD, &flag, &status);
    if (!flag) { return false; }
    MPI_Recv(NULL, 0, MPI_INT, 0, M_INTERRUPT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    return true;
}

void Worker::start() {
    while (recv_task()) {;}
}

