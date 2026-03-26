#include <mpi.h>

class ClauseSharer {
public:
    ClauseSharer (MPI_Comm comm) { return; };
    bool cb_has_external_clause () { return false; };
    int cb_add_external_clause_lit () { return 0; };
    bool learning (int size) { return false; };
    void learn (int lit) { return; };
};