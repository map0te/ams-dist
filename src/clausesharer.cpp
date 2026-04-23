#include <cassert>

#include "internal.hpp"

#include "clausesharer.hpp"
#include "def.hpp"

#define MIN_SEND_SIZE 1024
#define MAX_CLAUSE_SIZE (15 + 1)
#define BUFSIZE (MIN_SEND_SIZE + 2 * MAX_CLAUSE_SIZE)

ClauseSharer::ClauseSharer (MPI_Comm comm) : comm(comm) {
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    total_imported_cas_literals = 0;
    total_imported_literals = 0;

    using_export_buffer_1 = true;
    using_cas_export_buffer_1 = true;

    import_buffer = new int [size * BUFSIZE];
    export_buffer_1 = new int [BUFSIZE];
    export_buffer_2 = new int [BUFSIZE];

    import_buffer_size = 0;
    export_buffer_1_size = 0;
    export_buffer_2_size = 0;

    cas_import_buffer.reserve(size * BUFSIZE);
    cas_export_buffer_1.reserve(BUFSIZE);
    cas_export_buffer_2.reserve(BUFSIZE);

    req1 = new MPI_Request [size];
    req2 = new MPI_Request [size];
    cas_req1 = new MPI_Request [size];
    cas_req2 = new MPI_Request [size];

    for (int i = 0; i < size; i++) {
        req1[i] = MPI_REQUEST_NULL;
        req2[i] = MPI_REQUEST_NULL;
        cas_req1[i] = MPI_REQUEST_NULL;
        cas_req2[i] = MPI_REQUEST_NULL;
    }

    flag1 = true;
    flag2 = true;
    cas_flag1 = true;
    cas_flag2 = true;

    n_read_literals = 0;
    n_read_cas_literals = 0;
}

ClauseSharer::~ClauseSharer () {
    //printf("imported literals: shared %ld, cas %ld\n", total_imported_literals, total_imported_cas_literals); fflush(stdout);
    delete [] import_buffer;
    delete [] export_buffer_1;
    delete [] export_buffer_2;
    delete [] req1;
    delete [] req2;
    delete [] cas_req1;
    delete [] cas_req2; 
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
    if (!cas_flag1) {
        MPI_Testall(size, cas_req1, &cas_flag1, MPI_STATUS_IGNORE);
        if (cas_flag1) {
            MPI_Waitall(size, cas_req1, MPI_STATUS_IGNORE);
            cas_export_buffer_1.clear();
        }
    }
    if (!cas_flag2) {
        MPI_Testall(size, cas_req2, &cas_flag2, MPI_STATUS_IGNORE);
        if (cas_flag2) {
            MPI_Waitall(size, cas_req2, MPI_STATUS_IGNORE);
            cas_export_buffer_2.clear();
        }
    }
    if (using_export_buffer_1) {
        if (flag2 && export_buffer_1_size >= MIN_SEND_SIZE) {
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
            for (int dst = 0; dst < size; dst++) {
                if (dst == rank) continue;
                MPI_Isend(export_buffer_2, export_buffer_2_size, MPI_INT, 
                    dst, M_CLAUSES, comm, &req2[dst]);
            }
            flag2 = false;
            using_export_buffer_1 = true;
        }
    }
    if (using_cas_export_buffer_1) {
        if (cas_flag2 && !cas_export_buffer_1.empty()) {
            for (int dst = 0; dst < size; dst++) {
                if (dst == rank) continue;
                MPI_Isend(
                    cas_export_buffer_1.data(),
                    cas_export_buffer_1.size(),
                    MPI_INT,
                    dst,
                    M_CASCLAUSES,
                    comm,
                    &cas_req1[dst]
                );
            }
            cas_flag1 = false;
            using_cas_export_buffer_1 = false;
        }
    } else {
        if (cas_flag1 && !cas_export_buffer_2.empty()) {
            for (int dst = 0; dst < size; dst++) {
                if (dst == rank) continue;
                MPI_Isend(
                    cas_export_buffer_2.data(),
                    cas_export_buffer_2.size(),
                    MPI_INT,
                    dst,
                    M_CASCLAUSES,
                    comm,
                    &cas_req2[dst]
                );
            }
            cas_flag2 = false;
            using_cas_export_buffer_1 = true;
        }
    }
}

void ClauseSharer::import_clauses () {
    int flag;
    int count;
    MPI_Status status;
    // cas clauses
    cas_import_buffer.clear();
    MPI_Iprobe(MPI_ANY_SOURCE, M_CASCLAUSES, comm, &flag, &status);
    while (flag) {
        MPI_Get_count(&status, MPI_INT, &count);
        int old_buffer_size = cas_import_buffer.size();
        cas_import_buffer.resize(count + old_buffer_size);
        MPI_Recv(
            cas_import_buffer.data() + old_buffer_size,
            count,
            MPI_INT,
            status.MPI_SOURCE,
            M_CASCLAUSES,
            comm,
            MPI_STATUS_IGNORE
        );
        MPI_Iprobe(MPI_ANY_SOURCE, M_CASCLAUSES, comm, &flag, &status);
    }
    // conflict clauses
    import_buffer_size = 0;
    while (import_buffer_size < (size-1) * BUFSIZE) {
        MPI_Iprobe(MPI_ANY_SOURCE, M_CLAUSES, comm, &flag, &status);
        if (!flag) break;
        MPI_Get_count(&status, MPI_INT, &count);
        assert (count < BUFSIZE);
        MPI_Recv(import_buffer + import_buffer_size, count, MPI_INT, 
            status.MPI_SOURCE, M_CLAUSES, comm, MPI_STATUS_IGNORE);
        import_buffer_size += count;
    }

    total_imported_literals += n_read_literals;
    total_imported_cas_literals += n_read_cas_literals;
    n_read_literals = 0;
    n_read_cas_literals = 0;
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

void ClauseSharer::learn_cas_clause (const std::vector<int>& cas_clause) {
    for (int lit : cas_clause) {
        if (using_cas_export_buffer_1) {
            cas_export_buffer_1.push_back(lit);
        } else {
            cas_export_buffer_2.push_back(lit);
        }
    }
    if (using_cas_export_buffer_1) {
        cas_export_buffer_1.push_back(0);
    } else {
        cas_export_buffer_2.push_back(0);   
    }
}

bool ClauseSharer::cb_has_external_clause () {
    if (cas_import_buffer.size() - n_read_cas_literals) {
        return true;
    } 
    if (import_buffer_size - n_read_literals) {
        return true;
    }
    export_clauses ();
    import_clauses ();
    return false;
}

int ClauseSharer::cb_add_external_clause_lit () {
    if (cas_import_buffer.size() - n_read_cas_literals) {
        return cas_import_buffer[n_read_cas_literals++];
    }
    return import_buffer[n_read_literals++];
}

void ClauseSharer::cleanup () {
    // cleans up any outstanding MPI_Isend 
    MPI_Request req;
    int num_completed = 0;
    int count, clauses_flag;
    MPI_Status status;
    for (int dst = 0; dst < size; dst++) {
        if (dst == rank) continue;
        MPI_Isend(NULL, 0, MPI_INT, dst, M_CLAUSES, comm, &req);
        MPI_Request_free(&req);
        MPI_Isend(NULL, 0, MPI_INT, dst, M_CASCLAUSES, comm, &req);
        MPI_Request_free(&req);
    }
    while (num_completed < 2 * (size - 1)) {
        MPI_Iprobe(MPI_ANY_SOURCE, M_CASCLAUSES, comm, &clauses_flag, &status);
        if (clauses_flag) {
            MPI_Get_count(&status, MPI_INT, &count);
            if (count == 0) { num_completed++; }
            cas_import_buffer.resize(count);
            MPI_Recv(cas_import_buffer.data(), count, MPI_INT, 
                status.MPI_SOURCE, M_CASCLAUSES, comm, MPI_STATUS_IGNORE);
        }
        MPI_Iprobe(MPI_ANY_SOURCE, M_CLAUSES, comm, &clauses_flag, &status);
        if (clauses_flag) {
            MPI_Get_count(&status, MPI_INT, &count);
            if (count == 0) { num_completed++; }
            MPI_Recv(import_buffer, count, MPI_INT,
                status.MPI_SOURCE, M_CLAUSES, comm, MPI_STATUS_IGNORE);
        }
    }
    MPI_Waitall(size, req1, MPI_STATUS_IGNORE);
    MPI_Waitall(size, req2, MPI_STATUS_IGNORE);
    MPI_Waitall(size, cas_req1, MPI_STATUS_IGNORE);
    MPI_Waitall(size, cas_req2, MPI_STATUS_IGNORE);
}