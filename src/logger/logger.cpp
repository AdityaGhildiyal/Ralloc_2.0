#include "logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string Logger::log_filename = "scheduler.log";
bool Logger::logging_enabled = true;
std::ofstream Logger::log_file;

void Logger::log_performance(
    const std::vector<ProcessInfo>& processes,
    double memory_usage,
    double cpu_usage
) {
    if (!logging_enabled) return;
    
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);
    
    std::stringstream ss;
    ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
    std::string timestamp = ss.str();
    
    if (!log_file.is_open()) {
        log_file.open(log_filename, std::ios::app);
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file: " << log_filename << std::endl;
            return;
        }
    }
    
    log_file << "[" << timestamp << "] "
             << "System - CPU: " << std::fixed << std::setprecision(2) << cpu_usage << "%, "
             << "Memory: " << memory_usage << "%, "
             << "Processes: " << processes.size() << std::endl;
    
    int suspended_count = 0;
    for (const auto& proc : processes) {
        if (proc.is_suspended) suspended_count++;
    }
    
    if (suspended_count > 0) {
        log_file << "[" << timestamp << "] "
                 << "Status - " << suspended_count << " processes suspended" << std::endl;
    }
    
    log_file.flush();
}

void Logger::set_log_file(const std::string& filename) {
    if (log_file.is_open()) {
        log_file.close();
    }
    log_filename = filename;
}

void Logger::enable_logging(bool enabled) {
    logging_enabled = enabled;
    if (!enabled && log_file.is_open()) {
        log_file.close();
    }
}