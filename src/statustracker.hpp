#ifndef STATUSTRACKER_HPP
#define STATUSTRACKER_HPP

#include <cstddef>
#include <set>

struct StatEntry {
    int rank;
    int active;
    bool operator<(const StatEntry& rhs) const {
        return active > rhs.active;
    }
    bool operator==(const StatEntry& rhs) const {
        return rank == rhs.rank;
    }
};

class StatusTracker {
    public:
    std::multiset<StatEntry> stats;
    StatEntry pop();
    void print();
    size_t size() { return stats.size(); };
    void erase(int rank);
    void update(int rank, int active);
};

#endif
