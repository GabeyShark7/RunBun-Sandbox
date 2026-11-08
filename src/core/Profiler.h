#pragma once
#include <chrono>
#include <cstdio>
#include <map>
#include <string>

struct ProfileStats {
    double total = 0.0, worst = 0.0;
    int count = 0;
};

inline std::map<std::string, ProfileStats>& ProfileTable() {
    static std::map<std::string, ProfileStats> table;
    return table;
}

struct ProfileScope {
    const char* name;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    explicit ProfileScope(const char* n) : name(n) {}
    ~ProfileScope() {
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ProfileStats& s = ProfileTable()[name];
        s.total += ms;
        s.count++;
        if (ms > s.worst) s.worst = ms;
    }
};

inline void ProfileDump(double now) {
#ifndef RUNBUN_PROFILE
    (void)now;
    return;
#endif
    static double last = 0.0;
    if (now - last < 2.0) return;
    last = now;
    FILE* f = fopen("perf.txt", "a");
    if (!f) return;
    fprintf(f, "---- %.1f\n", now);
    for (auto& entry : ProfileTable()) {
        if (entry.second.count == 0) continue;
        fprintf(f, "%-28s avg %7.2f ms  worst %7.2f ms  calls %d\n", entry.first.c_str(), entry.second.total / entry.second.count, entry.second.worst, entry.second.count);
        entry.second = ProfileStats();
    }
    fclose(f);
}

#ifdef RUNBUN_PROFILE
#define PROFILE(name) ProfileScope profileScope##__LINE__(name)
#else
#define PROFILE(name)
#endif
