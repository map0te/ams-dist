#ifndef CUBEHPP
#define CUBEHPP

#include <string>
#include <sstream>

#include <mpi.h>

class Cube {
	public:
		int order, status;
		std::string top_name, name, id;
		Cube(int order, int status, std::string id, std::string top_name);
		Cube(std::string cubestr);
		std::string to_str();	
};

Cube::Cube(int order, int status, std::string id, std::string top_name)
	: order(order), status(status), id(id), top_name(top_name) {
		if (id == "") {
			this->name = top_name;
		} else {
			this->name = top_name + "." + id + ".cnf";
		}	
	}

// initialize Cube from Cube string
Cube::Cube(std::string cubestr) {
    std::stringstream ss(cubestr);
    std::string temp;
    std::getline(ss, temp, ' ');
    order = atoi(temp.c_str());
    std::getline(ss, temp, ' ');
    status = atoi(temp.c_str());
    std::getline(ss, top_name, ' ');
    std::getline(ss, id, ' ');
    if (id == "") {
        name = top_name;
    } else {
        name = top_name + "." + id + ".cnf";
    }
}

// serialize Cube object to Cube string
std::string Cube::to_str() {
    std::stringstream ss;
    ss << order << " ";
    ss << status << " ";
    ss << top_name << " ";
    ss << id;
    return std::move(ss).str();
}

std::string recv_cubestr(int src, int type) {
    int recvlen;
    MPI_Status s;
    MPI_Probe(src, type, MPI_COMM_WORLD, &s);
    MPI_Get_count(&s, MPI_CHAR, &recvlen);
    char* recvdata = new char[recvlen];
    MPI_Recv(recvdata, recvlen, MPI_CHAR, src, type, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    std::string cube = std::string(recvdata);
    delete recvdata;
    return cube;
}

void isend_cubestr(int dst, std::string& cube) {
    int sendlen = strlen(cube.c_str()) + 1;
    MPI_Request req;
    MPI_Isend(cube.c_str(), sendlen, MPI_CHAR, dst, CUBESTR, MPI_COMM_WORLD, &req);
    MPI_Request_free(&req);
}

#endif
