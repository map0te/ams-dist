
#include "clausesharer.hpp"
#include "def.hpp"
#include "propagator.hpp"
#include "symbreak.hpp"

Propagator::Propagator (const InstanceInfo& instance, CaDiCaL::Solver* solver,
    bool portfolio_mode, MPI_Comm comm, bool share_cas_clauses) : 
    solver(solver), 
    portfolio_mode(portfolio_mode), 
    comm(comm),
    share_cas_clauses(share_cas_clauses) {

    symmetrybreaker = new SymmetryBreaker (instance.order);
    if (portfolio_mode) {
        clausesharer = new ClauseSharer (comm);
    }
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
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

std::vector<std::vector<int>>& Propagator::solutions () {
    return symmetrybreaker->solutions ();
}

void Propagator::connect () {
    solver->connect_external_propagator (this);
    if (portfolio_mode) {
        solver->connect_learner (clausesharer);
        solver->connect_terminator (this);
    }
    std::vector<int> vars = symmetrybreaker->observed_vars ();
    for (const int var : vars) {
        solver->add_observed_var(var);
    }
}

void Propagator::disconnect () {
    if (portfolio_mode) {
        if (!interrupted) {
            int dst = rank + 1;
            int src = rank - 1; 
            if (dst == size) dst = 0;
            if (src < 0) src = size - 1;
            MPI_Request req;
            MPI_Isend(NULL, 0, MPI_INT, dst, M_INTERRUPT, comm, &req);
            MPI_Recv(NULL, 0, MPI_INT, src, 
                M_INTERRUPT, comm, MPI_STATUS_IGNORE);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
        MPI_Barrier(comm);
        clausesharer->cleanup ();
    }
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
    symmetrybreaker->cb_check_found_model (model);
    return false;
}

int Propagator::cb_decide () { return 0; }
int Propagator::cb_propagate () { return 0; }
int Propagator::cb_add_reason_clause_lit (int plit) {
    (void)plit;
    return 0;
};

bool Propagator::cb_has_external_clause () {
    has_cas_clause = symmetrybreaker->cb_has_external_clause ();
    if (has_cas_clause && portfolio_mode && share_cas_clauses) {
        assert (!symmetrybreaker->cas_clauses.empty());
        clausesharer->learn_cas_clause(symmetrybreaker->cas_clauses.front());
    }
    bool has_shared_clause = 
        (portfolio_mode) ? clausesharer->cb_has_external_clause () : false;
    return has_cas_clause || has_shared_clause;
}

int Propagator::cb_add_external_clause_lit () {
    if (has_cas_clause) {
        return symmetrybreaker->cb_add_external_clause_lit ();
    } else if (portfolio_mode) {
        return clausesharer->cb_add_external_clause_lit ();
    }
    return false;
}

bool Propagator::terminate () {
    int dst = rank + 1;
    if (dst == size) dst = 0;
    int flag;
    MPI_Status status;
    MPI_Request req;
    MPI_Iprobe(MPI_ANY_SOURCE, M_INTERRUPT, comm, &flag, &status);
    if (flag) {
        MPI_Isend(NULL, 0, MPI_INT, dst, M_INTERRUPT, comm, &req);
        MPI_Request_free(&req);
        MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, 
            M_INTERRUPT, comm, MPI_STATUS_IGNORE);
        interrupted = true;
        return true;
    }
    return false;
}
