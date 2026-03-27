#ifndef SYMBREAK_HPP
#define SYMBREAK_HPP

#include <cstddef>
#include <queue>
#include <list>
#include <set>
#include <vector>

#define l_False 0
#define l_True 1
#define l_Undef 2

#define MAX(X,Y) ((X) > (Y)) ? (X) : (Y)
#define MIN(X,Y) ((X) > (Y)) ? (Y) : (X)

#define MAXORDER 40

#include "internal.hpp"

class SymmetryBreaker {
    std::deque<std::vector<int>> current_trail;

    int n = 0;
    int num_edge_vars = 0;
    std::list<std::vector<int>> _solutions;

    int* assign;
    bool* fixed;
    int* colsuntouched;
    std::set<unsigned long> canonical_hashes[MAXORDER];
    std::set<unsigned long> solution_hashes;

    long perm_cutoff[MAXORDER] = {
        0, 0, 0, 0, 0, 0, 
        20, 50, 125, 313, 783, 1958, 
        4895, 12238, 30595, 76488, 191220, 478050, 
        1195125, 2987813, 7469533, 18673833, 46684583
    };

    bool build_noncanonicity_clause ();
    bool is_canonical(int k, int p[], int& x, int& y, int& i, bool opt_pseudo_test);

public:
    std::vector<std::vector<int>> cas_clauses;
    SymmetryBreaker (int order);
    ~SymmetryBreaker ();
    void notify_assignment (int lit, bool is_fixed);
    void notify_new_decision_level ();
    void notify_backtrack (size_t new_level);
    std::vector<int> observed_vars ();
    bool cb_check_found_model (const std::vector<int>& model);
    bool cb_has_external_clause ();
    int cb_add_external_clause_lit ();
    size_t n_solutions() { return _solutions.size(); };
    std::list<std::vector<int>>& solutions() { return _solutions; };
};

#endif