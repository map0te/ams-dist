#ifndef UTIL_HPP
#define UTIL_HPP

#include "internal.hpp"

#include <vector>
#include <string>
#include <fstream>

inline void append_solutions(std::vector<std::vector<int>>& solutions, std::vector<std::vector<int>>& new_solutions);

void write_dimacs_with_units(CaDiCaL::Solver* solver, const std::string& path);

#endif