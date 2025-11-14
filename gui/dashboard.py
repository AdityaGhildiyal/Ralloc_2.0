import tkinter as tk
from tkinter import ttk, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import scheduler_module
import time
import os
import sys
from brightness_control import BrightnessControl

class Dashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("Smart Resource Scheduler v1.0")
        self.root.geometry("1200x800")
        
        # Check for root privileges
        if os.geteuid() != 0:
            messagebox.showerror(
                "Insufficient Privileges",
                "This application requires root privileges to manage processes.\n\n"
                "Please run with: sudo python3 dashboard.py"
            )
            sys.exit(1)
        
        self.scheduler = scheduler_module.Scheduler()
        self.scheduler.start_monitoring()

        # Initialize brightness control
        self.brightness_control = BrightnessControl()
        
        # State variables
        self.running = True
        self.all_processes = []
        self.sort_column = "Memory"  # Default sort column
        self.sort_reverse = True     # True = descending (higher values first)
        self.user_sorted = False
        
        # Setup UI
        self.setup_ui()
        
        # Start periodic updates
        self.schedule_update()
        
        # Handle window close
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
    
    def setup_ui(self):
        """Setup the user interface"""
        # Main frame
        self.main_frame = ttk.Frame(self.root, padding="10")
        self.main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        self.main_frame.columnconfigure(0, weight=1)
        self.main_frame.rowconfigure(1, weight=1)
        
        # Mode label
        self.mode_label = ttk.Label(
            self.main_frame, 
            text="Mode: Productivity", 
            font=("Arial", 14, "bold"),
            foreground="blue"
        )
        self.mode_label.grid(row=0, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        
        # Notebook (tabs)
        self.notebook = ttk.Notebook(self.main_frame)
        self.notebook.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Create tabs
        self.setup_processes_tab()
        self.setup_performance_tab()
        self.setup_settings_tab()
        self.setup_logs_tab()
    
    def setup_processes_tab(self):
        """Setup the processes management tab"""
        self.processes_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.processes_frame, text="Processes")
        
        # Configure grid
        self.processes_frame.columnconfigure(0, weight=1)
        self.processes_frame.rowconfigure(2, weight=1)
        
        # Top controls frame
        top_frame = ttk.Frame(self.processes_frame)
        top_frame.grid(row=0, column=0, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Mode selector
        ttk.Label(top_frame, text="Mode:").grid(row=0, column=0, padx=(0, 5))
        self.mode_combo = ttk.Combobox(
            top_frame,
            values=["Gaming", "Productivity", "Power-Saving"],
            state="readonly",
            width=15
        )
        self.mode_combo.set("Productivity")
        self.mode_combo.grid(row=0, column=1, padx=(0, 20))
        self.mode_combo.bind("<<ComboboxSelected>>", self.on_mode_changed)
        
        # Search/filter
        ttk.Label(top_frame, text="Filter:").grid(row=0, column=2, padx=(0, 5))
        self.search_entry = ttk.Entry(top_frame, width=20)
        self.search_entry.grid(row=0, column=3, padx=(0, 10))
        self.search_entry.bind("<KeyRelease>", self.filter_processes)
        
        # Process count
        self.process_count_label = ttk.Label(top_frame, text="Processes: 0")
        self.process_count_label.grid(row=0, column=4, padx=(20, 0))
        
        # Button frame
        button_frame = ttk.Frame(self.processes_frame)
        button_frame.grid(row=1, column=0, sticky=tk.W, pady=(0, 10))
        
        ttk.Button(button_frame, text="Suspend", command=self.suspend_process, width=12).grid(row=0, column=0, padx=5)
        ttk.Button(button_frame, text="Resume", command=self.resume_process, width=12).grid(row=0, column=1, padx=5)
        ttk.Button(button_frame, text="Terminate", command=self.terminate_process, width=12).grid(row=0, column=2, padx=5)
        ttk.Button(button_frame, text="Refresh", command=self.update_ui, width=12).grid(row=0, column=3, padx=5)
        
        # Process tree with scrollbars
        tree_frame = ttk.Frame(self.processes_frame)
        tree_frame.grid(row=2, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        tree_frame.columnconfigure(0, weight=1)
        tree_frame.rowconfigure(0, weight=1)
        
        self.process_tree = ttk.Treeview(
            tree_frame,
            columns=("PID", "Name", "Priority", "Status", "Memory", "CPU"),
            show="headings",
            selectmode="extended"
            )
        
        # Configure columns
        self.process_tree.heading("PID", text="PID", command=lambda: self.sort_treeview("PID"))
        self.process_tree.heading("Name", text="Name", command=lambda: self.sort_treeview("Name"))
        self.process_tree.heading("Priority", text="Priority", command=lambda: self.sort_treeview("Priority"))
        self.process_tree.heading("Status", text="Status", command=lambda: self.sort_treeview("Status"))
        self.process_tree.heading("Memory", text="Memory (MB)", command=lambda: self.sort_treeview("Memory"))
        self.process_tree.heading("CPU", text="CPU (%)", command=lambda: self.sort_treeview("CPU"))
        
        self.process_tree.column("PID", width=80, anchor=tk.CENTER)
        self.process_tree.column("Name", width=200, anchor=tk.W)
        self.process_tree.column("Priority", width=80, anchor=tk.CENTER)
        self.process_tree.column("Status", width=100, anchor=tk.CENTER)
        self.process_tree.column("Memory", width=120, anchor=tk.E)
        self.process_tree.column("CPU", width=100, anchor=tk.E)
        
        # Scrollbars
        vsb = ttk.Scrollbar(tree_frame, orient="vertical", command=self.process_tree.yview)
        hsb = ttk.Scrollbar(tree_frame, orient="horizontal", command=self.process_tree.xview)
        self.process_tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)
        
        self.process_tree.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        vsb.grid(row=0, column=1, sticky=(tk.N, tk.S))
        hsb.grid(row=1, column=0, sticky=(tk.W, tk.E))
        # Set default sort column
        #self.sort_column = "Memory"
        #self.sort_reverse = False  # Will be toggled to True on first sort
    
    def setup_performance_tab(self):
        """Setup the performance monitoring tab"""
        self.performance_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.performance_frame, text="Performance")
        
        # Configure grid
        self.performance_frame.columnconfigure(0, weight=1)
        self.performance_frame.rowconfigure(1, weight=1)
        
        # Status labels frame
        self.status_frame = ttk.Frame(self.performance_frame)
        self.status_frame.grid(row=0, column=0, sticky=tk.W, pady=(0, 10))
        
        self.cpu_label = ttk.Label(self.status_frame, text="CPU: 0.00%", font=("Arial", 12))
        self.cpu_label.grid(row=0, column=0, padx=10)
        
        self.mem_label = ttk.Label(self.status_frame, text="RAM: 0.00%", font=("Arial", 12))
        self.mem_label.grid(row=0, column=1, padx=10)
        
        self.swap_label = ttk.Label(self.status_frame, text="Swap: 0.00%", font=("Arial", 12))
        self.swap_label.grid(row=0, column=2, padx=10)
        
        # Matplotlib graph
        self.fig, self.ax = plt.subplots(figsize=(10, 6))
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.performance_frame)
        self.canvas.get_tk_widget().grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.times = []
        self.cpu_data = []
        self.mem_data = []
        self.swap_data = []
        self.time_step = 0
    
    def setup_settings_tab(self):
        """Setup the settings tab"""
        self.settings_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(self.settings_frame, text="Settings")
        
        # Scheduling settings
        settings_label = ttk.Label(self.settings_frame, text="Scheduling Parameters", font=("Arial", 12, "bold"))
        settings_label.grid(row=0, column=0, columnspan=2, sticky=tk.W, pady=(0, 20))
        
        ttk.Label(self.settings_frame, text="Time Slice (ms):").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.time_slice_entry = ttk.Entry(self.settings_frame, width=20)
        self.time_slice_entry.insert(0, "5")
        self.time_slice_entry.grid(row=1, column=1, sticky=tk.W, padx=(10, 0), pady=5)
        
        ttk.Label(self.settings_frame, text="Memory Threshold (MB):").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.mem_threshold_entry = ttk.Entry(self.settings_frame, width=20)
        self.mem_threshold_entry.insert(0, "200.0")
        self.mem_threshold_entry.grid(row=2, column=1, sticky=tk.W, padx=(10, 0), pady=5)
        
        ttk.Label(self.settings_frame, text="Scheduling Algorithm:").grid(row=3, column=0, sticky=tk.W, pady=5)
        self.algorithm_combo = ttk.Combobox(
            self.settings_frame,
            values=["FCFS", "SJF", "Priority", "Round-Robin", "Hybrid"],
            state="readonly",
            width=18
        )
        self.algorithm_combo.set("Hybrid")
        self.algorithm_combo.grid(row=3, column=1, sticky=tk.W, padx=(10, 0), pady=5)
        
        # Algorithm descriptions
        descriptions = ttk.LabelFrame(self.settings_frame, text="Algorithm Descriptions", padding="10")
        descriptions.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(20, 10))
        
        desc_text = tk.Text(descriptions, height=10, width=60, wrap=tk.WORD, state='disabled')
        desc_text.pack(fill=tk.BOTH, expand=True)
        
        desc_text.config(state='normal')
        desc_text.insert('1.0', 
            "FCFS (First-Come-First-Served): Earlier processes get higher priority\n\n"
            "SJF (Shortest Job First): Processes with less CPU time get priority\n\n"
            "Priority: Uses process priority values directly\n\n"
            "Round-Robin: All processes get equal time slices\n\n"
            "Hybrid (Recommended): Adapts based on process behavior:\n"
            "  • Interactive (foreground) → Highest priority\n"
            "  • I/O bound (low CPU) → Medium-high priority\n"
            "  • CPU bound (high CPU) → Lower priority"
        )
        desc_text.config(state='disabled')
        
        ttk.Button(
            self.settings_frame, 
            text="Apply Settings", 
            command=self.apply_settings,
            width=20
        ).grid(row=5, column=0, columnspan=2, pady=(20, 0))
    
    def setup_logs_tab(self):
        """Setup the logs tab"""
        self.logs_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.logs_frame, text="Logs")
        
        # Configure grid
        self.logs_frame.columnconfigure(0, weight=1)
        self.logs_frame.rowconfigure(0, weight=1)
        
        # Log text widget with scrollbar
        log_container = ttk.Frame(self.logs_frame)
        log_container.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        log_container.columnconfigure(0, weight=1)
        log_container.rowconfigure(0, weight=1)
        
        self.log_text = tk.Text(log_container, height=20, width=80, state='disabled', wrap=tk.WORD)
        self.log_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        scrollbar = ttk.Scrollbar(log_container, command=self.log_text.yview)
        scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        self.log_text.config(yscrollcommand=scrollbar.set)
        
        # Clear logs button
        ttk.Button(self.logs_frame, text="Clear Logs", command=self.clear_logs).grid(row=1, column=0, pady=(10, 0))
        
        # Log initial message
        self.log_message("Smart Resource Scheduler started successfully")
        self.log_message(f"Running as user: {os.getenv('USER', 'root')} (UID: {os.geteuid()})")
    
    def schedule_update(self):
        """Schedule periodic UI updates using Tkinter's event loop"""
        if self.running:
            try:
                self.update_ui()
            except Exception as e:
                self.log_message(f"Error updating UI: {str(e)}")
            finally:
                self.root.after(1000, self.schedule_update)  # Update every 1000ms
    
    def update_ui(self):
        """Update UI with current process list and system stats"""
        try:
            # Get processes from scheduler
            processes = self.scheduler.get_processes()
            self.all_processes = processes
            
            # Apply current filter
            self.filter_processes()
            
            # Update system stats
            cpu = scheduler_module.MemoryManager.get_cpu_usage()
            mem = scheduler_module.MemoryManager.get_system_memory_usage()
            swap = scheduler_module.MemoryManager.get_swap_usage()
            
            # Update labels with color coding
            self.cpu_label.config(text=f"CPU: {cpu:.2f}%")
            self.mem_label.config(text=f"RAM: {mem:.2f}%")
            self.swap_label.config(text=f"Swap: {swap:.2f}%")
            
            # Color code based on usage
            self.update_label_color(self.cpu_label, cpu, 80, 60)
            self.update_label_color(self.mem_label, mem, 85, 70)
            self.update_label_color(self.swap_label, swap, 70, 50)
            
            # Update performance graph
            self.update_graph(cpu, mem, swap)
            
        except Exception as e:
            self.log_message(f"Error in update_ui: {str(e)}")
    
    def update_label_color(self, label, value, critical, warning):
        """Update label color based on value thresholds"""
        if value > critical:
            label.config(foreground="red")
        elif value > warning:
            label.config(foreground="orange")
        else:
            label.config(foreground="green")
    
    def update_graph(self, cpu, mem, swap):
        """Update the performance graph"""
        self.times.append(self.time_step)
        self.cpu_data.append(cpu)
        self.mem_data.append(mem)
        self.swap_data.append(swap)
        
        # Keep only last 60 seconds
        if len(self.times) > 60:
            self.times.pop(0)
            self.cpu_data.pop(0)
            self.mem_data.pop(0)
            self.swap_data.pop(0)
        
        # Clear and redraw
        self.ax.clear()
        self.ax.plot(self.times, self.cpu_data, label="CPU Usage (%)", linewidth=2, color='#1f77b4')
        self.ax.plot(self.times, self.mem_data, label="Memory Usage (%)", linewidth=2, color='#ff7f0e')
        self.ax.plot(self.times, self.swap_data, label="Swap Usage (%)", linewidth=2, color='#2ca02c')
        
        self.ax.legend(loc='upper left')
        self.ax.set_ylim(0, 100)
        self.ax.set_xlim(max(0, self.time_step - 60), self.time_step)
        self.ax.set_xlabel('Time (seconds)')
        self.ax.set_ylabel('Usage (%)')
        self.ax.set_title('System Performance')
        self.ax.grid(True, alpha=0.3)
        
        self.canvas.draw()
        self.time_step += 1
    
    def filter_processes(self, event=None):
        """Filter processes based on search term"""
        search_term = self.search_entry.get().lower()
    
        # Store current selection
        selected_items = self.process_tree.selection()
        selected_pid = None
        if selected_items:
            selected_pid = self.process_tree.item(selected_items[0])["values"][0]
    
        # Store current scroll position
        first_visible = None
        visible_items = self.process_tree.get_children()
        if visible_items:
            # Get the first visible item's index
            yview = self.process_tree.yview()
            first_visible = yview[0]  # Store scroll position
    
        self.process_tree.delete(*self.process_tree.get_children())
    
        filtered_count = 0
        for proc in self.all_processes:
            if (search_term in proc.name.lower() or 
                search_term in str(proc.pid)):
            
                item_id = self.process_tree.insert("", "end", values=(
                    proc.pid,
                    proc.name,
                    proc.priority,
                    "Suspended" if proc.is_suspended else "Running",
                    f"{proc.memory_usage / (1024 * 1024):.2f}",
                    f"{proc.cpu_usage:.2f}"
                ))
            
                # Restore selection if this was the selected process (but don't scroll to it)
                if selected_pid and proc.pid == selected_pid:
                    self.process_tree.selection_set(item_id)
                    # REMOVED: self.process_tree.see(item_id)  # This causes scrolling
            
                filtered_count += 1
    
        self.process_count_label.config(
            text=f"Processes: {filtered_count} / {len(self.all_processes)}"
        )
    
        # Only apply sorting if user has chosen a sort or on initial load
        if hasattr(self, 'sort_column') and self.sort_column and self.user_sorted:
            # Re-apply the user's chosen sort without toggling
            current_reverse = self.sort_reverse
            items = [(self.process_tree.set(item, self.sort_column), item) 
                    for item in self.process_tree.get_children('')]
        
            if self.sort_column in ["PID", "Priority"]:
                try:
                    items.sort(key=lambda x: int(x[0]), reverse=current_reverse)
                except ValueError:
                    items.sort(key=lambda x: str(x[0]).lower(), reverse=current_reverse)
            elif self.sort_column in ["Memory", "CPU"]:
                try:
                    items.sort(key=lambda x: float(x[0].replace(',', '')), reverse=current_reverse)
                except ValueError:
                    items.sort(key=lambda x: 0.0, reverse=current_reverse)
            else:
                items.sort(key=lambda x: str(x[0]).lower(), reverse=current_reverse)
        
            for index, (_, item) in enumerate(items):
                self.process_tree.move(item, '', index)  


    def sort_treeview(self, col):
        """Sort treeview by column"""
        # If clicking the same column, toggle direction
        if self.sort_column == col:
            self.sort_reverse = not self.sort_reverse
        else:
            # New column: default to descending for numeric columns, ascending for text
            self.sort_column = col
            if col in ["Memory", "CPU"]:
                self.sort_reverse = True  # Descending for numeric
            else:
                self.sort_reverse = False  # Ascending for text/priority
    
        self.user_sorted = True  # Mark that user has manually sorted
    
        items = [(self.process_tree.set(item, col), item) 
                for item in self.process_tree.get_children('')]
    
        # Determine data type for sorting
        if col in ["PID", "Priority"]:
            # Integer sorting
            try:
                items.sort(key=lambda x: int(x[0]), reverse=self.sort_reverse)
            except ValueError:
                items.sort(key=lambda x: str(x[0]).lower(), reverse=self.sort_reverse)
        elif col in ["Memory", "CPU"]:
            # Float sorting
            try:
                items.sort(key=lambda x: float(x[0].replace(',', '')), reverse=self.sort_reverse)
            except ValueError:
                items.sort(key=lambda x: 0.0, reverse=self.sort_reverse)
        else:
            # String sorting
            items.sort(key=lambda x: str(x[0]).lower(), reverse=self.sort_reverse)
    
        # Rearrange items
        for index, (_, item) in enumerate(items):
            self.process_tree.move(item, '', index)
    
        # Update column heading to show sort direction
        for column in self.process_tree["columns"]:
            if column == col:
                heading = f"{column} {'▼' if self.sort_reverse else '▲'}"
            else:
                heading = column
            self.process_tree.heading(column, text=heading)
    
    def on_mode_changed(self, event):
        """Handle mode change"""
        mode_map = {
            "Gaming": scheduler_module.Mode.GAMING,
            "Productivity": scheduler_module.Mode.PRODUCTIVITY,
            "Power-Saving": scheduler_module.Mode.POWER_SAVING
        }
    
        selected_mode = self.mode_combo.get()
    
        # Handle brightness for power saving mode
        if selected_mode == "Power-Saving":
            # Save current brightness before changing
            if not hasattr(self, '_brightness_saved'):
                self.brightness_control.save_current_brightness()
                self._brightness_saved = True
        
            # Set brightness to 20% for power saving
            if self.brightness_control.set_brightness_percent(20):
                self.log_message("Display brightness reduced to 20% for power saving")
            else:
                self.log_message("Could not adjust display brightness (may not be supported)")
        else:
            # Restore original brightness when leaving power saving mode
            if hasattr(self, '_brightness_saved') and self._brightness_saved:
                if self.brightness_control.restore_brightness():
                    self.log_message("Display brightness restored")
                self._brightness_saved = False
    
        # Apply mode to scheduler
        self.scheduler.set_mode(mode_map[selected_mode])
        self.mode_label.config(text=f"Mode: {selected_mode}")
    
        # Log mode-specific behavior
        if selected_mode == "Gaming":
            self.log_message(f"Mode changed to {selected_mode} - Foreground apps prioritized, background suspended")
        elif selected_mode == "Power-Saving":
            self.log_message(f"Mode changed to {selected_mode} - All processes minimized, heavy processes suspended")
        else:
            self.log_message(f"Mode changed to {selected_mode} - Balanced resource allocation")
    
        self.update_ui()
    
    def apply_settings(self):
        """Apply custom settings"""
        try:
            time_slice = int(self.time_slice_entry.get())
            mem_threshold = float(self.mem_threshold_entry.get())
            
            if time_slice < 1 or time_slice > 1000:
                raise ValueError("Time slice must be between 1 and 1000 ms")
            
            if mem_threshold < 50 or mem_threshold > 10000:
                raise ValueError("Memory threshold must be between 50 and 10000 MB")
            
            self.scheduler.set_custom_params(time_slice, mem_threshold)
            
            alg_map = {
                "FCFS": scheduler_module.SchedulingAlgorithm.FCFS,
                "SJF": scheduler_module.SchedulingAlgorithm.SJF,
                "Priority": scheduler_module.SchedulingAlgorithm.PRIORITY,
                "Round-Robin": scheduler_module.SchedulingAlgorithm.RR,
                "Hybrid": scheduler_module.SchedulingAlgorithm.HYBRID
            }
            
            selected_alg = self.algorithm_combo.get()
            self.scheduler.set_algorithm(alg_map[selected_alg])
            
            self.log_message(f"Settings applied: Time slice={time_slice}ms, "
                           f"Memory threshold={mem_threshold}MB, Algorithm={selected_alg}")
            messagebox.showinfo("Settings", "Settings applied successfully!")
            
        except ValueError as e:
            messagebox.showerror("Invalid Input", str(e))
            self.log_message(f"Error applying settings: {str(e)}")
    
    def suspend_process(self):
        """Suspend selected process"""
        selected = self.process_tree.selection()
        if not selected:
            messagebox.showwarning("Selection Required", "Please select a process first.")
            return
        
        try:
            pid = int(self.process_tree.item(selected[0])["values"][0])
            proc_name = self.process_tree.item(selected[0])["values"][1]
            
            # Warn about critical processes
            if proc_name in ['systemd', 'init', 'Xorg', 'gdm', 'sddm']:
                if not messagebox.askyesno("Warning", 
                    f"Suspending {proc_name} may cause system instability.\n\n"
                    f"Continue anyway?"):
                    return
            
            scheduler_module.ProcessManager.suspend_process(pid)
            self.log_message(f"Process {proc_name} (PID: {pid}) suspended")
            messagebox.showinfo("Success", f"Process {proc_name} suspended.")
            
        except Exception as e:
            error_msg = f"Failed to suspend process: {str(e)}"
            messagebox.showerror("Error", error_msg)
            self.log_message(error_msg)
        finally:
            self.update_ui()
    
    def resume_process(self):
        """Resume selected process"""
        selected = self.process_tree.selection()
        if not selected:
            messagebox.showwarning("Selection Required", "Please select a process first.")
            return
        
        try:
            pid = int(self.process_tree.item(selected[0])["values"][0])
            proc_name = self.process_tree.item(selected[0])["values"][1]
            
            scheduler_module.ProcessManager.resume_process(pid)
            self.log_message(f"Process {proc_name} (PID: {pid}) resumed")
            messagebox.showinfo("Success", f"Process {proc_name} resumed.")
            
        except Exception as e:
            error_msg = f"Failed to resume process: {str(e)}"
            messagebox.showerror("Error", error_msg)
            self.log_message(error_msg)
        finally:
            self.update_ui()
    
    def terminate_process(self):
        """Terminate selected process"""
        selected = self.process_tree.selection()
        if not selected:
            messagebox.showwarning("Selection Required", "Please select a process first.")
            return
        
        pid = int(self.process_tree.item(selected[0])["values"][0])
        proc_name = self.process_tree.item(selected[0])["values"][1]
        
        import os
        actual_uid = os.geteuid()
        self.log_message(f"DEBUG: Current UID = {actual_uid}, PID to terminate = {pid}")
        
        # Extra warning for system processes
        warning_msg = f"Are you sure you want to terminate {proc_name} (PID: {pid})?"
        
        if proc_name in ['systemd', 'init', 'kthreadd', 'Xorg', 'gdm', 'sddm', 'sshd']:
            warning_msg += f"\n\nWARNING: Terminating {proc_name} may crash your system!"
        
        if not messagebox.askyesno("Confirm Termination", warning_msg):
            return
        
        try:
            scheduler_module.ProcessManager.terminate_process(pid)
            self.log_message(f"Process {proc_name} (PID: {pid}) terminated")
            messagebox.showinfo("Success", f"Process {proc_name} terminated.")
            
        except Exception as e:
            error_msg = f"Failed to terminate process: {str(e)}"
            messagebox.showerror("Error", error_msg)
            self.log_message(error_msg)
        finally:
            self.update_ui()
    
    def log_message(self, message):
        """Add a log message with timestamp"""
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        self.log_text.config(state='normal')
        self.log_text.insert('end', f"[{timestamp}] {message}\n")
        self.log_text.see('end')
        self.log_text.config(state='disabled')
    
    def clear_logs(self):
        """Clear all log messages"""
        self.log_text.config(state='normal')
        self.log_text.delete('1.0', 'end')
        self.log_text.config(state='disabled')
        self.log_message("Logs cleared")
    
    def on_closing(self):
        """Handle window close event"""
        if messagebox.askokcancel("Quit", "Do you want to quit the Smart Resource Scheduler?"):
            self.log_message("Scheduler stopping...")
            self.running = False
        
            # Restore brightness if it was changed
            if hasattr(self, '_brightness_saved') and self._brightness_saved:
                self.brightness_control.restore_brightness()
                self.log_message("Display brightness restored")
        
            self.scheduler.stop_monitoring()
            self.root.destroy()


def main():
    root = tk.Tk()
    app = Dashboard(root)
    root.mainloop()


if __name__ == "__main__":
    main()