#ifndef CLAUSESHARE_HPP
#define CLAUSESHARE_HPP

#include <vector>
#include <set>

#include <mpi.h>

#include "internal.hpp"

#define l_False 0
#define l_True 1
#define l_Undef 2

#define MAX(X,Y) ((X) > (Y)) ? (X) : (Y)
#define MIN(X,Y) ((X) > (Y)) ? (Y) : (X)

#define MAXORDER 40

class ClauseShare : CaDiCaL::Learner, CaDiCaL::ExternalPropagator, CaDiCaL::Terminator {

    CaDiCaL::Solver * solver;

    std::vector<int> export_clause_literals;
    std::vector<int> import_clause_literals;

    std::vector<std::vector<int>> cas_clauses;
    
    std::deque<std::vector<int>> current_trail;
    int * assign;
    bool * fixed;
    int * colsuntouched;
    int n = 0;
    int unembeddable_check = 0;
    long sol_count = 0;
    int num_edge_vars = 0;
    std::set<unsigned long> canonical_hashes[MAXORDER];
    std::set<unsigned long> solution_hashes;
    const char* outfile;
    
    MPI_Comm comm;

    bool canonicity_check();
    bool is_canonical(int k, int p[], int& x, int& y, int& i, bool opt_pseudo_test);
public:
    ClauseShare (CaDiCaL::Solver* solver, int order, const char* outfile, MPI_Comm comm);
    ~ClauseShare ();
    // Learner
    bool learning (int size);
    void learn (int lit);
    // External Propagator
    void notify_assignment (int lit, bool is_fixed);
    void notify_new_decision_level ();
    void notify_backtrack (size_t new_level);
    bool cb_check_found_model (const std::vector<int> & model);
    bool cb_has_external_clause ();
    int cb_add_external_clause_lit ();
    int cb_decide ();
    int cb_propagate ();
    int cb_add_reason_clause_lit (int plit);
    // Terminator
    bool terminate () { return false; };
    // Misc
    void share ();
    long n_sol () { return sol_count; };
};

#endif