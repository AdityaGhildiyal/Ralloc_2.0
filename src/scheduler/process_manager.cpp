#include "process_manager.h"
#include <dirent.h>
#include <fstream>
#include <signal.h>
#include <sys/resource.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <map>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <unistd.h>

std::vector<ProcessInfo> ProcessManager::get_running_processes() {
    std::vector<ProcessInfo> processes;
    DIR* dir = opendir("/proc");
    if (!dir) return processes;
    
    struct dirent* entry;
    static std::map<pid_t, long> prev_cpu_times;
    static std::map<pid_t, std::chrono::steady_clock::time_point> prev_times;
    auto now = std::chrono::steady_clock::now();
    
    // Clean up stale entries
    std::vector<pid_t> stale_pids;
    for (const auto& pair : prev_cpu_times) {
        bool found = false;
        rewinddir(dir);
        while ((entry = readdir(dir))) {
            if (entry->d_type == DT_DIR && std::isdigit(entry->d_name[0])) {
                if (std::stoi(entry->d_name) == pair.first) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) stale_pids.push_back(pair.first);
    }
    for (pid_t pid : stale_pids) {
        prev_cpu_times.erase(pid);
        prev_times.erase(pid);
    }
    
    rewinddir(dir);
    
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_DIR || !std::isdigit(entry->d_name[0])) continue;
        
        pid_t pid = std::stoi(entry->d_name);
        std::string name;
        std::string stat_path = std::string("/proc/") + entry->d_name + "/stat";
        std::ifstream stat_file(stat_path);
        long cpu_time = 0;
        char state = '?';
        int tty_nr = 0;
        
        if (stat_file.is_open()) {
            std::string line;
            std::getline(stat_file, line);
            
            // Parse process name (between parentheses)
            size_t start = line.find('(');
            size_t end = line.rfind(')');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                name = line.substr(start + 1, end - start - 1);
                
                // Parse fields after the closing parenthesis
                std::string after_comm = line.substr(end + 2);
                std::istringstream iss(after_comm);
                int ppid, pgrp, session, tpgid;
                unsigned long flags, minflt, cminflt, majflt, cmajflt;
                unsigned long utime, stime;
                
                iss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid 
                    >> flags >> minflt >> cminflt >> majflt >> cmajflt 
                    >> utime >> stime;
                
                cpu_time = utime + stime;
            }
            stat_file.close();
        }
        
        // Skip if name is empty
        if (name.empty()) continue;
        
        // Read memory usage
        long mem = 0;
        std::ifstream status_file(std::string("/proc/") + entry->d_name + "/status");
        if (status_file.is_open()) {
            std::string line;
            while (std::getline(status_file, line)) {
                if (line.find("VmRSS:") == 0) {
                    std::istringstream iss(line);
                    std::string key;
                    long value;
                    std::string unit;
                    if (iss >> key >> value >> unit) {
                        mem = value * 1024; // Convert kB to bytes
                    }
                    break;
                }
            }
            status_file.close();
        }
        
        // Calculate CPU usage
        double cpu_usage = 0.0;
        if (prev_cpu_times.count(pid) && prev_times.count(pid)) {
            auto elapsed = std::chrono::duration<double>(now - prev_times[pid]).count();
            if (elapsed > 0.1) { // Only calculate if enough time has passed
                long delta_cpu = cpu_time - prev_cpu_times[pid];
                // Convert jiffies to seconds, then to percentage
                cpu_usage = (delta_cpu * 100.0) / (sysconf(_SC_CLK_TCK) * elapsed);
                cpu_usage = std::min(cpu_usage, 100.0); // Cap at 100%
            }
        }
        prev_cpu_times[pid] = cpu_time;
        prev_times[pid] = now;
        
        // Determine if process is system or foreground
        // A process is considered foreground if it has a controlling terminal
        bool is_fg = (tty_nr > 0);
        
        // System processes typically have low PIDs or are kernel threads
        bool is_system = (pid < 1000) || (state == 'S' && name.find("kworker") != std::string::npos) ||
                         name.find("systemd") != std::string::npos || 
                         name.find("kthreadd") != std::string::npos;
        
        // Get current priority
        int current_priority = getpriority(PRIO_PROCESS, pid);
        if (errno == ESRCH) continue; // Process no longer exists
        
        processes.push_back({
            pid, 
            name, 
            is_system, 
            is_fg, 
            (state == 'T'), // Suspended if state is 'T' (stopped)
            current_priority, 
            mem, 
            cpu_usage, 
            cpu_time
        });
    }
    closedir(dir);
    return processes;
}

void ProcessManager::set_priority(pid_t pid, int priority) {
    priority = std::max(-20, std::min(19, priority));
    if (setpriority(PRIO_PROCESS, pid, priority) != 0) {
        if (errno == ESRCH) {
            // Process no longer exists - not an error we want to throw
            return;
        } else if (errno == EPERM) {
            throw std::runtime_error("Permission denied to set priority for PID " + 
                std::to_string(pid) + " (need root privileges)");
        } else {
            throw std::runtime_error("Failed to set priority for PID " + 
                std::to_string(pid) + ": " + strerror(errno));
        }
    }
}

void ProcessManager::suspend_process(pid_t pid) {
    if (kill(pid, SIGSTOP) != 0) {
        if (errno == ESRCH) {
            throw std::runtime_error("Process " + std::to_string(pid) + " not found");
        } else if (errno == EPERM) {
            throw std::runtime_error("Permission denied to suspend PID " + 
                std::to_string(pid) + " (need root privileges)");
        } else {
            throw std::runtime_error("Failed to suspend PID " + 
                std::to_string(pid) + ": " + strerror(errno));
        }
    }
}

void ProcessManager::resume_process(pid_t pid) {
    if (kill(pid, SIGCONT) != 0) {
        if (errno == ESRCH) {
            throw std::runtime_error("Process " + std::to_string(pid) + " not found");
        } else if (errno == EPERM) {
            throw std::runtime_error("Permission denied to resume PID " + 
                std::to_string(pid) + " (need root privileges)");
        } else {
            throw std::runtime_error("Failed to resume PID " + 
                std::to_string(pid) + ": " + strerror(errno));
        }
    }
}

void ProcessManager::terminate_process(pid_t pid) {
    // Check if it's a critical system process
    if (pid == 1) {
        throw std::runtime_error("Cannot terminate init process (PID 1)");
    }
    
    if (kill(pid, SIGTERM) != 0) {
        if (errno == ESRCH) {
            throw std::runtime_error("Process " + std::to_string(pid) + " not found");
        } else if (errno == EPERM) {
            throw std::runtime_error("Permission denied to terminate PID " + 
                std::to_string(pid) + " (need root privileges)");
        } else {
            throw std::runtime_error("Failed to terminate PID " + 
                std::to_string(pid) + ": " + strerror(errno));
        }
    }
}