#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process_manager.h"
#include <vector>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <condition_variable>
#include <atomic>

enum class Mode {
    GAMING,
    PRODUCTIVITY,
    POWER_SAVING
};

enum class SchedulingAlgorithm {
    FCFS,
    SJF,
    PRIORITY,
    RR,
    HYBRID
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();
    
    void set_mode(Mode mode);
    void set_algorithm(SchedulingAlgorithm alg);
    void set_custom_params(int time_slice_ms, double mem_threshold_mb);
    
    void start_monitoring();
    void stop_monitoring();
    
    std::vector<ProcessInfo> get_processes() const;
    void adjust_priorities();
    
private:
    Mode current_mode;
    SchedulingAlgorithm current_algorithm;
    std::vector<ProcessInfo> processes;
    
    std::thread monitoring_thread;
    std::atomic<bool> running;
    mutable std::shared_mutex rw_mtx;
    std::mutex cv_mtx;
    std::condition_variable cv;
    
    int time_slice_ms;
    double mem_threshold_mb;
    
    void monitoring_loop();
    void monitor_processes();
    void apply_mode_settings();
    void perform_scheduling();
    
    void fcfs_schedule();
    void sjf_schedule();
    void priority_schedule();
    void rr_schedule();
    void hybrid_schedule();
};

#endif // SCHEDULER_H