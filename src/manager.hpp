#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <string>
#include <queue>
#include <vector>

#include "cube.hpp"
#include "def.hpp"
#include "stattrack.hpp"

struct WInfo {
    int status;
    std::string cube;
    WInfo() { status = IDLE; }
};

class Manager {
    public:
        int nproc, nworkers, order;
        int num_cubing, num_solving, num_terminated;
        std::string top_name;

        StatTrack st;
        std::multiset<Cube> simp_queue;
        std::queue<Cube> solve_queue;
        std::vector<WInfo> winfo;

        int recv_cubes(int& rank, Cube& cube1, Cube& cube2);
        void send_interrupt(int worker);
        
        void start();

        Manager(int nproc, int order, std::string top_name) {
            this->nproc = nproc;
            this->nworkers = nproc-1;
            this->order = order;
            this->top_name = top_name;
            winfo.resize(nproc);
            num_solving = 0;
            num_terminated = 0;
            num_cubing = 0;
        }
};

#endif