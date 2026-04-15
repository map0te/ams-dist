#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <iostream>

#include "symbreak.hpp"
#include "unembeddable_graphs.hpp"
#include "hash_values.hpp"
#include "internal.hpp"

SymmetryBreaker::SymmetryBreaker (int order) {
    n = order;
    num_edge_vars = n * (n-1) / 2;
    assign = new int[num_edge_vars];
    fixed = new bool[num_edge_vars];
    colsuntouched = new int[n];
    
    for (int i = 0; i < num_edge_vars; i++) {
        assign[i] = l_Undef;
        fixed[i] = false;
    }

    //TODO add edge variables for orderly generation

    notify_new_decision_level ();
}

SymmetryBreaker::~SymmetryBreaker () {
    delete [] assign;
    delete [] colsuntouched;
    delete [] fixed;
}

void SymmetryBreaker::notify_assignment(int lit, bool is_fixed) {
    assign[abs(lit)-1] = (lit > 0 ? l_True : l_False);
    if (is_fixed) {
        fixed[abs(lit)-1] = true;
    } else {
        current_trail.back().push_back(lit);
    }
}

void SymmetryBreaker::notify_new_decision_level () {
    current_trail.push_back(std::vector<int>());
}

void SymmetryBreaker::notify_backtrack (size_t new_level) {
    while (current_trail.size() > new_level + 1) {
        for (const auto& lit: current_trail.back()) {
            const int x = abs(lit) - 1;
            // Don't remove literals that have been fixed
            if(fixed[x])
                continue;
            assign[x] = l_Undef;
            const int col = 1+(-1+sqrt(1+8*x))/2;
            for(int i=col; i<n; i++)
                colsuntouched[i] = false;
        }
        current_trail.pop_back();
    }
}

std::vector<int> SymmetryBreaker::observed_vars () {
    std::vector<int> vars;
    for (int i = 0; i < num_edge_vars; i++) {
        vars.push_back(i+1);
    }
    return vars;
}

bool SymmetryBreaker::cb_check_found_model (const std::vector<int> & model) {
    assert((int) model.size() == num_edge_vars);
    std::vector<int> clause;
    std::vector<int> solution;

    for (const auto& lit: model) {
        if (lit > 0) {
            solution.push_back(lit);
        }
        clause.push_back(-lit);
    }
    fflush(stdout);

    cas_clauses.push_back(clause);
    _solutions.push_back(solution);
    return false;
}

bool SymmetryBreaker::cb_has_external_clause() {
    if (!cas_clauses.empty()) return true;
    if (build_noncanonicity_clause()) return true;
    return false;
}

int SymmetryBreaker::cb_add_external_clause_lit () {
    size_t clause_idx = cas_clauses.size() - 1;
    if (cas_clauses[clause_idx].empty()) {
        cas_clauses.pop_back();
        return 0;
    }

    int lit = cas_clauses[clause_idx].back();
    cas_clauses[clause_idx].pop_back();
    return lit;
}

bool SymmetryBreaker::build_noncanonicity_clause () {
    long hash = 0;
    // Initialize i to be the first column that has been touched since the last analysis
    int i = 2;
    for(; i < n; i++) {
        if(!colsuntouched[i])
            break;
    }
    // Ensure variables are defined and update current graph hash
    for(int j = 0; j < i*(i-1)/2; j++) {
        if(assign[j] == l_Undef)
            return false;
        else if(assign[j] == l_True)
            hash += hash_values[j];
    }
    for(; i < n; i++) {
        // Ensure variables are defined and update current graph hash
        for(int j = i*(i-1)/2; j < i*(i+1)/2; j++) {
            if(assign[j]==l_Undef) {
                return false;
            }
            if(assign[j]==l_True) {
                hash += hash_values[j];
            }
        }
        colsuntouched[i] = true;

        // Check if current graph hash has been seen
        if(canonical_hashes[i].find(hash)==canonical_hashes[i].end()) {
            // Found a new subgraph of order i+1 to test for canonicity
            // Uses a pseudo-check except when i+1 = n
            int p[i+1]; // Permutation on i+1 vertices
            int x, y;   // These will be the indices of first adjacency matrix entry that demonstrates noncanonicity (when such indices exist)
            int mi;     // This will be the index of the maximum defined entry of p
            bool ret = (hash == 0) ? true : is_canonical(i+1, p, x, y, mi, i < n-1);

            // If subgraph is canonical
            if (ret) {
                canonical_hashes[i].insert(hash);
            } else {
                // Generate a blocking clause smaller than the naive blocking clause
                cas_clauses.push_back(std::vector<int>());
                cas_clauses.back().push_back(-(x*(x-1)/2+y+1));
                const int px = MAX(p[x], p[y]);
                const int py = MIN(p[x], p[y]);
                cas_clauses.back().push_back(px*(px-1)/2+py+1);
                for(int ii=0; ii < x+1; ii++) {
                    for(int jj=0; jj < ii; jj++) {
                        if(ii==x && jj==y) {
                            break;
                        }
                        const int pii = MAX(p[ii], p[jj]);
                        const int pjj = MIN(p[ii], p[jj]);
                        if(ii==pii && jj==pjj) {
                            continue;
                        } else if(assign[ii*(ii-1)/2+jj] == l_True) {
                            cas_clauses.back().push_back(-(ii*(ii-1)/2+jj+1));
                        } else if (assign[pii*(pii-1)/2+pjj] == l_False) {
                            cas_clauses.back().push_back(pii*(pii-1)/2+pjj+1);
                        }
                    }
                }
                return true;
            }
        }
    }
    return false;
}

bool SymmetryBreaker::is_canonical(int k, int p[], int& x, int& y, int& i, bool opt_pseudo_test) {
    int pl[k]; // pl[k] contains the current list of possibilities for kth vertex (encoded bitwise)
    int pn[k+1]; // pn[k] contains the initial list of possibilities for kth vertex (encoded bitwise)
    pl[0] = (1 << k) - 1;
    pn[0] = (1 << k) - 1;
    i = 0;
    int last_x = 0;
    int last_y = 0;

    int np = 1;
    int limit = INT32_MAX;

    // If pseudo-test enabled then stop test if it is taking over 10 times longer than average
    if(opt_pseudo_test && k >= 7) {
        limit = 10*perm_cutoff[k-1];
    }

    while(np < limit) {
        // If no possibilities for ith vertex then backtrack
        if(pl[i]==0) {
            // Backtrack to vertex that has at least two possibilities
            while((pl[i] & (pl[i] - 1)) == 0) {
                if(last_x > p[i]) {
                    last_x = p[i];
                    last_y = 0;
                }
                i--;
                if(i==-1) {
                    // No permutations produce a smaller matrix; M is canonical
                    return true;
                }
            }
            // Remove p[i] as a possibility from the ith vertex
            pl[i] = pl[i] & ~(1 << p[i]);
        }

        p[i] = log2(pl[i] & -pl[i]); // Get index of rightmost high bit
        pn[i+1] = pn[i] & ~(1 << p[i]); // List of possibilities for (i+1)th vertex

        // If pseudo-test enabled then stop shortly after the first row is no longer fixed
        if(i == 0 && p[i] == 1 && opt_pseudo_test && k < n) {
            limit = np + 100;
        }

        // Check if the entry on which to begin lex-checking needs to be updated
        if(last_x > p[i]) {
            last_x = p[i];
            last_y = 0;
        }
        if(i == last_x) {
            last_y = 0;
        }

        // Determine if the permuted matrix p(M) is lex-smaller than M
        bool lex_result_unknown = false;
        x = last_x == 0 ? 1 : last_x;
        y = last_y;
        int j;
        for(j=last_x*(last_x-1)/2+last_y; j<k*(k-1)/2; j++) {
            if(x > i) {
                // Unknown if permutation produces a larger or smaller matrix
                lex_result_unknown = true;
                break;
            }
            const int px = MAX(p[x], p[y]);
            const int py = MIN(p[x], p[y]);
            const int pj = px*(px-1)/2 + py;
            if(assign[j] == l_False && assign[pj] == l_True) {
                // Permutation produces a larger matrix; stop considering
                break;
            }
            if(assign[j] == l_True && assign[pj] == l_False) {
                // Permutation produces a smaller matrix; M is not canonical
                return false;
            }

            y++;
            if(x==y) {
                x++;
                y = 0;
            }
        }
        last_x = x;
        last_y = y;

        if(lex_result_unknown) {
            // Lex result is unknown; need to define p[i] for another i
            i++;
            pl[i] = pn[i];
        }
        else {
            np++;
            // Remove p[i] as a possibility from the ith vertex
            pl[i] = pl[i] & ~(1 << p[i]);
        }
    }

    // Pseudo-test return: Assume matrix is canonical if a noncanonical permutation witness not yet found
    return true;
}