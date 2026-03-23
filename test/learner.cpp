#include "internal.hpp"
#include <iostream>

class Wrapper : CaDiCaL::Learner {
  CaDiCaL::Solver *solver;
  std::vector<int> clause;

public:
  unsigned clauses;
  Wrapper (CaDiCaL::Solver *s) : solver (s), clauses (0) {
    solver->connect_learner (this);
  }
  ~Wrapper () { solver->disconnect_learner (); }
  bool learning (int size) {
    (void) size;
    return true;
  }
  void learn (int lit) {
    if (lit)
      clause.push_back (lit);
    else {
      std::cout << "solver[" << ((void *) solver)
                << "] imported clause of size " << clause.size () << ':';
      for (auto lit : clause)
        std::cout << ' ' << lit;
      std::cout << std::endl << std::flush;
      clause.clear ();
      clauses++;
    }
  }
};

int main (int argc, char** argv) {
  CaDiCaL::Solver ping;
  ping.set ("log", 1);
  ping.set ("otfs", 0);
  ping.limit("conflicts", 10);
  ping.set("phase", atoi(argv[1]));
  Wrapper wing (&ping);
    bool incremental;
    std::vector<int> cube_literals;
    int max_var;
    ping.read_dimacs ("/storage/home/hcoda1/1/mzhulin3/scratch/instances/21/ks_21.cnf", max_var, true, incremental, cube_literals);
  int a = ping.solve ();
  std::cout << "ping returns " << a << std::endl;
  std::cout << "wong imported " << wing.clauses << " clauses" << std::endl;
  ping.clause(1,2);
  ping.limit("conflicts", 10);
  a = ping.solve ();
  std::cout << "ping returns " << a << std::endl;
  std::cout << "wong imported " << wing.clauses << " clauses" << std::endl;
  return 0;
}