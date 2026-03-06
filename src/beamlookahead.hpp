#ifndef BEAMLOOKAHEAD_HPP
#define BEAMLOOKAHEAD_HPP

#include <vector>

#define HEADER_BUF 1024
#define BIMP_GROW_CHUNK 4
#define ASSIGN_NONE 0
#define ASSIGN_TRUE 1
#define ASSIGN_FALSE 2

struct VarScore {
    int var = 0;
    int pos = 0;
    int neg = 0;
    double score = 0.0;
};

class BeamLookahead {
private:
    int m;
    int n_vars, n_clauses;
    int current_stamp = 1;
    std::vector<int> clause_literals;
    std::vector<int> clause_idx;
    std::vector<int> clause_stamp;

    std::vector<int*> bimp;
    std::vector<int> bimp_size;
    std::vector<int> bimp_capacity;
    
    int* global_queue = 0;
    uint8_t* assignments = 0;
    uint8_t* is_unit_var = 0;

    inline int lit_index(int lit);
    inline void reset_assignments();
    inline bool is_preselected(int var);
    void add_bimp(int lit, int implied);
    int propagate_bimp(int lit, std::vector<int>& propagated);
    int propagate_big_clauses(std::vector<int>& propagated);
    int propagate_with_assumptions(const std::vector<int>& assumptions, std::vector<int>& propagated);
    int formula_score(const std::vector<int>& assumptions);
    std::vector<VarScore> rank_all_vars(int max_var, const std::vector<int>& base_assumptions);
    VarScore score_variable_under_base(int var, const std::vector<int>& base_assumptions);
    void parse_cnf(const char *filename);

public:
    int cubing_var;
    void setup(int order, const char *infile);
    void lookahead();
    int write_cubes();
    ~BeamLookahead();
};

#endif