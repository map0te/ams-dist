#ifndef STATTRACK_HPP
#define STATTRACK_HPP

#include <set>

struct StatEntry {
    int rank;
    int progress;
    bool operator<(const StatEntry& rhs) const {
        return progress < rhs.progress;
    }
    bool operator==(const StatEntry& rhs) const {
        return rank == rhs.rank;
    }
};

class StatTrack {
    public:
    std::multiset<StatEntry> stats;
    StatEntry pop();
    size_t size() { return stats.size(); };
    void erase(int rank);
    void update(int rank, int progress);
};

StatEntry StatTrack::pop() {
    StatEntry entry = *(stats.begin());
    stats.erase(stats.begin());
    return entry;
}

void StatTrack::erase(int rank) {
    for (auto it = stats.begin(); it != stats.end(); it++) {
        if (it->rank == rank) {
            stats.erase(it);
            break;
        }
    }
}

void StatTrack::update(int rank, int progress) {
    erase(rank);
    StatEntry new_entry = {rank, progress};
    stats.insert(new_entry);
}

#endif
