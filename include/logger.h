#ifndef LOGGER_H
#define LOGGER_H

#include "process_manager.h"
#include <vector>
#include <string>
#include <fstream>

class Logger {
public:
    static void log_performance(
        const std::vector<ProcessInfo>& processes,
        double memory_usage,
        double cpu_usage
    );
    
    static void set_log_file(const std::string& filename);
    static void enable_logging(bool enabled);
    
private:
    static std::string log_filename;
    static bool logging_enabled;
    static std::ofstream log_file;
};

#endif // LOGGER_H