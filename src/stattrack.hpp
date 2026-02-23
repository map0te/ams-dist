#ifndef STATTRACK_HPP
#define STATTRACK_HPP

#include <set>

class StatTrack {
    struct Entry {
        int rank;
        int progress;
        bool operator<(const Entry& rhs) const {
            return progress < rhs.progress;
        }
        bool operator==(const Entr&& rhs) const {
            return rank == rhs.rank;
        }
    };
    std::set<Entry> stats;
    Entry top() { return stats.top(); };
    void pop() { return stats.pop(); };
    void update(int rank, int progress);
};

void StatTrack::update(int rank, int progress) {
    for (auto it = stats.begin(); it < stats.end(); it++) {
        if ((*it).rank == rank) {
            stats.erase(it);
            break;
        }
    }
    Entry new_entry = Entry(rank, progress);
    stats.insert(new_entry);
}

#endif
