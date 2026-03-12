#include <iostream>
#include <sstream>
#include <getopt.h>

#include "def.hpp"
#include "manager.hpp"
#include "worker.hpp"

void print_help(const char* name) {
    std::cout << "Usage: " << name << " [OPTION]... ORDER FILE PATH" << std::endl;
    std::cout << "Distributed cube n conquer solver" << std::endl;
    std::cout << "Solve FILE with order ORDER in working directory PATH" << std::endl << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help          Show help" << std::endl;
    std::cout << "  -s, --solfile       Output solution file            (default=none)" << std::endl;
    std::cout << "  -p, --inprobing     Level of inprobing [0-2]        (default=1)" << std::endl;
    std::cout << "  -c, --cutoffv       Variable cuttoff heuristic      (default=none)" << std::endl;
    std::cout << "  -a, --aggressive    Solve if num cubes decreases" << std::endl;
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
        {"aggressive", no_argument, 0, 'a'}
    };

    InstanceInfo instance;
    instance.order = -1;
    instance.inprobing = 1;
    instance.cutoff_v = -1;
    instance.aggressive = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "has:p:c:", long_options, NULL)) != -1) {
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

    instance.top_name = positional_args[2] + "/top.cnf";
    instance.order = atoi(positional_args[0].c_str());

    init_cubeinfo_type(MPI_CUBEINFO);
    init_taskinfo_type(MPI_TASKINFO);
    init_varscore_type(MPI_VARSCORE);

    if (rank == 0) {
        Manager manager(instance);
        manager.start_time = std::chrono::steady_clock::now();
        manager.printtime();
        printf("Running on %d cores\n", size);
        manager.printtime();
        printf("Instance: %s\n", positional_args[1].c_str()); fflush(stdout);
        manager.printtime();
        printf("Order: %d\n", instance.order); fflush(stdout);
        manager.printtime();
        printf("Working directory: %s\n", positional_args[2].c_str()); fflush(stdout);
        manager.printtime();
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