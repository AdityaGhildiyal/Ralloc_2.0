#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "scheduler.h"
#include "process_manager.h"
#include "memory_manager.h"

namespace py = pybind11;

PYBIND11_MODULE(scheduler_module, m) {
    m.doc() = "Smart Resource Scheduler module for Linux process management";
    
    py::class_<ProcessInfo>(m, "ProcessInfo")
        .def(py::init<>())
        .def_readonly("pid", &ProcessInfo::pid, "Process ID")
        .def_readonly("name", &ProcessInfo::name, "Process name")
        .def_readonly("is_system", &ProcessInfo::is_system, "Is system process")
        .def_readonly("is_foreground", &ProcessInfo::is_foreground, "Is foreground process")
        .def_readwrite("is_suspended", &ProcessInfo::is_suspended, "Is suspended")
        .def_readwrite("priority", &ProcessInfo::priority, "Process priority (-20 to 19)")
        .def_readonly("memory_usage", &ProcessInfo::memory_usage, "Memory usage in bytes")
        .def_readonly("cpu_usage", &ProcessInfo::cpu_usage, "CPU usage percentage")
        .def_readonly("last_cpu_time", &ProcessInfo::last_cpu_time, "Last CPU time in jiffies")
        .def("__repr__", [](const ProcessInfo& p) {
            return "<ProcessInfo pid=" + std::to_string(p.pid) + 
                   " name='" + p.name + "' priority=" + std::to_string(p.priority) + ">";
        });
    
    py::enum_<Mode>(m, "Mode")
        .value("GAMING", Mode::GAMING, "Gaming mode - prioritizes foreground applications")
        .value("PRODUCTIVITY", Mode::PRODUCTIVITY, "Productivity mode - balanced priorities")
        .value("POWER_SAVING", Mode::POWER_SAVING, "Power saving mode - reduces all priorities")
        .export_values();
    
    py::enum_<SchedulingAlgorithm>(m, "SchedulingAlgorithm")
        .value("FCFS", SchedulingAlgorithm::FCFS, "First-Come-First-Served")
        .value("SJF", SchedulingAlgorithm::SJF, "Shortest Job First")
        .value("PRIORITY", SchedulingAlgorithm::PRIORITY, "Priority-based scheduling")
        .value("RR", SchedulingAlgorithm::RR, "Round Robin")
        .value("HYBRID", SchedulingAlgorithm::HYBRID, "Hybrid scheduling (recommended)")
        .export_values();
    
    py::class_<Scheduler>(m, "Scheduler")
        .def(py::init<>(), "Create a new scheduler instance")
        .def("set_mode", &Scheduler::set_mode, 
             py::arg("mode"),
             "Set the scheduler mode (Gaming/Productivity/Power-Saving)")
        .def("set_algorithm", &Scheduler::set_algorithm,
             py::arg("algorithm"),
             "Set the scheduling algorithm")
        .def("start_monitoring", &Scheduler::start_monitoring,
             "Start the monitoring thread")
        .def("stop_monitoring", &Scheduler::stop_monitoring,
             "Stop the monitoring thread")
        .def("get_processes", &Scheduler::get_processes,
             "Get list of all processes")
        .def("set_custom_params", &Scheduler::set_custom_params,
             py::arg("time_slice_ms"), py::arg("mem_threshold_mb"),
             "Set custom scheduling parameters")
        .def("adjust_priorities", &Scheduler::adjust_priorities,
             "Manually adjust process priorities based on current mode");
    
    py::class_<ProcessManager>(m, "ProcessManager")
        .def_static("get_running_processes", &ProcessManager::get_running_processes,
                    "Get all currently running processes")
        .def_static("set_priority", &ProcessManager::set_priority,
                    py::arg("pid"), py::arg("priority"),
                    "Set process priority (-20 to 19, requires root)")
        .def_static("suspend_process", &ProcessManager::suspend_process,
                    py::arg("pid"),
                    "Suspend a process with SIGSTOP (requires root)")
        .def_static("resume_process", &ProcessManager::resume_process,
                    py::arg("pid"),
                    "Resume a suspended process with SIGCONT (requires root)")
        .def_static("terminate_process", &ProcessManager::terminate_process,
                    py::arg("pid"),
                    "Terminate a process with SIGTERM (requires root)");
    
    py::class_<MemoryManager>(m, "MemoryManager")
        .def_static("get_cpu_usage", &MemoryManager::get_cpu_usage,
                    "Get current CPU usage percentage")
        .def_static("get_system_memory_usage", &MemoryManager::get_system_memory_usage,
                    "Get current RAM usage percentage")
        .def_static("get_swap_usage", &MemoryManager::get_swap_usage,
                    "Get current swap usage percentage");
    
    m.attr("__version__") = "1.0.0";
    m.attr("__author__") = "Smart Resource Scheduler Team";
}