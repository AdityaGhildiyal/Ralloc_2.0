#include "memory_manager.h"
#include "process_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <unistd.h>

double MemoryManager::get_system_memory_usage() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return 0.0;
    
    std::string line;
    long total = 0, free = 0, buffers = 0, cached = 0, slab = 0;
    
    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        long value;
        std::string unit;
        
        if (iss >> key >> value) {
            if (key == "MemTotal:") total = value;
            else if (key == "MemFree:") free = value;
            else if (key == "Buffers:") buffers = value;
            else if (key == "Cached:") cached = value;
            else if (key == "Slab:") slab = value;
        }
    }
    meminfo.close();
    
    if (total == 0) return 0.0;
    
    // Used memory = Total - Free - Buffers - Cached
    // This matches what 'free' command shows
    long used = total - free - buffers - cached - slab;
    used = std::max(0L, used); // Ensure non-negative
    
    return (used * 100.0) / total;
}

double MemoryManager::get_swap_usage() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return 0.0;
    
    std::string line;
    long total = 0, free = 0;
    
    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        long value;
        
        if (iss >> key >> value) {
            if (key == "SwapTotal:") total = value;
            else if (key == "SwapFree:") free = value;
        }
    }
    meminfo.close();
    
    if (total == 0) return 0.0;
    
    return ((total - free) * 100.0) / total;
}

double MemoryManager::get_cpu_usage() {
    static long prev_total = 0, prev_idle = 0;
    static auto prev_time = std::chrono::steady_clock::now();
    static bool first_call = true;
    
    std::ifstream stat("/proc/stat");
    if (!stat.is_open()) return 0.0;
    
    std::string line;
    std::getline(stat, line);
    stat.close();
    
    long user, nice, system, idle, iowait, irq, softirq, steal;
    user = nice = system = idle = iowait = irq = softirq = steal = 0;
    
    sscanf(line.c_str(), "cpu %ld %ld %ld %ld %ld %ld %ld %ld", 
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    
    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - prev_time).count();
    
    double cpu_usage = 0.0;
    
    if (!first_call && elapsed >= 0.1) {
        long delta_total = total - prev_total;
        long delta_idle = idle - prev_idle;
        
        if (delta_total > 0) {
            cpu_usage = 100.0 * (delta_total - delta_idle) / delta_total;
            cpu_usage = std::max(0.0, std::min(100.0, cpu_usage)); // Clamp to [0, 100]
        }
        
        prev_total = total;
        prev_idle = idle;
        prev_time = now;
    } else if (first_call) {
        first_call = false;
        prev_total = total;
        prev_idle = idle;
        prev_time = now;
    }
    
    return cpu_usage;
}

void MemoryManager::optimize_memory(std::vector<ProcessInfo>& processes, double mem_threshold_mb) {
    double mem_usage = get_system_memory_usage();
    double swap_usage = get_swap_usage();
    
    // Only optimize if memory or swap usage is critically high
    if (mem_usage > 90.0 || swap_usage > 70.0) {
        // Sort processes by memory usage (highest first)
        std::sort(processes.begin(), processes.end(), 
            [](const ProcessInfo& a, const ProcessInfo& b) {
                return a.memory_usage > b.memory_usage;
            });
        
        // Suspend non-system processes that exceed the threshold
        int suspended_count = 0;
        for (auto& proc : processes) {
            // Don't suspend system processes or already suspended processes
            if (proc.is_system || proc.is_suspended) continue;
            
            // Don't suspend processes with controlling terminal (likely user-facing)
            if (proc.is_foreground) continue;
            
            // Check if memory usage exceeds threshold
            if (proc.memory_usage > mem_threshold_mb * 1024 * 1024) {
                try {
                    ProcessManager::suspend_process(proc.pid);
                    proc.is_suspended = true;
                    suspended_count++;
                    
                    // Stop after suspending a few processes
                    if (suspended_count >= 3) break;
                } catch (const std::exception& e) {
                    // Process might have terminated, ignore
                    continue;
                }
            }
        }
    }
    
    // Resume processes if memory pressure is relieved
    if (mem_usage < 70.0 && swap_usage < 50.0) {
        for (auto& proc : processes) {
            if (proc.is_suspended && !proc.is_system) {
                try {
                    ProcessManager::resume_process(proc.pid);
                    proc.is_suspended = false;
                } catch (const std::exception& e) {
                    // Process might have terminated, ignore
                    continue;
                }
            }
        }
    }
}