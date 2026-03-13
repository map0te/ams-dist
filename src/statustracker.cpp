#include "statustracker.hpp"
#include <iostream>

StatEntry StatusTracker::pop() {
    StatEntry entry = *(stats.begin());
    stats.erase(stats.begin());
    return entry;
}

void StatusTracker::erase(int rank) {
    for (auto it = stats.begin(); it != stats.end(); it++) {
        if (it->rank == rank) {
            stats.erase(it);
            break;
        }
    }
}

void StatusTracker::update(int rank, int active) {
    erase(rank);
    StatEntry new_entry = {rank, active};
    stats.insert(new_entry);
}

void StatusTracker::print() {
    printf("Statustracker: ");
    for (auto it = stats.begin(); it != stats.end(); it++) {
        printf("(%d %d) ", it->rank, it->active);
    }
    printf("\n");
    fflush(stdout);
}