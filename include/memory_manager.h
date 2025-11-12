#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "process_manager.h"
#include <vector>

class MemoryManager {
public:
    static double get_system_memory_usage();
    static double get_swap_usage();
    static double get_cpu_usage();
    static void optimize_memory(std::vector<ProcessInfo>& processes, double mem_threshold_mb);
};

#endif // MEMORY_MANAGER_H