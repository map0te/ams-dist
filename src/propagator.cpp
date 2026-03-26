
#include "clausesharer.hpp"
#include "def.hpp"
#include "propagator.hpp"
#include "symbreak.hpp"

Propagator::Propagator (const InstanceInfo& instance, CaDiCaL::Solver* solver,
    bool portfolio_mode, MPI_Comm comm) : 
    solver(solver), portfolio_mode(portfolio_mode) {

    symmetrybreaker = new SymmetryBreaker (instance.order);
    if (portfolio_mode) {
        clausesharer = new ClauseSharer (comm);
    }
}

Propagator::~Propagator () {
    delete symmetrybreaker;
    if (portfolio_mode) {
        delete clausesharer;
    }
}

long Propagator::n_solutions () { 
    return symmetrybreaker->n_solutions ();
}

std::list<std::vector<int>>& Propagator::solutions () {
    return symmetrybreaker->solutions ();
}

void Propagator::connect () {
    solver->connect_external_propagator (this);
    if (portfolio_mode) { 
        solver->connect_learner (this);
        solver->connect_terminator (this);
    }
    std::vector<int> vars = symmetrybreaker->observed_vars ();
    for (const int var : vars) {
        solver->add_observed_var(var);
    }
}

void Propagator::disconnect () {
    solver->disconnect_external_propagator ();
    if (portfolio_mode) {
        solver->disconnect_learner ();
        solver->disconnect_terminator ();
    }
}

void Propagator::notify_assignment (int lit, bool is_fixed) {
    symmetrybreaker->notify_assignment (lit, is_fixed);
}

void Propagator::notify_new_decision_level () {
    symmetrybreaker->notify_new_decision_level ();
}

void Propagator::notify_backtrack (size_t new_level) {
    symmetrybreaker->notify_backtrack (new_level);
}

bool Propagator::cb_check_found_model (const std::vector<int>& model) {
    return symmetrybreaker->cb_check_found_model (model);
}

int Propagator::cb_decide () { return 0; }
int Propagator::cb_propagate () { return 0; }
int Propagator::cb_add_reason_clause_lit (int plit) {
    (void)plit;
    return 0;
};

bool Propagator::cb_has_external_clause () {
    has_cas_clause = symmetrybreaker->cb_has_external_clause ();
    bool has_shared_clause = 
        (portfolio_mode) ? clausesharer->cb_has_external_clause () : false;
    return has_cas_clause || has_shared_clause;
}

int Propagator::cb_add_external_clause_lit () {
    if (has_cas_clause) {
        return symmetrybreaker->cb_add_external_clause_lit ();
    } else if (portfolio_mode) {
        return clausesharer->cb_add_external_clause_lit ();
    } else {
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 0;
    }
}

bool Propagator::learning (int size) {
    return clausesharer->learning (size);
}

void Propagator::learn (int lit) {
    clausesharer->learn (lit);
}
