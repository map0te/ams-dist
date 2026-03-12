#ifndef DEFHPP
#define DEFHPP

#include <string>
#include <mpi.h>

#define ROOT 0

#define NUMSKIP 100
#define WARMUP 30

#define MAXIDLENGTH 512

#define SIMPLIMIT 10000
#define PROOFSIZE 7168
#define TIMELIMIT 7200
#define NUMMCTS 100

// MPI struct datatypes
extern MPI_Datatype MPI_CUBEINFO;
extern MPI_Datatype MPI_TASKINFO;
extern MPI_Datatype MPI_VARSCORE;

enum TASK_TYPE {
	CUBE,
	DCUBE,
	SIMPLIFY,
	SOLVE,
	SOLVE_NOINT,
	END
};

enum WORKER_STATE { 
	IDLE,
	CUBING,
	DCUBING,
	SIMPLIFYING,
	SOLVING,
	TERMINATED 
};

enum CUBE_STATE { 
	UNKNOWN = 0,
	SAT = 10,
	UNSAT = 20 
};

enum MESSAGE_TYPE { 
	M_PROGRESS,
	M_INTERRUPT,
	M_TASKINFO,
	M_NUMCUBE,
	M_CUBEINFO,
};

struct CubeInfo {
	int active;
	int status;
	long n_solutions;
	char id[MAXIDLENGTH];
	bool operator< (const CubeInfo rhs) const { return active > rhs.active; };
};

struct TaskInfo {
	int type;
	int n_cubeinfo;
};

struct VarScore {
    int var = 0;
    int pos = 0;
    int neg = 0;
    double score = 0.0;
};

inline void init_cubeinfo_type(MPI_Datatype& mpi_type) {
	MPI_Datatype types[3] = { MPI_INT, MPI_LONG, MPI_CHAR };
    int blocklen[3] = {2, 1, MAXIDLENGTH};
    MPI_Aint offsets[3];
	offsets[0] = offsetof(struct CubeInfo, active);
    offsets[1] = offsetof(struct CubeInfo, n_solutions);
    offsets[2] = offsetof(struct CubeInfo, id);
	MPI_Type_create_struct(3, blocklen, offsets, types, &mpi_type);
    MPI_Type_commit(&mpi_type);
}

inline void init_taskinfo_type(MPI_Datatype& mpi_type) {
	MPI_Datatype types[1] = { MPI_INT };
	int blocklen[1] = {2};
	MPI_Aint offsets[1];
	offsets[0] = offsetof(struct TaskInfo, type);
	MPI_Type_create_struct(1, blocklen, offsets, types, &mpi_type);
	MPI_Type_commit(&mpi_type);
}

inline void init_varscore_type(MPI_Datatype& mpi_type) {
	MPI_Datatype types[2] = { MPI_INT, MPI_DOUBLE };
	int blocklen[2] = {3, 1};
	MPI_Aint offsets[2];
	offsets[0] = offsetof(struct VarScore, var);
	offsets[1] = offsetof(struct VarScore, score);
	MPI_Type_create_struct(2, blocklen, offsets, types, &mpi_type);
	MPI_Type_commit(&mpi_type);
}

struct InstanceInfo {
	int order;
	int inprobing;
	int cutoff_v;
	bool aggressive;
	std::string top_name;
	std::string solution_file_name;
};

#endif
