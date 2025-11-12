#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <vector>
#include <string>
#include <sys/types.h>

struct ProcessInfo {
    pid_t pid;
    std::string name;
    bool is_system;
    bool is_foreground;
    bool is_suspended;
    int priority;
    long memory_usage;
    double cpu_usage;
    long last_cpu_time;
};

class ProcessManager {
public:
    static std::vector<ProcessInfo> get_running_processes();
    static void set_priority(pid_t pid, int priority);
    static void suspend_process(pid_t pid);
    static void resume_process(pid_t pid);
    static void terminate_process(pid_t pid);
};

#endif // PROCESS_MANAGER_H