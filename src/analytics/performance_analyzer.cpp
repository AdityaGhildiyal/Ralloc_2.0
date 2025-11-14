#include "performance_analyzer.h"
#include <algorithm>
#include <numeric>

std::vector<double> PerformanceAnalyzer::cpu_samples;
std::vector<double> PerformanceAnalyzer::memory_samples;
std::map<std::string, int> PerformanceAnalyzer::process_counts;

void PerformanceAnalyzer::collect_sample(
    const std::vector<ProcessInfo>& processes,
    double system_memory,
    double system_cpu
) {
    cpu_samples.push_back(system_cpu);
    memory_samples.push_back(system_memory);
    
    if (cpu_samples.size() > 100) {
        cpu_samples.erase(cpu_samples.begin());
    }
    if (memory_samples.size() > 100) {
        memory_samples.erase(memory_samples.begin());
    }
    
    process_counts.clear();
    for (const auto& proc : processes) {
        if (proc.is_system) {
            process_counts["system"]++;
        } else if (proc.is_foreground) {
            process_counts["foreground"]++;
        } else {
            process_counts["background"]++;
        }
        
        if (proc.is_suspended) {
            process_counts["suspended"]++;
        }
    }
}

PerformanceStats PerformanceAnalyzer::get_stats() {
    PerformanceStats stats = {0};
    
    if (!cpu_samples.empty()) {
        stats.avg_cpu_usage = std::accumulate(cpu_samples.begin(), cpu_samples.end(), 0.0) / cpu_samples.size();
        stats.max_cpu_usage = *std::max_element(cpu_samples.begin(), cpu_samples.end());
    }
    
    if (!memory_samples.empty()) {
        stats.avg_memory_usage = std::accumulate(memory_samples.begin(), memory_samples.end(), 0.0) / memory_samples.size();
        stats.max_memory_usage = *std::max_element(memory_samples.begin(), memory_samples.end());
    }
    
    stats.total_processes = process_counts["system"] + process_counts["foreground"] + process_counts["background"];
    stats.suspended_processes = process_counts["suspended"];
    
    return stats;
}

void PerformanceAnalyzer::reset_stats() {
    cpu_samples.clear();
    memory_samples.clear();
    process_counts.clear();
}

std::map<std::string, int> PerformanceAnalyzer::get_process_distribution() {
    return process_counts;
}