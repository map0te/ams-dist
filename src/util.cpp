#include "internal.hpp"

#include <vector>
#include <fstream>

#include <mpi.h>

#include "util.hpp"

inline void append_solutions(std::vector<std::vector<int>>& solutions, std::vector<std::vector<int>>& new_solutions) {
    solutions.insert(solutions.end(), new_solutions.begin(), new_solutions.end());
}

void write_dimacs_with_units(CaDiCaL::Solver* solver, const std::string& path) {
    std::vector<int> units;
    for (int var = 1; var <= solver->vars(); var++) {
        int fix = solver->fixed(var);
        if (fix != 0) units.push_back(fix > 0 ? var : -var);
    }

    solver->write_dimacs(path.c_str(), solver->vars());

    if (units.empty()) return;

    FILE* f = fopen(path.c_str(), "r+");

    char line[64];
    fpos_t header_pos;
    while (true) {
        fgetpos(f, &header_pos);
        fgets(line, sizeof(line), f);
        if (line[0] == 'p') break;
    }

    int vars, clauses;
    sscanf(line, "p cnf %d %d", &vars, &clauses);
    int new_clauses = clauses + (int)units.size();

    char old_header[64], new_header[64];
    int old_len = snprintf(old_header, sizeof(old_header), "p cnf %d %d", vars, clauses);
    int new_len = snprintf(new_header, sizeof(new_header), "p cnf %d %d", vars, new_clauses);

    if (new_len <= old_len) {
        // Safe to patch in-place — pad to preserve byte offsets
        while (new_len < old_len) new_header[new_len++] = ' ';
        new_header[new_len] = '\0';
        fsetpos(f, &header_pos);
        fputs(new_header, f);
        fseek(f, 0, SEEK_END);
    } else {
        // Header grew — full rewrite from header position onward
        // Read everything after the header
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fpos_t body_pos;
        fgetpos(f, &body_pos); // end of header line
        // re-find body start
        fsetpos(f, &header_pos);
        fgets(line, sizeof(line), f); // skip old header
        long body_start = ftell(f);
        long body_size = file_size - body_start;
        std::vector<char> body(body_size);
        fread(body.data(), 1, body_size, f);

        fsetpos(f, &header_pos);
        fprintf(f, "%s\n", new_header);
        fwrite(body.data(), 1, body_size, f);
    }

    for (int lit : units) fprintf(f, "%d 0\n", lit);

    fclose(f);
}