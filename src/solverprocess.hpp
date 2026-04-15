#ifndef SOLVERPROCESS_HPP
#define SOLVERPROCESS_HPP

#include <chrono>
#include <vector>
#include <mpi.h>

#include "internal.hpp"

#include "beamlookahead.hpp"
#include "def.hpp"

class Propagator;

class DistributedSolverProcess : CaDiCaL::Terminator {
    InstanceInfo instance;
    CubeInfo* cube;

    CaDiCaL::Solver* solver;
    Propagator* propagator;
    BeamLookahead beamlookahead;

    std::vector<std::vector<int>> solutions_;

    std::chrono::steady_clock::time_point start_time;

    inline void write_dimacs_with_units (const std::string& path);
    inline void append_solutions(std::vector<std::vector<int>>& new_solutions);
    void diversify (MPI_Comm comm);
public:
    DistributedSolverProcess(const InstanceInfo& instance);
    void set_cube (CubeInfo* cube);
    std::vector<std::vector<int>>& solutions ();

    int simplify ();
    int solve ();
    int portfolio_simplify (MPI_Comm comm);
    void distributed_cube (MPI_Comm comm);

    inline std::string get_input_filename (bool simplifying);
    inline std::string get_output_filename (int index);
    // CaDiCaL::Terminator
    bool terminate ();
};

#endif 