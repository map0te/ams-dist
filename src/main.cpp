#include <iostream>
#include <sstream>
#include <getopt.h>

#include "def.hpp"
#include "manager.hpp"
#include "worker.hpp"

MPI_Datatype MPI_TASKINFO;
MPI_Datatype MPI_CUBEINFO;
MPI_Datatype MPI_VARSCORE;

void print_help(const char* name) {
    std::cout << "Usage: " << name << " [OPTION]... ORDER FILE PATH" << std::endl;
    std::cout << "Solve FILE with order ORDER in working directory PATH" << std::endl << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help              show help" << std::endl;
    std::cout << "  -a, --aggressive        solve if num cubes decreases" << std::endl;
    std::cout << "  -s, --solfile FILE      output solution file            (default=none)" << std::endl;
    std::cout << "  -t, --twarmup VAL       time before interrupt (s)       (defauult=60)" << std::endl;
    std::cout << "  -p, --inprobing VAL     level of inprobing [0-2]        (default=1)" << std::endl;
    //std::cout << "  -c, --cutoffv VAL       variable cuttoff heuristic      (default=none)" << std::endl;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"solfile", required_argument, 0, 's'},
        {"inprobing", required_argument, 0, 'p'},
        {"cutoffv", required_argument, 0, 'c'},
        {"aggressive", no_argument, 0, 'a'},
        {"twarmup", required_argument, 0, 't'}
    };

    InstanceInfo instance;
    instance.order = -1;
    instance.inprobing = 1;
    instance.cutoff_v = -1;
    instance.twarmup = 60;
    instance.aggressive = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "has:p:c:t:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                if (!rank) {
                    print_help(argv[0]);
                }
                MPI_Finalize();
                return 0;
            case 'a':
                instance.aggressive = true;
                break;
            case 's':
                instance.solution_file_name = optarg;
                break;
            case 'p':
                instance.inprobing = atoi(optarg);
                if (instance.inprobing < 0 || instance.inprobing > 2) {
                    if (!rank) {
                        std::cout << argv[0] << ": option \"inprobing\" must be 0-2" << std::endl;
                    }
                    MPI_Finalize();
                    return 1;
                }
                break;
            case 't':
                instance.twarmup = atoi(optarg);
                break;
            case 'c':
                instance.cutoff_v = atoi(optarg);
                break;
            default:
                MPI_Finalize(); 
                return 1;
        }
    }

    std::vector<std::string> positional_args;
    for (int i = optind; i < argc; ++i) {
        positional_args.push_back(argv[i]);
    }

    if (positional_args.size() != 3) {
        if (!rank) {
            std::cout << argv[0] << ": requires 3 positional arguments" << std::endl;
        } 
        MPI_Finalize(); 
        return 1; 
    }

    if (size <= 1) {
        std::cout << argv[0] << ": must be run on greater than 1 core" << std::endl;
        MPI_Finalize();
        return 1;
    }

    instance.top_name = positional_args[2] + "/top.cnf";
    instance.order = atoi(positional_args[0].c_str());

    init_cubeinfo_type(MPI_CUBEINFO);
    init_taskinfo_type(MPI_TASKINFO);
    init_varscore_type(MPI_VARSCORE);

    if (rank == 0) {
        Manager manager(instance);
        manager.init_time();
        manager.print_time();
        printf("Running on %d cores\n", size);
        manager.print_time();
        printf("Instance: %s\n", positional_args[1].c_str()); fflush(stdout);
        manager.print_time();
        printf("Order: %d\n", instance.order); fflush(stdout);
        manager.print_time();
        printf("Working directory: %s\n", positional_args[2].c_str()); fflush(stdout);
        manager.print_time();
        printf("Copying instance into working directory...\n"); fflush(stdout);
        std::stringstream cmd;
        cmd << "cp " << positional_args[1] << " " << instance.top_name;
        system(cmd.str().c_str());
        manager.start();
    } else {
        Worker worker(instance);
        worker.start();
    }
    MPI_Finalize();
}