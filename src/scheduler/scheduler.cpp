#include "scheduler.h"
#include "process_manager.h"
#include "memory_manager.h"
#include "logger.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <numeric>

Scheduler::Scheduler() 
    : current_mode(Mode::PRODUCTIVITY), 
      current_algorithm(SchedulingAlgorithm::HYBRID), 
      running(false), 
      time_slice_ms(5), 
      mem_threshold_mb(200) {}

Scheduler::~Scheduler() {
    stop_monitoring();
}

void Scheduler::set_mode(Mode mode) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx);
    current_mode = mode;
    apply_mode_settings();
    cv.notify_all();
}

void Scheduler::set_algorithm(SchedulingAlgorithm alg) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx);
    current_algorithm = alg;
}

void Scheduler::set_custom_params(int time_slice_ms, double mem_threshold_mb) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx);
    this->time_slice_ms = std::max(1, time_slice_ms);
    this->mem_threshold_mb = std::max(50.0, mem_threshold_mb);
}

void Scheduler::start_monitoring() {
    if (!running.load()) {
        running.store(true);
        monitoring_thread = std::thread(&Scheduler::monitoring_loop, this);
    }
}

void Scheduler::stop_monitoring() {
    if (running.load()) {
        running.store(false);
        cv.notify_all();
        if (monitoring_thread.joinable()) {
            monitoring_thread.join();
        }
    }
}

void Scheduler::monitoring_loop() {
    while (running.load()) {
        try {
            {
                std::unique_lock<std::shared_mutex> lock(rw_mtx);
                monitor_processes();
                perform_scheduling();
                MemoryManager::optimize_memory(processes, mem_threshold_mb);
                Logger::log_performance(processes, 
                    MemoryManager::get_system_memory_usage(), 
                    MemoryManager::get_cpu_usage());
            }
            
            // Sleep for 1 second or until stopped
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            std::cerr << "Error in monitoring loop: " << e.what() << std::endl;
        }
    }
}

void Scheduler::monitor_processes() {
    processes = ProcessManager::get_running_processes();
}

void Scheduler::apply_mode_settings() {
    for (auto& proc : processes) {
        // Skip if process no longer exists
        if (proc.pid <= 0) continue;
        
        int priority = proc.priority;
        bool should_suspend = false;
        
        switch (current_mode) {
            case Mode::GAMING:
                // Boost foreground applications, deprioritize background
                if (proc.is_foreground) {
                    priority = std::max(-15, priority - 5);
                } else if (!proc.is_system) {
                    priority = std::min(15, priority + 5);
                }
                should_suspend = false;
                break;
                
            case Mode::PRODUCTIVITY:
                // Balanced approach
                if (proc.is_foreground) {
                    priority = std::max(-10, priority - 3);
                } else if (!proc.is_system) {
                    priority = std::min(10, priority + 2);
                }
                should_suspend = false;
                break;
                
            case Mode::POWER_SAVING:
                // Reduce all priorities, suspend high-memory processes
                if (!proc.is_system) {
                    priority = std::min(19, priority + 5);
                    if (proc.memory_usage > mem_threshold_mb * 1024 * 1024 && !proc.is_foreground) {
                        should_suspend = true;
                    }
                }
                break;
        }
        
        proc.priority = priority;
        
        try {
            ProcessManager::set_priority(proc.pid, priority);
            
            if (should_suspend && !proc.is_suspended) {
                ProcessManager::suspend_process(proc.pid);
                proc.is_suspended = true;
            } else if (!should_suspend && proc.is_suspended && current_mode != Mode::POWER_SAVING) {
                ProcessManager::resume_process(proc.pid);
                proc.is_suspended = false;
            }
        } catch (const std::exception& e) {
            // Process might have terminated, continue with others
            continue;
        }
    }
}

void Scheduler::perform_scheduling() {
    switch (current_algorithm) {
        case SchedulingAlgorithm::FCFS:
            fcfs_schedule();
            break;
        case SchedulingAlgorithm::SJF:
            sjf_schedule();
            break;
        case SchedulingAlgorithm::PRIORITY:
            priority_schedule();
            break;
        case SchedulingAlgorithm::RR:
            rr_schedule();
            break;
        case SchedulingAlgorithm::HYBRID:
            hybrid_schedule();
            break;
    }
}

void Scheduler::fcfs_schedule() {
    // First-Come-First-Served: Earlier PIDs get higher priority
    std::sort(processes.begin(), processes.end(), 
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.pid < b.pid;
        });
    
    int priority = -20; // Start with highest priority
    for (auto& proc : processes) {
        if (!proc.is_suspended && !proc.is_system) {
            priority = std::min(19, priority + 1);
            proc.priority = priority;
            try {
                ProcessManager::set_priority(proc.pid, priority);
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
}

void Scheduler::sjf_schedule() {
    // Shortest Job First: Processes with less CPU time get higher priority
    std::sort(processes.begin(), processes.end(), 
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.last_cpu_time < b.last_cpu_time;
        });
    
    int priority = -20;
    for (auto& proc : processes) {
        if (!proc.is_suspended && !proc.is_system) {
            priority = std::min(19, priority + 1);
            proc.priority = priority;
            try {
                ProcessManager::set_priority(proc.pid, priority);
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
}

void Scheduler::priority_schedule() {
    // Priority-based: Use existing priorities
    std::sort(processes.begin(), processes.end(), 
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.priority < b.priority;
        });
    
    for (auto& proc : processes) {
        if (!proc.is_suspended) {
            try {
                ProcessManager::set_priority(proc.pid, proc.priority);
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
}

void Scheduler::rr_schedule() {
    // Round Robin: All processes get equal priority
    for (auto& proc : processes) {
        if (!proc.is_suspended && !proc.is_system) {
            proc.priority = 0; // Normal priority
            try {
                ProcessManager::set_priority(proc.pid, 0);
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
}

void Scheduler::hybrid_schedule() {
    if (processes.empty()) return;
    
    // Classify processes into categories
    std::vector<ProcessInfo*> interactive, io_bound, cpu_bound, background;
    
    for (auto& proc : processes) {
        if (proc.is_suspended || proc.is_system) continue;
        
        if (proc.is_foreground) {
            interactive.push_back(&proc);
        } else if (proc.cpu_usage > 70.0) {
            cpu_bound.push_back(&proc);
        } else if (proc.cpu_usage < 20.0) {
            io_bound.push_back(&proc);
        } else {
            background.push_back(&proc);
        }
    }
    
    // Assign priorities based on category
    // Interactive: Highest priority (-15 to -10)
    int priority = -15;
    for (auto* proc : interactive) {
        proc->priority = priority;
        try {
            ProcessManager::set_priority(proc->pid, priority);
        } catch (const std::exception& e) {
            continue;
        }
        priority = std::min(-10, priority + 1);
    }
    
    // I/O bound: Medium-high priority (-5 to 0)
    priority = -5;
    for (auto* proc : io_bound) {
        proc->priority = priority;
        try {
            ProcessManager::set_priority(proc->pid, priority);
        } catch (const std::exception& e) {
            continue;
        }
        priority = std::min(0, priority + 1);
    }
    
    // Background: Medium priority (5 to 10)
    priority = 5;
    for (auto* proc : background) {
        proc->priority = priority;
        try {
            ProcessManager::set_priority(proc->pid, priority);
        } catch (const std::exception& e) {
            continue;
        }
        priority = std::min(10, priority + 1);
    }
    
    // CPU bound: Lower priority (10 to 19)
    priority = 10;
    for (auto* proc : cpu_bound) {
        proc->priority = priority;
        try {
            ProcessManager::set_priority(proc->pid, priority);
        } catch (const std::exception& e) {
            continue;
        }
        priority = std::min(19, priority + 1);
    }
}

void Scheduler::adjust_priorities() {
    std::unique_lock<std::shared_mutex> lock(rw_mtx);
    apply_mode_settings();
}

std::vector<ProcessInfo> Scheduler::get_processes() const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx);
    return processes;
}