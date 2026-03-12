#ifndef BEAMLOOKAHEAD_HPP
#define BEAMLOOKAHEAD_HPP

#include <vector>
#include <queue>
#include <mpi.h>

#include "def.hpp"

#define HEADER_BUF 1024
#define BIMP_GROW_CHUNK 4
#define ASSIGN_NONE 0
#define ASSIGN_TRUE 1
#define ASSIGN_FALSE 2

class BeamLookahead {
private:
    MPI_Comm comm;

    int m;
    int n_vars, n_clauses;
    int current_stamp = 1;
    std::vector<int> clause_literals;
    std::vector<unsigned long> clause_idx;
    std::vector<std::vector<int>> watch_list;
    std::vector<std::pair<int, int>> watched;

    std::vector<int*> bimp;
    std::vector<int> bimp_size;
    std::vector<int> bimp_capacity;
    
    int* global_queue = 0;
    uint8_t* assignments = 0;
    uint8_t* is_unit_var = 0;

    inline int lit_index(int lit);
    inline void reset_assignments();
    inline bool is_preselected(int var);
    inline bool is_lit_true(int lit);
    inline bool is_lit_false(int lit);
    void add_bimp(int lit, int implied);
    int propagate_bimp(int lit, std::vector<int>& propagated);
    int update_watches_and_propagate(int false_lit, std::vector<int>& propagated, int& queue_rear);
    int propagate_with_assumptions(const std::vector<int>& assumptions, std::vector<int>& propagated);
    int formula_score(const std::vector<int>& assumptions);
    std::vector<VarScore> rank_all_vars(int max_var, const std::vector<int>& base_assumptions);
    VarScore score_variable_under_base(int var, const std::vector<int>& base_assumptions);
    void parse_cnf(const char *filename);

public:
    int cubing_var;
    void setup(int order, const char *infile, MPI_Comm comm);
    void lookahead();
    int write_cubes(const char* infile, const char* outfile1, const char* outfile2);
    ~BeamLookahead();
};

#endif