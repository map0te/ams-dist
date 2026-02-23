#ifndef WORKER_HPP
#define WORKER_HPP

#include <mpi.h>

#include "internal.hpp"

class Cube;

class Worker : public CaDiCaL::Terminator, public CaDiCaL::Handler {
    public:
        /*--------- Solver ----------*/
        CaDiCaL::Solver *solver; 
        Cube cube;
        int res; // result
        volatile bool timesup = false;
        int time_limit; // time limit in seconds
        int max_var;

        int get(const char *o) { return solver->get (o); };
        bool set(const char *o, int v) { return solver->set (o, v); };
        bool set(const char *arg) { return solver->set_long_option (arg); };

        int split();
        int simplify();
        int solve(bool interruptable);
        void format_res(int res);
        int eliminated() { max_var - solver->active(); };

        void write_file();
        void read_file(std::string name);

        /*--------- Terminator ----------*/
        int counter;
        int progress;
        bool p_flag;
        bool terminate ();
        void catch_signal (int sig) { return; };
        void catch_alarm () { timesup = true; };

        /*--------- Message Handler ----------*/
        MPI_Request interrupt_req, progress_req; 
        void isend_active();
        void send_cubeids(int count);

        /*--------- Worker ----------*/
        int state, rank;
        double start_ts;
        Worker() {
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            solver = 0;
            counter = 0; 
            state = 0;
        };
        ~Worker() { return; };
        void start();
};

#endif
