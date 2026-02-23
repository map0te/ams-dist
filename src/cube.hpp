#ifndef CUBEHPP
#define CUBEHPP

#include <string>

class Cube {
	public:
		int order, status, active;
		std::string top_name, name, id;
        Cube() {;};
        Cube(int order, int status, int active, std::string id, std::string top_name);
		Cube(std::string cubestr);
		std::string str();
        bool operator< (const Cube rhs) const { return active > rhs.active; };
};

std::string recv_cubestr(int src, int type);
void isend_cubestr(int dst, std::string& cube);
void send_cubestr(int dst, std::string cube);

#endif
