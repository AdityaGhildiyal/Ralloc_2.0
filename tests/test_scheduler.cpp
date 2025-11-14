#include "scheduler.h"
#include "process_manager.h"
#include "memory_manager.h"
#include "logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <algorithm>


void print_separator() {
    std::cout << std::string(80, '=') << std::endl;
}

void print_system_info() {
    std::cout << "\nSystem Information:" << std::endl;
    std::cout << "  CPU Usage:    " << std::fixed << std::setprecision(2) 
              << MemoryManager::get_cpu_usage() << "%" << std::endl;
    std::cout << "  Memory Usage: " << MemoryManager::get_system_memory_usage() << "%" << std::endl;
    std::cout << "  Swap Usage:   " << MemoryManager::get_swap_usage() << "%" << std::endl;
}

void print_processes(const std::vector<ProcessInfo>& processes) {
    std::cout << "\nProcess List (showing top 10 by memory):" << std::endl;
    std::cout << std::left 
              << std::setw(8) << "PID"
              << std::setw(25) << "Name"
              << std::setw(10) << "Priority"
              << std::setw(12) << "Status"
              << std::setw(12) << "Memory(MB)"
              << std::setw(10) << "CPU(%)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    auto sorted = processes;
    std::sort(sorted.begin(), sorted.end(), 
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.memory_usage > b.memory_usage;
        });
    
    int count = 0;
    for (const auto& proc : sorted) {
        if (count++ >= 10) break;
        
        std::cout << std::left 
                  << std::setw(8) << proc.pid
                  << std::setw(25) << (proc.name.length() > 24 ? proc.name.substr(0, 21) + "..." : proc.name)
                  << std::setw(10) << proc.priority
                  << std::setw(12) << (proc.is_suspended ? "Suspended" : "Running")
                  << std::setw(12) << std::fixed << std::setprecision(2) << (proc.memory_usage / (1024.0 * 1024.0))
                  << std::setw(10) << proc.cpu_usage << std::endl;
    }
}

void test_process_manager() {
    print_separator();
    std::cout << "Testing Process Manager" << std::endl;
    print_separator();
    
    std::cout << "\nFetching running processes..." << std::endl;
    auto processes = ProcessManager::get_running_processes();
    std::cout << "Found " << processes.size() << " processes" << std::endl;
    
    print_processes(processes);
}

void test_memory_manager() {
    print_separator();
    std::cout << "Testing Memory Manager" << std::endl;
    print_separator();
    
    print_system_info();
    
    std::cout << "\nCollecting CPU usage samples..." << std::endl;
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "  Sample " << (i+1) << ": CPU = " 
                  << std::fixed << std::setprecision(2)
                  << MemoryManager::get_cpu_usage() << "%" << std::endl;
    }
}

void test_scheduler() {
    print_separator();
    std::cout << "Testing Scheduler" << std::endl;
    print_separator();
    
    Scheduler scheduler;
    
    std::cout << "\nTesting different modes:" << std::endl;
    
    std::cout << "\n1. Productivity Mode" << std::endl;
    scheduler.set_mode(Mode::PRODUCTIVITY);
    scheduler.start_monitoring();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto processes = scheduler.get_processes();
    std::cout << "   Total processes: " << processes.size() << std::endl;
    int suspended = std::count_if(processes.begin(), processes.end(),
        [](const ProcessInfo& p) { return p.is_suspended; });
    std::cout << "   Suspended: " << suspended << std::endl;
    
    std::cout << "\n2. Gaming Mode" << std::endl;
    scheduler.set_mode(Mode::GAMING);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    processes = scheduler.get_processes();
    suspended = std::count_if(processes.begin(), processes.end(),
        [](const ProcessInfo& p) { return p.is_suspended; });
    std::cout << "   Suspended: " << suspended << std::endl;
    
    std::cout << "\n3. Power Saving Mode" << std::endl;
    scheduler.set_mode(Mode::POWER_SAVING);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    processes = scheduler.get_processes();
    suspended = std::count_if(processes.begin(), processes.end(),
        [](const ProcessInfo& p) { return p.is_suspended; });
    std::cout << "   Suspended: " << suspended << std::endl;
    
    std::cout << "\nTesting scheduling algorithms:" << std::endl;
    
    const char* alg_names[] = {"FCFS", "SJF", "Priority", "Round-Robin", "Hybrid"};
    SchedulingAlgorithm algorithms[] = {
        SchedulingAlgorithm::FCFS,
        SchedulingAlgorithm::SJF,
        SchedulingAlgorithm::PRIORITY,
        SchedulingAlgorithm::RR,
        SchedulingAlgorithm::HYBRID
    };
    
    for (int i = 0; i < 5; ++i) {
        std::cout << "   " << alg_names[i] << "..." << std::endl;
        scheduler.set_algorithm(algorithms[i]);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    scheduler.stop_monitoring();
    std::cout << "\nScheduler stopped." << std::endl;
}

void test_logger() {
    print_separator();
    std::cout << "Testing Logger" << std::endl;
    print_separator();
    
    std::cout << "\nEnabling logging to 'test_scheduler.log'..." << std::endl;
    Logger::set_log_file("test_scheduler.log");
    Logger::enable_logging(true);
    
    auto processes = ProcessManager::get_running_processes();
    double mem = MemoryManager::get_system_memory_usage();
    double cpu = MemoryManager::get_cpu_usage();
    
    Logger::log_performance(processes, mem, cpu);
    std::cout << "Log entry written. Check 'test_scheduler.log'" << std::endl;
}

int main() {
    if (geteuid() != 0) {
        std::cerr << "\nWARNING: Not running as root!" << std::endl;
        std::cerr << "Some operations may fail without root privileges." << std::endl;
        std::cerr << "Run with: sudo ./test_scheduler\n" << std::endl;
    }
    
    print_separator();
    std::cout << "Smart Resource Scheduler - Test Program" << std::endl;
    print_separator();
    
    try {
        test_process_manager();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        test_memory_manager();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        test_logger();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (geteuid() == 0) {
            test_scheduler();
        } else {
            std::cout << "\nSkipping scheduler tests (requires root)" << std::endl;
        }
        
        print_separator();
        std::cout << "All tests completed successfully!" << std::endl;
        print_separator();
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}