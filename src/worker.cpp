#include "cube.hpp"
#include "def.hpp"
#include "internal.hpp"
#include "signal.hpp"
#include "symbreak.hpp"
#include "worker.hpp"

void Worker::read_file(std::string name) {
    bool incremental;
    std::vector<int> cube_literals;
    solver->read_dimacs (name.c_str(), max_var, true, incremental, cube_literals);
}

void Worker::write_file() {
    std::string output_path = cube.name + ".simp";
    solver->write_dimacs (output_path.c_str(), max_var);
}

void Worker::isend_active() {
    MPI_ISend(&progress, 1, MPI_INT, ROOT, PROGRESS, MPI_COMM_WORLD, &progress_req);
    MPI_Request_free(&progress_req);
}

void Worker::send_cubeids(int count) {
    MPI_Send(&count, 1, MPI_INT, ROOT, CUBENUM, MPI_COMM_WORLD);
    if (count) {
        // send cube ids
        std::string id1 = cube.id + "1";
        std::string id2 = cube.id + "2";
        MPI_Send(id1.c_str(), id1.size()+1, MPI_CHAR,
            ROOT, CUBEID, MPI_COMM_WORLD);
        MPI_Send(id2.c_str(), id2.size()+1, MPI_CHAR,
            ROOT, CUBEID, MPI_COMM_WORLD);
    }   
}

void Worker::format_res(int res) {
    printf("c ----- %d RESULT: ", rank);
    if (res == 0) {
        printf("UNKNOWN -----\n");
    } else if (res == 10) {
        printf("SATISFIABLE -----\n");
    } else {
        printf("UNSATISFIABLE -----\n");
    }
    fflush(stdout);
}

int Worker::simplify() {
    // setup solver
    solver = new CaDiCaL::Solver ();
    CaDiCaL::Signal::alarm (TIMELIMIT);
    CaDiCaL::Signal::set (this);
    solver->limit ("proofsize", PROOFSIZE);
    solver->limit ("conflicts", SIMPLIMIT);

    // simplify
    std::string solving_file = cube.name;
    printf("c ----- SIMPLIFY -----\n");
    read_file(solving_file.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, cube.order, 0);
    res = solver->solve ();
    if (res == 0) { write_file(); } else { std::remove(cube.name.c_str()); }
    format_res(res);
    delete se;
    delete solver;
    solver = 0;
    return res;
}

int Worker::solve(bool interruptable) {
    // setup solver
    assert (!solver);
    solver = new CaDiCaL::Solver ();
    CaDiCaL::Signal::alarm (TIMELIMIT);
    CaDiCaL::Signal::set (this);
    solver->limit ("proofsize", PROOFSIZE);

    // terminiator for solving
    p_flag = false;
	if (interruptable) solver->connect_terminator (this);
	
    // solve
    printf("c ----- SOLVE -----\n");
    read_file(cube.name.c_str());
    SymmetryBreaker* se = new SymmetryBreaker(solver, cube.order, 0);
    start_ts = clock();
    res = solver->solve ();
    if (res == 0) { write_file(); } else { std::remove(cube.name_cstr()); }
    format_res(res);
    delete se;
    delete solver;
    solver = 0;
    return res;
}

bool Worker::terminate() {
    if (timesup)
        return true;
    // check every NUMSKIP termination calls to reduce overhead
    if (counter % NUMSKIP == 0) {
        counter = 1;
        // send progress
        // warmup solving period, should not be preempted
        if ((clock() - start_ts) / CLOCKS_PER_SEC > WARMUP) {
            if (!p_flag || solver->active() < prev_progress) {
                p_flag = true;
                prev_progress = solver->active();
                isend_active();
            }
        }
        // probe interrupt request
        int flag = false;
        MPI_Test(&interrupt_req, &flag, MPI_STATUS_IGNORE);
        return flag;
    } else {
        counter++;
        return false;
    }
}

int Worker::split() {
    // in order to generate cubes, we must call a python
    // program via a system call as this is the easiest
    // and most reliable method

    std::stringstream apply_cmd1, apply_cmd2;
    std::stringstream cube_cmd, cube_file;
    cube_file << cube.top_name;
    cube_file << "." << cube.id << ".cube";

    cube_cmd << "./simple_mcts ";
    cube_cmd << cube.name + ".simp ";
    cube_cmd << "-d 1 -m " << cube.order*(cube.order-1)/2 << " ";
    cube_cmd << "-o " << cube_file.str();

    apply_cmd1 << "./gen_cubes/apply.sh ";
    apply_cmd1 << cube.name << ".simp " << cube_file.str() << " 1 > ";
    apply_cmd1 << cube.top_name << "." << cube.id << "1.cnf";

    apply_cmd2 << "./gen_cubes/apply.sh ";
    apply_cmd2 << cube.name << ".simp " << cube_file.str() << " 2 > ";
    apply_cmd2 << cube.top_name << "." << cube.id << "2.cnf";

    system(cube_cmd.str().c_str());

    // workaround for no cube generated
    std::uintmax_t size = std::filesystem::file_size(cube_file.str());
    if (size == 5) {
        cube.name = cube.name + ".simp";
        solve(false);
        return 0;
    }

    system(apply_cmd1.str().c_str());
    system(apply_cmd2.str().c_str());
    std::remove(cube.name.c_str());
    std::remove((cube.name + ".simp").c_str());
    return 2;
}

void Worker::start() {
    std::string cubestr;
    int count;

    // work loop
    for (;;) {
        state = IDLE;
        count = 0;

        // recieve cube
        printf("rank %d recv_cube\n", rank);
        fflush(stdout);
        cubestr = recv_cube(ROOT, CUBESTR);
        if (!cubestr.size()) { break; } // end
        cube = Cube(cubestr);
        printf("rank %d recieved %s\n", rank, cube.name.c_str());
        fflush(stdout);
        state = cube.status;

        // if forced cubing
        if (state == CUBING) {
            count = simplify() ? 0 : 2;
            if (count) { count = split(); }
            send_cubeids(count, ROOT);
        }

        // if solve
        else if (state == SOLVING) {
            // setup interrupt request
            MPI_Irecv(NULL, 0, MPI_INT, ROOT, INTERRUPT, 
                MPI_COMM_WORLD, &interrupt_req);
            int res = solve(true);
            if (!res) { count = split(); }
            send_cubeids(count, ROOT);
            // clean up interrupt request
            MPI_Wait(&interrupt_req, MPI_STATUS_IGNORE); 
            //MPI_Request_free(&interrupt_req);
        }
    }
}

