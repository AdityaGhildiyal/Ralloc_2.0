#ifndef PERFORMANCE_ANALYZER_H
#define PERFORMANCE_ANALYZER_H

#include "process_manager.h"
#include <vector>
#include <map>
#include <string>

struct PerformanceStats {
    double avg_cpu_usage;
    double avg_memory_usage;
    double max_cpu_usage;
    double max_memory_usage;
    int total_processes;
    int suspended_processes;
};

class PerformanceAnalyzer {
public:
    static void collect_sample(
        const std::vector<ProcessInfo>& processes,
        double system_memory,
        double system_cpu
    );
    
    static PerformanceStats get_stats();
    static void reset_stats();
    
    static std::map<std::string, int> get_process_distribution();
    
private:
    static std::vector<double> cpu_samples;
    static std::vector<double> memory_samples;
    static std::map<std::string, int> process_counts;
};

#endif // PERFORMANCE_ANALYZER_H