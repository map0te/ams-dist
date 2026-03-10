#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <fstream>

#include <mpi.h>

#include "beamlookahead.hpp"
#include "def.hpp"

BeamLookahead::~BeamLookahead() {
    if (assignments == 0) { return; }
}

inline int BeamLookahead::lit_index(int lit) {
    return (lit > 0) ? lit : n_vars - lit;
}

inline void BeamLookahead::reset_assignments() {
    memset(assignments, ASSIGN_NONE, sizeof(uint8_t) * (n_vars + 1));
    current_stamp++;
}

inline bool BeamLookahead::is_preselected(int var) {
    if (is_unit_var[var]) return false;  // Exclude unit clause variables
    return bimp_size[lit_index(var)] > 0 || bimp_size[lit_index(-var)] > 0;
}

inline bool BeamLookahead::is_lit_true(int lit) {
    int var = abs(lit);
    if (assignments[var] == ASSIGN_NONE) return false;
    return (lit > 0 && assignments[var] == ASSIGN_TRUE) ||
           (lit < 0 && assignments[var] == ASSIGN_FALSE);
}

inline bool BeamLookahead::is_lit_false(int lit) {
    int var = abs(lit);
    if (assignments[var] == ASSIGN_NONE) return false;
    return (lit > 0 && assignments[var] == ASSIGN_FALSE) ||
           (lit < 0 && assignments[var] == ASSIGN_TRUE);
}

void BeamLookahead::add_bimp(int lit, int implied) {
    int idx = lit_index(lit);
    if (bimp_capacity[idx] == 0) {
        bimp_capacity[idx] = BIMP_GROW_CHUNK;
        bimp[idx] = (int*)malloc(sizeof(int) * bimp_capacity[idx]);
    } else if (bimp_size[idx] >= bimp_capacity[idx]) {
        bimp_capacity[idx] *= 2;
        bimp[idx] = (int*)realloc(bimp[idx], sizeof(int) * bimp_capacity[idx]);
    }
    bimp[idx][bimp_size[idx]++] = implied;
}

void BeamLookahead::parse_cnf(const char *filename) {
    MPI_File fh;
    MPI_Offset file_size;
    MPI_Offset header_offset = 0;
    MPI_Status status;
    int rank, size;
    int err;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    err = MPI_File_open(comm, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
    if (err) {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MPI_File_get_size(fh, &file_size);

    char header[HEADER_BUF];

    // Read cnf header
    if (rank == 0) {
        MPI_File_read_at(fh, 0, header, HEADER_BUF, MPI_CHAR, &status);
        for(int i = 0; i < HEADER_BUF; i++) {
            if (header[i] == '\n') {
                header_offset = i + 1;
                header[i+1] = '\0';
                break;
            }
        }
        sscanf(header, "p cnf %d %d", &n_vars, &n_clauses);
    }
    MPI_Bcast(&n_clauses, 1, MPI_INT, 0, comm);
    MPI_Bcast(&n_vars, 1, MPI_INT, 0, comm);
    MPI_Bcast(&header_offset, 1, MPI_OFFSET, 0, comm);

    // Parse integers into literal vector
    MPI_File_get_size(fh, &file_size);
    MPI_Offset data_size = file_size - header_offset;
    MPI_Offset chunk = data_size / size;
    MPI_Offset start = header_offset + rank * chunk;
    MPI_Offset end = header_offset + (rank + 1) * chunk;
    MPI_Offset read_size;
    if (rank == size - 1) {
        end = file_size;
        read_size = end - start;
    } else {
        read_size = end - start + std::log10(n_vars) + 2;
    }
    char *buffer = new char[read_size + 1];
    MPI_File_read_at_all(fh, start, buffer, read_size, MPI_CHAR, &status);
    buffer[read_size] = '\0';

    char *local_start = buffer;
    char *local_end = buffer + (end - start);

    if (rank != 0) {
        while (*local_start != '\n' && *local_start != '\0' && *local_start != ' ')
            local_start++;
        local_start++;
    }

    if (rank != size - 1) {
        while (*local_end != '\n' && *local_end != '\0' && *local_end != ' ')
            local_end++;
        *local_end = '\0';
    }

    std::vector<int> local_clause_literals;
    std::vector<unsigned long> local_clause_idx;

    char *token = strtok(local_start, " \n\t");
    unsigned long idx = 0;
    if (rank == 0) { local_clause_idx.push_back(0); }
    while (token != NULL) {
        int val = atoi(token);
        if (val != 0) {
            local_clause_literals.push_back(val);
            idx++;
        } else {
            local_clause_idx.push_back(idx);
        }
        token = strtok(NULL, " \n\t");
    }

    // Gather literals
    int local_literals_read = local_clause_literals.size();
    int total_literals_read = 0;
    int * literals_read = new int [size];
    MPI_Allgather(&local_literals_read, 1, MPI_INT, literals_read, 1, MPI_INT, comm);

    int * literals_read_offsets = new int [size];
    literals_read_offsets[0] = 0;
    for (int i = 0; i < size; i++) { 
        total_literals_read += literals_read[i]; 
    }
    for (int i = 1; i < size; i++) { 
        literals_read_offsets[i] = literals_read[i-1] + literals_read_offsets[i-1]; 
    }
    clause_literals.resize(total_literals_read);
    MPI_Allgatherv(local_clause_literals.data(), local_literals_read, MPI_INT, 
        clause_literals.data(), literals_read, literals_read_offsets, MPI_INT, comm);

    // Add offset to clause idx
    for (int i = 0; i < local_clause_idx.size(); i++) {
        local_clause_idx[i] += literals_read_offsets[rank];
    }

    // Gather clause offsets
    int local_clause_idx_read = local_clause_idx.size();
    int total_clause_idx_read = 0;
    int * clause_idx_read = new int [size];
    MPI_Allgather(&local_clause_idx_read, 1, MPI_INT, clause_idx_read, 1, MPI_INT, comm);

    int * clause_idx_read_offsets = new int [size];
    clause_idx_read_offsets[0] = 0;
    for (int i = 0; i < size; i++) { 
        total_clause_idx_read += clause_idx_read[i]; 
    }
    for (int i = 1; i < size; i++) { 
        clause_idx_read_offsets[i] = clause_idx_read[i-1] + clause_idx_read_offsets[i-1]; 
    }

    clause_idx.resize(total_clause_idx_read);

    MPI_Allgatherv(local_clause_idx.data(), local_clause_idx_read, MPI_UNSIGNED_LONG,
        clause_idx.data(), clause_idx_read, clause_idx_read_offsets, MPI_UNSIGNED_LONG, comm);

    delete[] clause_idx_read;
    delete[] clause_idx_read_offsets;
    delete[] literals_read;
    delete[] literals_read_offsets;
    delete[] buffer;
    MPI_File_close(&fh);

    // handle unit and binary implication clauses
    bimp.resize(n_vars * 2 + 2);
    bimp_size.resize(n_vars * 2 + 2);
    bimp_capacity.resize(n_vars * 2 + 2);

    watched.resize(n_clauses);
    watch_list.resize(n_vars * 2 + 2);
    global_queue = new int [n_vars + 1]();
    assignments = new uint8_t [n_vars + 1]();
    is_unit_var = new uint8_t [n_vars + 1]();

    for (int i = 1; i < clause_idx.size(); i++) {
        if (clause_idx[i] - clause_idx[i-1] == 1) {
            is_unit_var[abs(clause_literals[clause_idx[i-1]])] = 1;
        } else if (clause_idx[i] - clause_idx[i-1] == 2) {
            add_bimp(-clause_literals[clause_idx[i-1]], clause_literals[clause_idx[i-1]+1]);
            add_bimp(-clause_literals[clause_idx[i-1]+1], clause_literals[clause_idx[i-1]]);
        } else {
            watched[i-1].first = 0;
            watched[i-1].second = 1;
            watch_list[lit_index(clause_literals[clause_idx[i-i]])].push_back(i-1);
            watch_list[lit_index(clause_literals[clause_idx[i-1]+1])].push_back(i-1);
        }
    }
}

int BeamLookahead::propagate_bimp(int lit, std::vector<int>& propagated) {
    int front = 0, rear = 0;
    global_queue[rear++] = lit;

    while (front < rear) {
        int l = global_queue[front++];
        int var = abs(l);

        if (assignments[var] != ASSIGN_NONE) {
            if ((assignments[var] == ASSIGN_TRUE && l < 0) ||
                (assignments[var] == ASSIGN_FALSE && l > 0)) {
                return 0;
            }
            continue;
        }

        assignments[var] = (l > 0) ? ASSIGN_TRUE : ASSIGN_FALSE;
        propagated.push_back(var);

        int idx = lit_index(l);
        for (int i = 0; i < bimp_size[idx]; i++) global_queue[rear++] = bimp[idx][i];
    }
    return 1;
}

int BeamLookahead::update_watches_and_propagate(int false_lit, std::vector<int>& propagated, int& queue_rear) {
    int lit_idx = lit_index(false_lit);
    std::vector<int>& watches = watch_list[lit_idx];
    // Process all clauses watching this literal
    for (size_t i = 0; i < watches.size(); ) {
        int c_idx = watches[i];
        int size = clause_idx[c_idx+1] - clause_idx[c_idx];
        if (size <= 2) { i++; continue; }
        // Find which watch position contains the false literal
        int watch_pos = -1;
        if (clause_literals[clause_idx[c_idx] + watched[c_idx].first] == false_lit) {
            watch_pos = 0;
        } else if (clause_literals[clause_idx[c_idx] + watched[c_idx].second] == false_lit) {
            watch_pos = 1;
        } else {
            // This literal is no longer watched, skip
            i++;
            continue;
        }

        int other_watch_lit;
        if (watch_pos == 0) {
            other_watch_lit = clause_literals[clause_idx[c_idx] + watched[c_idx].second];
        } else {
            other_watch_lit = clause_literals[clause_idx[c_idx] + watched[c_idx].first];
        }

        if (is_lit_true(other_watch_lit)) {
            i++;
            continue;
        }

        bool found_new_watch = false;
        for (int j = 0; j < size; j++) {
            if (j == watched[c_idx].first || j == watched[c_idx].second) continue;
            
            int lit = clause_literals[clause_idx[c_idx] + j];
            if (!is_lit_false(lit)) {
                // Found a new literal to watch
                // Remove from current watch list
                watches[i] = watches.back();
                watches.pop_back();
                
                // Update watch and add to new watch list
                if (watch_pos == 0) {
                    watched[c_idx].first = j;
                } else {
                    watched[c_idx].second = j;
                }
                watch_list[lit_index(lit)].push_back(c_idx);
                found_new_watch = true;
                break;
            }
        }
        if (!found_new_watch) {
            // Could not find new watch, check clause status
            if (is_lit_false(other_watch_lit)) {
                // Both watches are false - conflict
                return 0;
            } else {
                // Other watch must be unassigned - unit clause
                int var = abs(other_watch_lit);
                if (assignments[var] == ASSIGN_NONE) {
                    assignments[var] = (other_watch_lit > 0) ? ASSIGN_TRUE : ASSIGN_FALSE;
                    propagated.push_back(var);
                    global_queue[queue_rear++] = other_watch_lit;
                }
            }
            i++;
        }
    }
    return 1;
}

int BeamLookahead::propagate_with_assumptions(const std::vector<int>& assumptions, std::vector<int>& propagated) {
    reset_assignments();
    propagated.clear();

    int front = 0, rear = 0;

    // Add all assumption to the queue
    for (int lit : assumptions) {
        int var = abs(lit);
        if (assignments[var] != ASSIGN_NONE) {
            if ((assignments[var] == ASSIGN_TRUE && lit < 0) ||
                (assignments[var] == ASSIGN_FALSE && lit > 0)) {
                return 0;
            }
            continue;
        }
        assignments[var] = (lit > 0) ? ASSIGN_TRUE : ASSIGN_FALSE;
        propagated.push_back(var);
        global_queue[rear++] = lit;
    }

    while (front < rear) {
        int lit = global_queue[front++];
        
        // Propagate binary implications
        int idx = lit_index(lit);
        for (int i = 0; i < bimp_size[idx]; i++) {
            int implied = bimp[idx][i];
            int var = abs(implied);
            
            if (assignments[var] != ASSIGN_NONE) {
                if ((assignments[var] == ASSIGN_TRUE && implied < 0) ||
                    (assignments[var] == ASSIGN_FALSE && implied > 0)) {
                    return 0;
                }
                continue;
            }
            
            assignments[var] = (implied > 0) ? ASSIGN_TRUE : ASSIGN_FALSE;
            propagated.push_back(var);
            global_queue[rear++] = implied;
        }
        
        // Update watches for the negation of the assigned literal
        int false_lit = -lit;
        if (!update_watches_and_propagate(false_lit, propagated, rear)) {
            return 0;
        }
    }
    return 1;
}

int BeamLookahead::formula_score(const std::vector<int>& assumptions) {
    std::vector<int> propagated;
    if (!propagate_with_assumptions(assumptions, propagated)) return 0;
    return (int)propagated.size();
}

VarScore BeamLookahead::score_variable_under_base(int var, const std::vector<int>& base_assumptions) {
    std::vector<int> plus = base_assumptions;
    plus.push_back(var);
    int pos = formula_score(plus);

    std::vector<int> minus = base_assumptions;
    minus.push_back(-var);
    int neg = formula_score(minus);

    double score = (double)pos * (double)neg + (double)pos + (double)neg;
    return {var, pos, neg, score};
}

std::vector<VarScore> BeamLookahead::rank_all_vars(int max_var, const std::vector<int>& base_assumptions) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    int start_var = rank * (max_var/size);
    int end_var = (rank + 1) * (max_var/size);
    if (rank < max_var % size) {
        start_var += rank;
        end_var += rank + 1;
    } else {
        start_var += max_var % size;
        end_var += max_var % size;
    }
    if (end_var > max_var + 1) { end_var = max_var + 1; }

    std::vector<VarScore> local_out;
    std::vector<VarScore> out;
    for (int v = start_var + 1; v < end_var + 1; ++v) {
        if (!is_preselected(v)) continue;
        local_out.push_back(score_variable_under_base(v, base_assumptions));
    }
    int local_count = local_out.size();
    int* counts = new int [size];
    int* offsets = new int [size]();
    MPI_Allgather(&local_count, 1, MPI_INT, counts, 1, MPI_INT, comm);
    for (int i = 1; i < size; i++) {
        offsets[i] = offsets[i-1] + counts[i-1];
    }
    out.resize(offsets[size-1] + counts[size-1]);
    MPI_Allgatherv(local_out.data(), local_count, MPI_VARSCORE, out.data(), counts, offsets, MPI_VARSCORE, comm);
    std::sort(out.begin(), out.end(), [](const VarScore& a, const VarScore& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.var < b.var;
    });
    delete[] counts;
    delete[] offsets;
    return out;
}

struct UpdatedSeedScore {
    int seed_var = 0;
    double s1 = 0.0;
    double s2_balanced = 0.0;
    double s1_norm = 0.0;
    double s2_norm = 0.0;
    double updated_score = 0.0;
};

void BeamLookahead::setup(int order, const char* infile, MPI_Comm comm) {
    this->comm = comm;
    m = (order * (order - 1)) / 2;
    parse_cnf(infile);
    if (m == -1 || m > n_vars) m = n_vars;
}

void BeamLookahead::lookahead() {
    std::vector<int> free_vars;
    for (int v = 1; v <= m; ++v) {
        if (is_preselected(v)) {
            free_vars.push_back(v);
        }
    }
    const double lambda = 0.5;
    const double gamma = 0.2;

    auto ranked = rank_all_vars(m, {});
    std::vector<int> top3;
    for (int i = 0; i < 3 && i < (int)ranked.size(); ++i) top3.push_back(ranked[i].var);

    std::vector<UpdatedSeedScore> updated;
    for (int x : top3) {
        VarScore s1 = score_variable_under_base(x, {});
        double s2_plus = 0.0, s2_minus = 0.0;

        for (int sign : {+1, -1}) {
            int lit = sign * x;
            std::vector<int> base = {lit};
            auto rescored = rank_all_vars(m, base);

            int partner = x;
            for (const auto& rs : rescored) {
                if (rs.var != x) {
                    partner = rs.var;
                    break;
                }
            }

            VarScore partner_score = score_variable_under_base(partner, {lit});
            if (sign > 0) s2_plus = partner_score.score;
            else s2_minus = partner_score.score;
        }
        double s2_mean = (s2_plus + s2_minus) / 2.0;
        double s2_balanced = s2_mean - gamma * fabs(s2_plus - s2_minus);
        updated.push_back({x, s1.score, s2_balanced, 0.0, 0.0, 0.0});
    }

    double max_s1 = 1e-9, max_s2 = 1e-9;
    for (const auto& u : updated) {
        max_s1 = std::max(max_s1, u.s1);
        max_s2 = std::max(max_s2, u.s2_balanced);
    }

    for (auto& u : updated) {
        u.s1_norm = u.s1 / max_s1;
        u.s2_norm = u.s2_balanced / max_s2;
        u.updated_score = (1.0 - lambda) * u.s1_norm + lambda * u.s2_norm;
    }

    std::sort(updated.begin(), updated.end(), [](const UpdatedSeedScore& a, const UpdatedSeedScore& b) {
        if (a.updated_score != b.updated_score) return a.updated_score > b.updated_score;
        return a.seed_var < b.seed_var;
    });

    cubing_var = updated[0].seed_var;
}

int BeamLookahead::write_cubes(const char* infile, const char* outfile1, const char* outfile2) {
    int rank;
    MPI_Comm_rank(comm, &rank);
    for (int i = 0; i < (int) bimp.size(); i++) {
        if (bimp_size[i]) {
            free(bimp[i]);
        }
    }
    std::fill(bimp.begin(), bimp.end(), (int*) 0);
    std::fill(bimp_size.begin(), bimp_size.end(), 0);
    std::fill(bimp_capacity.begin(), bimp_capacity.end(), 0);

    delete[] global_queue;
    delete[] assignments;
    delete[] is_unit_var;
    if (rank == 0) {
        FILE *in_ptr = fopen(infile, "r");
        FILE *out1_ptr = fopen(outfile1, "w");
        FILE *out2_ptr = fopen(outfile2, "w");
        fscanf(in_ptr, "p cnf %*d %*d\n");
        fprintf(out1_ptr, "p cnf %d %d\n", n_vars, n_clauses+1);
        fprintf(out2_ptr, "p cnf %d %d\n", n_vars, n_clauses+1);
        fprintf(out1_ptr, "%d 0\n", cubing_var);
        fprintf(out2_ptr, "-%d 0\n", cubing_var);
        int readlen = 16777216;
        char buf[readlen];
        int read = 0;
        while ((read = fread(buf, 1, readlen, in_ptr)) == readlen){
            fwrite(buf, 1, readlen, out1_ptr);
            fwrite(buf, 1, readlen, out2_ptr);
        }
        fwrite(buf, 1, read, out1_ptr);
        fwrite(buf, 1, read, out2_ptr);
        fclose(in_ptr);
        fclose(out1_ptr);
        fclose(out2_ptr);
    }
    return 0;
}
/*

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    BeamLookahead BL;
    bool debug = false;
    std::string filename;
    std::string out_file1;
    std::string out_file2;
    int m = -1;

    MPI_Datatype types[2] = { MPI_INT, MPI_DOUBLE };
    int blocklen[2] = {3, 1};

    MPI_Aint offsets[2];
    offsets[0] = offsetof(struct VarScore, var);
    offsets[1] = offsetof(struct VarScore, score);

    MPI_Type_create_struct(2, blocklen, offsets, types, &MPI_VARSCORE);
    MPI_Type_commit(&MPI_VARSCORE);

    if (argc < 2) {
        printf("Usage: %s <cnf-file> -o <cube-file> [-m M] [-debug]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc) {
            m = atoi(argv[++i]);
        } else if (arg == "-o1" && i + 1 < argc) {
            out_file1 = argv[++i];
        } else if (arg == "-o2" && i + 1 < argc) {
            out_file2 = argv[++i];
        } else if (arg == "-debug") {
            debug = true;
        } else if (!arg.empty() && arg[0] != '-') {
            filename = arg;
        }
    }

    if (filename.empty() || out_file1.empty() || out_file2.empty()) {
        if (rank == 0) {
            printf("Usage: %s <cnf-file> -o <cube-file> [-m M] [-debug]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    auto time_start = std::chrono::high_resolution_clock::now();
    BL.setup(m, filename.c_str(), MPI_COMM_WORLD);
    BL.lookahead();
    BL.write_cubes(filename.c_str(), out_file1.c_str(), out_file2.c_str());
    double total_time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - time_start).count();

    if (rank == 0) { printf("Total runtime: %.3f\n", total_time); }
    if (rank == 0) { printf("Computed cubing variable: %d\n", BL.cubing_var); }

    MPI_Finalize();
    return 0;
}*/
