#include "clausesharer.hpp"
#include "def.hpp"

#define MIN_SEND_SIZE 1024
#define MAX_CLAUSE_SIZE 11
#define BUFSIZE MIN_SEND_SIZE + 2 * MAX_CLAUSE_SIZE

ClauseSharer::ClauseSharer (MPI_Comm comm) : comm(comm) {
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    using_export_buffer_1 = true;

    import_buffer = new int [size * BUFSIZE];
    export_buffer_1 = new int [BUFSIZE];
    export_buffer_2 = new int [BUFSIZE];

    import_buffer_size = 0;
    export_buffer_1_size = 0;
    export_buffer_2_size = 0;

    req1 = new MPI_Request [size];
    req2 = new MPI_Request [size];

    for (int i = 0; i < size; i++) {
        req1[i] = MPI_REQUEST_NULL;
        req2[i] = MPI_REQUEST_NULL;
    }

    flag1 = true;
    flag2 = true;
    n_read_literals = 0;
}

ClauseSharer::~ClauseSharer () {
    delete [] import_buffer;
    delete [] export_buffer_1;
    delete [] export_buffer_2;
    delete [] req1;
    delete [] req2;
}

void ClauseSharer::export_clauses () {
    if (!flag1) {
        MPI_Testall(size, req1, &flag1, MPI_STATUS_IGNORE);
        if (flag1) {
            MPI_Waitall(size, req1, MPI_STATUS_IGNORE);
            export_buffer_1_size = 0;
        }
    }
    if (!flag2) {
        MPI_Testall(size, req2, &flag2, MPI_STATUS_IGNORE);
        if (flag2) {
            MPI_Waitall(size, req2, MPI_STATUS_IGNORE);   
            export_buffer_2_size = 0;
        }
    }
    if (using_export_buffer_1) {
        if (flag2 && export_buffer_1_size >= MIN_SEND_SIZE) {
            //printf("rank %d exporting clause buffer of size %d\n", rank, export_buffer_1_size); fflush(stdout);
            for (int dst = 0; dst < size; dst++) {
                if (dst == rank) continue;
                MPI_Isend(export_buffer_1, export_buffer_1_size, MPI_INT, 
                    dst, M_CLAUSES, comm, &req1[dst]);
            }
            flag1 = false;
            using_export_buffer_1 = false;
        }
    } else {
        if (flag1 && export_buffer_2_size >= MIN_SEND_SIZE) {
            //printf("rank %d exporting clause buffer of size %d\n", rank, export_buffer_2_size); fflush(stdout);
            for (int dst = 0; dst < size; dst++) {
                if (dst == rank) continue;
                MPI_Isend(export_buffer_2, export_buffer_2_size, MPI_INT, 
                    dst, M_CLAUSES, comm, &req2[dst]);
            }
            flag2 = false;
            using_export_buffer_1 = true;
        }
    }
}

void ClauseSharer::import_clauses () {
    import_buffer_size = 0;
    n_read_literals = 0;
    int flag, count;
    MPI_Status status;
    while (import_buffer_size < (size-1) * BUFSIZE) {
        MPI_Iprobe(MPI_ANY_SOURCE, M_CLAUSES, comm, &flag, &status);
        if (!flag) break;
        MPI_Get_count(&status, MPI_INT, &count);
        MPI_Recv(import_buffer + import_buffer_size, count, MPI_INT, 
            status.MPI_SOURCE, M_CLAUSES, comm, MPI_STATUS_IGNORE);
        import_buffer_size += count;
    }
}

bool ClauseSharer::learning (int size) {
    if (using_export_buffer_1 && export_buffer_1_size > MIN_SEND_SIZE) return false;
    if (!using_export_buffer_1 && export_buffer_2_size > MIN_SEND_SIZE) return false;
    return size <= MAX_CLAUSE_SIZE - 1;
}

void ClauseSharer::learn (int lit) {
    if (using_export_buffer_1) {
        export_buffer_1[export_buffer_1_size++] = lit;
    } else {
        export_buffer_2[export_buffer_2_size++] = lit;
    }
}

bool ClauseSharer::cb_has_external_clause () {
    if (import_buffer_size - n_read_literals) {
        return true;
    }
    export_clauses ();
    import_clauses ();
    return false;
}

int ClauseSharer::cb_add_external_clause_lit () {
    return import_buffer[n_read_literals++];
}

void ClauseSharer::share_cas_clause (std::vector<int>& clause) {
    if (using_export_buffer_1) {
        return;
    }
}

void ClauseSharer::cleanup () {
    int count, flag;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, M_CLAUSES, comm, &flag, &status);
    while (flag) {
        MPI_Get_count(&status, MPI_INT, &count);
        MPI_Recv(import_buffer, count, MPI_INT, 
            status.MPI_SOURCE, M_CLAUSES, comm, MPI_STATUS_IGNORE);
        MPI_Iprobe(MPI_ANY_SOURCE, M_CLAUSES, comm, &flag, &status);
    } 
}