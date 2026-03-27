#ifndef CLAUSESHARER_HPP
#define CLAUSESHARER_HPP

#include <mpi.h>
#include <vector>

class ClauseSharer {
    MPI_Comm comm;
    int rank, size;
    // double buffering for async message sending
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

    void export_clauses ();
    void import_clauses ();
public:
    ClauseSharer (MPI_Comm comm);
    ~ClauseSharer ();
    bool learning (int size);
    void learn (int lit);
    bool cb_has_external_clause ();
    int cb_add_external_clause_lit ();
    void share_cas_clause (std::vector<int>& clause);
    void cleanup ();
};

#endif