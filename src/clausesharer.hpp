#ifndef CLAUSESHARER_HPP
#define CLAUSESHARER_HPP

#include <chrono>
#include <cstdint>
#include <mpi.h>
#include <vector>

#include "internal.hpp"

class ClauseSharer : public CaDiCaL::Learner {
    MPI_Comm comm;
    int rank, size;

    // double buffering for conflict clauses
    bool using_export_buffer_1;
    int* import_buffer;
    int* export_buffer_1;
    int* export_buffer_2;
    int import_buffer_size;
    int export_buffer_1_size;
    int export_buffer_2_size;
    MPI_Request* req1;
    MPI_Request* req2;
    int flag1, flag2;
    int n_read_literals;

    // double buffering for cas clauses
    bool using_cas_export_buffer_1;
    std::vector<int> cas_import_buffer;
    std::vector<int> cas_export_buffer_1;
    std::vector<int> cas_export_buffer_2;
    MPI_Request* cas_req1;
    MPI_Request* cas_req2;
    int cas_flag1, cas_flag2;
    int n_read_cas_literals;

    // bloom filter for clause deduplication
    static constexpr size_t BLOOM_M = 1u << 25; // 32M bits, ~4MB
    static constexpr int    BLOOM_K = 7;
    uint64_t* bloom_bits;

    static uint64_t bloom_hash (const int* lits, int n, uint64_t seed);
    void bloom_add (const int* lits, int n);
    bool bloom_contains (const int* lits, int n) const;

    // staging buffer for clause currently being learned
    std::vector<int> current_clause;

    // timing accumulators
    double t_testall = 0.0;
    double t_iprobe  = 0.0;
    long   n_testall = 0;
    long   n_iprobe  = 0;

    void export_clauses ();
    void import_clauses ();
public:
    ClauseSharer (MPI_Comm comm);
    ~ClauseSharer ();
    bool learning (int size);
    void learn (int lit);
    bool cb_has_external_clause ();
    int cb_add_external_clause_lit ();
    void learn_cas_clause (const std::vector<int>& cas_clause);
    void cleanup ();
};

#endif