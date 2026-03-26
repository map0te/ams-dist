#ifndef PROPAGATOR_HPP
#define PROPAGATOR_HPP

#include <list>
#include <vector>

#include <mpi.h>

#include "internal.hpp"

class ClauseSharer;
class SymmetryBreaker;
struct InstanceInfo;

class Propagator : CaDiCaL::ExternalPropagator, CaDiCaL::Learner, CaDiCaL::Terminator {
    CaDiCaL::Solver* solver;
    ClauseSharer* clausesharer;
    SymmetryBreaker* symmetrybreaker;
    bool portfolio_mode;
    bool has_cas_clause;
public:
    // Propagator
    Propagator (const InstanceInfo& instance, CaDiCaL::Solver* solver, bool portfolio_mode = false, MPI_Comm comm = MPI_COMM_WORLD);
    ~Propagator ();
    long n_solutions ();
    std::list<std::vector<int>>& solutions ();
    void connect ();
    void disconnect ();

    // CaDiCaL::ExternalPropagator
    void notify_assignment (int lit, bool is_fixed);
    void notify_new_decision_level ();
    void notify_backtrack (size_t new_level);
    bool cb_check_found_model (const std::vector<int>& model);
    int cb_decide ();
    int cb_propagate ();
    int cb_add_reason_clause_lit (int plit);

    bool cb_has_external_clause ();
    int cb_add_external_clause_lit ();

    // CaDiCaL::Learner
    bool learning (int size);
    void learn (int lit);

    // CaDiCaL::Terminator
    bool terminate () { return false; };
};

#endif