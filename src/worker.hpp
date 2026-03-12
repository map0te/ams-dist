#ifndef WORKER_HPP
#define WORKER_HPP

#include <mpi.h>

#include "beamlookahead.hpp"
#include "internal.hpp"
#include "signal.hpp"

#include "def.hpp"

class Worker : public CaDiCaL::Terminator, public CaDiCaL::Handler {
    public:
        /*--------- Instance ----------*/
        InstanceInfo instance;
        TaskInfo task;
        CubeInfo cube;
        std::string current_instance;

        /*--------- Beam Lookahead ----------*/
        BeamLookahead beamlookahead;

        /*--------- Solver ----------*/
        CaDiCaL::Solver *solver; 
        
        int res; // result
        volatile bool timesup = false;
        int time_limit; // time limit in seconds
        int max_var;

        int get(const char *o) { return solver->get (o); };
        bool set(const char *o, int v) { return solver->set (o, v); };
        bool set(const char *arg) { return solver->set_long_option (arg); };

        //int split();
        int simplify();
        int solve();
        void format_res(int res);

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
        int recv_task();
        void send_simplify_result(int res);

        /*--------- Worker ----------*/
        int state, rank;
        double start_ts;
        Worker(InstanceInfo& instance) {
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            this->instance = instance;
            solver = 0;
            counter = 0; 
            state = 0;
        };
        ~Worker() { return; };
        void start();
};

#endif
