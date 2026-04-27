#include <chrono>
#include <filesystem>
#include <vector>
#include <mpi.h>

#include "internal.hpp"

#include "beamlookahead.hpp"
#include "clausesharer.hpp"
#include "def.hpp"
#include "propagator.hpp"
#include "solverprocess.hpp"

inline void DistributedSolverProcess::append_solutions(std::vector<std::vector<int>>& new_solutions) {
    solutions_.insert(solutions_.end(), new_solutions.begin(), new_solutions.end());
}

inline std::string DistributedSolverProcess::get_input_filename (bool simplifying) {
    if (!std::strcmp(cube->id, "")) {
        if (simplifying) {
            return instance.top_name;
        } else {
            return instance.top_name + ".simp";
        }
    } else {
        if (simplifying) {
            return instance.top_name + "." + std::string(cube->id) + ".cnf";
        } else {
            return instance.top_name + "." + std::string(cube->id) + ".cnf.simp";
        }
    }
}

inline std::string DistributedSolverProcess::get_output_filename (int index) {
    if (index == 1) {
        return instance.top_name + "." + std::string(cube->id) + "1.cnf";
    } else {
        return instance.top_name + "." + std::string(cube->id) + "2.cnf";
    }
}


DistributedSolverProcess::DistributedSolverProcess (const InstanceInfo& instance) {
    this->instance = instance;
}

void DistributedSolverProcess::set_cube (CubeInfo* cube) {
    this->cube = cube;
}

std::vector<std::vector<int>>& DistributedSolverProcess::solutions () {
    return solutions_;
}

int DistributedSolverProcess::simplify () {
    int max_var, res;
    bool incremental;
    std::vector<int> cube_literals;
    std::string input_file, output_file;
    
    solver = new CaDiCaL::Solver ();
    propagator = new Propagator (instance, solver);

    input_file = get_input_filename (true);
    output_file = input_file + ".simp";

    solver->limit ("conflicts", SIMPLIMIT * 2);
    solver->set ("quiet", 1);
    solver->set ("elim", 0);
    solver->set ("factor", 0);
    propagator->connect ();
    solver->read_dimacs (input_file.c_str(), max_var, true, incremental, cube_literals);

    res = solver->solve ();

    cube->status = res;
    cube->active = solver->active ();
    append_solutions (propagator->solutions());

    if (res == 0) {
        solver->write_dimacs (output_file.c_str(), solver->vars());
    }

    propagator->disconnect ();

    std::filesystem::remove(input_file);

    delete propagator;
    delete solver;
    return res;
}

int DistributedSolverProcess::solve () {
    int max_var, res;
    bool incremental;
    std::vector<int> cube_literals;
    std::string input_file, output_file;

    solver = new CaDiCaL::Solver ();
    solver->set ("terminateint", 100);
    propagator = new Propagator (instance, solver);

    input_file = get_input_filename (true);
    output_file = input_file + ".simp";

    solver->set ("quiet", 1);
    propagator->connect ();
    solver->read_dimacs (input_file.c_str(), max_var, true, incremental, cube_literals);

    start_time = std::chrono::steady_clock::now();
    solver->connect_terminator (this);
    cube->active = solver->irredundant ();
    res = solver->solve ();
    solver->disconnect_terminator ();

    cube->status = res;
    cube->active = solver->active ();
    append_solutions (propagator->solutions());

    std::filesystem::remove(input_file);

    if (res == 0) {
        // cube
        solver->write_dimacs (output_file.c_str(), solver->vars());
        bool cube_res = distributed_cube (MPI_COMM_SELF);
        propagator->disconnect ();
        
        if (!cube_res) {
            delete solver;
            delete propagator;
            printf("continuing solving %s\n", cube->id);
            solver = new CaDiCaL::Solver ();
            solver->set ("terminateint", 100);
            propagator = new Propagator (instance, solver);
            solver->set ("quiet", 1);
            propagator->connect ();
            solver->read_dimacs (output_file.c_str(), max_var, true, incremental, cube_literals);
            res = solver->solve ();
            propagator->disconnect ();
            assert(res != 0);
            cube->status = res;
            cube->active = solver->active ();
            append_solutions (propagator->solutions());
            std::filesystem::remove(output_file);
            res = 30;
        }
    } else {
        propagator->disconnect ();
    }
    

    delete propagator;
    delete solver;
    return res;
}

int DistributedSolverProcess::portfolio_simplify (MPI_Comm comm) {
    int max_var, res;
    bool incremental;
    std::vector<int> cube_literals;
    std::string input_file, output_file;

    solver = new CaDiCaL::Solver ();
    propagator = new Propagator (instance, solver, true, comm, instance.share_cas);

    input_file = get_input_filename (true);
    output_file = input_file + ".simp";

    solver->limit ("conflicts", SIMPLIMIT * 2);
    solver->set ("quiet", 1);
    solver->set ("elim", 0);
    solver->set ("factor", 0);
    propagator->connect ();
    solver->read_dimacs (input_file.c_str(), max_var, true, incremental, cube_literals);

    // MPI
    int job_rank;
    MPI_Comm_rank(comm, &job_rank);

    // diversify
    std::srand(job_rank);
    bool randomBool = std::rand() % 2;
    for (int i = 1; i <= max_var; i++) {
        if (randomBool) {
            solver->phase (-i);
        } else {
            solver->phase (i);
        }
    }

    res = solver->solve ();

    // collect results from solvers
    if (job_rank == 0) {
        MPI_Reduce (MPI_IN_PLACE, &res, 1, MPI_INT, MPI_MAX, 0, comm);
    } else {
        MPI_Reduce (&res, &res, 1, MPI_INT, MPI_MAX, 0, comm);
    }

    cube->status = res;
    cube->active = solver->active ();
    append_solutions (propagator->solutions());

    if (job_rank == 0 && res == 0) {
        solver->write_dimacs (output_file.c_str(), solver->vars());
    }

    if (job_rank == 0) {
        std::filesystem::remove (input_file);
    }

    propagator->disconnect ();

    delete propagator;
    delete solver;
    return res;
}

bool DistributedSolverProcess::distributed_cube (MPI_Comm comm) {
    int job_rank;
    bool res;
    std::string input_file;

    input_file = get_input_filename (false);

    beamlookahead.setup (instance.order, input_file.c_str(), comm);
    res = beamlookahead.lookahead ();
    if (res) {
        beamlookahead.write_cubes (
            input_file.c_str(), 
            get_output_filename(1).c_str(),  
            get_output_filename(2).c_str()
        );
    }

    MPI_Comm_rank (comm, &job_rank);
    if (job_rank == 0 && res) {
        std::filesystem::remove (input_file);
    }
    return res;
}

bool DistributedSolverProcess::terminate () {
    auto time_stamp = std::chrono::steady_clock::now();
    auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(time_stamp - start_time).count();
    if (seconds_elapsed < instance.twarmup) { return false; }
    // send active var update
    // probe for interrupt request
    MPI_Status status;
    int flag;
    MPI_Iprobe(0, M_INTERRUPT, MPI_COMM_WORLD, &flag, &status);
    if (!flag) {
        MPI_Request req;
        cube->active = solver->irredundant ();
        MPI_Isend(&cube->active, 1, MPI_INT, 0, M_ACTIVE, MPI_COMM_WORLD, &req);
        MPI_Request_free(&req);
        return false;
    }
    MPI_Recv(NULL, 0, MPI_INT, 0, M_INTERRUPT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    return true;
}