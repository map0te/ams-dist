#ifndef DEFHPP
#define DEFHPP

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

struct InstanceInfo {
	int order;
	const char *top_name;
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

#endif
