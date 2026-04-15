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
#include "util.hpp"

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

inline void DistributedSolverProcess::write_dimacs_with_units (const std::string& path) {
    std::vector<int> units;
    for (int var = 1; var <= solver->vars(); var++) {
        int fix = solver->fixed(var);
        if (fix != 0) units.push_back(fix > 0 ? var : -var);
    }

    solver->write_dimacs(path.c_str(), solver->vars());

    if (units.empty()) return;

    FILE* f = fopen(path.c_str(), "r+");

    char line[64];
    fpos_t header_pos;
    while (true) {
        fgetpos(f, &header_pos);
        fgets(line, sizeof(line), f);
        if (line[0] == 'p') break;
    }

    int vars, clauses;
    sscanf(line, "p cnf %d %d", &vars, &clauses);
    int new_clauses = clauses + (int)units.size();

    char old_header[64], new_header[64];
    int old_len = snprintf(old_header, sizeof(old_header), "p cnf %d %d", vars, clauses);
    int new_len = snprintf(new_header, sizeof(new_header), "p cnf %d %d", vars, new_clauses);

    if (new_len <= old_len) {
        // Safe to patch in-place — pad to preserve byte offsets
        while (new_len < old_len) new_header[new_len++] = ' ';
        new_header[new_len] = '\0';
        fsetpos(f, &header_pos);
        fputs(new_header, f);
        fseek(f, 0, SEEK_END);
    } else {
        // Header grew — full rewrite from header position onward
        // Read everything after the header
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fpos_t body_pos;
        fgetpos(f, &body_pos); // end of header line
        // re-find body start
        fsetpos(f, &header_pos);
        fgets(line, sizeof(line), f); // skip old header
        long body_start = ftell(f);
        long body_size = file_size - body_start;
        std::vector<char> body(body_size);
        fread(body.data(), 1, body_size, f);

        fsetpos(f, &header_pos);
        fprintf(f, "%s\n", new_header);
        fwrite(body.data(), 1, body_size, f);
    }

    for (int lit : units) fprintf(f, "%d 0\n", lit);

    fclose(f);
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

    solver->limit ("conflicts", SIMPLIMIT);
    solver->set ("quiet", 1);
    solver->read_dimacs (input_file.c_str(), max_var, true, incremental, cube_literals);

    propagator->connect ();
    res = solver->solve ();
    propagator->disconnect ();

    cube->status = res;
    cube->active = solver->active ();
    append_solutions (propagator->solutions());

    if (res == 0) {
        solver->write_dimacs (output_file.c_str(), solver->vars());
    }

    std::filesystem::remove(input_file);

    delete propagator;
    delete solver;
    return res;
}

int DistributedSolverProcess::solve () {
    int max_var, res;
    bool incremental;
    std::vector<int> cube_literals;
    std::string input_file;

    solver = new CaDiCaL::Solver ();
    propagator = new Propagator (instance, solver);

    input_file = get_input_filename (false);

    solver->set ("quiet", 1);
    solver->read_dimacs (input_file.c_str(), max_var, true, incremental, cube_literals);

    start_time = std::chrono::steady_clock::now();
    propagator->connect ();
    solver->connect_terminator (this);
    res = solver->solve ();
    solver->disconnect_terminator ();
    propagator->disconnect ();

    return res;
}

int DistributedSolverProcess::portfolio_simplify (MPI_Comm comm) {
    int max_var, res;
    bool incremental;
    std::vector<int> cube_literals;
    std::string input_file, output_file;

    solver = new CaDiCaL::Solver ();
    propagator = new Propagator (instance, solver, true, comm);

    input_file = get_input_filename (true);
    output_file = input_file + ".simp";

    solver->limit ("conflicts", SIMPLIMIT * 2);
    solver->set ("quiet", 1);
    solver->set ("elim", 0);
    solver->set ("factor", 0);

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

    propagator->connect ();
    res = solver->solve ();
    propagator->disconnect ();

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
        write_dimacs_with_units (output_file.c_str());
    }

    if (job_rank == 0) {
        std::filesystem::remove (input_file);
    }

    delete propagator;
    delete solver;
    return res;
}

void DistributedSolverProcess::distributed_cube (MPI_Comm comm) {
    int job_rank;
    std::string input_file;

    input_file = get_input_filename (false);

    beamlookahead.setup (instance.order, input_file.c_str(), comm);
    beamlookahead.lookahead ();
    beamlookahead.write_cubes (
        input_file.c_str(), 
        get_output_filename(1).c_str(),  
        get_output_filename(2).c_str()
    );
    MPI_Barrier (MPI_COMM_WORLD);

    MPI_Comm_rank (comm, &job_rank);
    if (job_rank == 0) {
        std::filesystem::remove (input_file);
    }
}

bool DistributedSolverProcess::terminate () {
    auto time_stamp = std::chrono::steady_clock::now();
    auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(time_stamp - start_time).count();
    if (seconds_elapsed < instance.twarmup) { return false; }
    // send active var update
    MPI_Request req;
    if (solver->active() < cube->active) {
        cube->active = solver->active();
        MPI_Isend(&cube->active, 1, MPI_INT, 0, M_ACTIVE, MPI_COMM_WORLD, &req);
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