from __future__ import annotations
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import pygame
from typing import Dict, List, Any, Optional
import time
from statistics import mean, median
import math
import json
import re
import traceback

# NEW: Matplotlib for graphing
try:
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

import config as cfg
from main import (
    configure as main_configure, run_frame as main_run_frame,
    AGENT_CLASSES as main_agent_classes,
    init_pygame_surface as main_init_pygame_surface,
    get_simulation_stats
)
from math_module import set_frame_dt
from config import (
    set_agent_speed_scaling_factor,
    set_angular_proportionality_constant
)

class VarD(tk.DoubleVar):
    def __init__(self, v: float): super().__init__(value=round(v, 4))
class VarI(tk.IntVar):
    def __init__(self, v: int): super().__init__(value=v)

class SimGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Agent Sim - Run and Tumble Model")
        # Set a minimum size to prevent the UI from becoming unusable
        self.root.minsize(720, 800)
        
        # Configure the root window's grid to be resizable
        self.root.grid_rowconfigure(0, weight=1)
        self.root.grid_columnconfigure(0, weight=1)

        self.notebook = ttk.Notebook(self.root)
        self.notebook.grid(row=0, column=0, sticky="nsew", padx=5, pady=5)

        self.controls_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.controls_tab, text="Controls & Sim Params")
        self.composite_config_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.composite_config_tab, text="Composite Config", state="disabled")
        self.headless_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.headless_tab, text="Headless Runs")
        self.status_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.status_tab, text="Status & Graphs")

        self._tk_vars = {
            "fps": VarI(60), "n_agents": VarI(1), "global_speed_modifier": VarD(1.0),
            "agent_speed_scaling_factor": VarD(cfg.AGENT_SPEED_SCALING_FACTOR),
            "angular_proportionality_constant": VarD(cfg.ANGULAR_PROPORTIONALITY_CONSTANT),
            "turn_decision_interval_sec": VarD(cfg.DEFAULT_TURN_DECISION_INTERVAL_SEC),
            "spawn_x": VarD(cfg.WINDOW_W / 2), "spawn_y": VarD(cfg.WINDOW_H / 2),
            "initial_direction_deg": VarD(0.0), "stoch_update_interval_sec": VarD(1.0),
            "inv_threshold_r": VarD(0.0), "inv_threshold_l": VarD(1.0),
            "inv_r_low_amp": VarD(8.0), "inv_r_high_amp": VarD(6.0),
            "inv_l_low_amp": VarD(4.0), "inv_l_high_amp": VarD(8.0),
            "inv_r_low_freq_hz": VarD(1.0), "inv_r_high_freq_hz": VarD(2.0),
            "inv_l_low_freq_hz": VarD(1.0), "inv_l_high_freq_hz": VarD(2.0),
        }
        self.draw_trace_var = tk.BooleanVar(value=False); self.enable_logging_var = tk.BooleanVar(value=False)
        self.permanent_trace_var = tk.BooleanVar(value=False)
        self.composite_layer_pair_vars: List[Dict[str, tk.Variable]] = []
        self.composite_config_canvas: Optional[tk.Canvas] = None; self.composite_scrollable_frame: Optional[ttk.Frame] = None
        self.specific_agent_params_frame: Optional[ttk.LabelFrame] = None
        self.headless_agent_type_var = tk.StringVar(value=list(main_agent_classes.keys())[0] if main_agent_classes else "")
        self.headless_expiry_timer_var = VarI(30); self.headless_num_agents_var = VarI(10); self.headless_num_instances_var = VarI(5)
        self.headless_avg_eff_var = tk.StringVar(value="Avg. Efficiency: N/A"); self.headless_median_eff_var = tk.StringVar(value="Median Efficiency: N/A")
        self.headless_status_var = tk.StringVar(value="Status: Idle")
        
        # Plotting variables (Updated for 2 subplots)
        self.fig = None
        self.ax_counts = None  # Top plot
        self.ax_rate = None    # Bottom plot
        self.canvas = None
        
        self._build_status_tab(self.status_tab); self._build_controls_tab(self.controls_tab); self._build_composite_config_tab(self.composite_config_tab); self._build_headless_tab(self.headless_tab)
        self.running_simulation = False; self.sim_thread = None; self.pygame_surface = None; self.simulation_start_time_visual = None; self.headless_run_active = False; self.active_log_file = None
        self.root.protocol("WM_DELETE_WINDOW", self.quit_application); self._periodic_status_update(); self._on_agent_type_change()

    def _add_log_message(self, msg):
        if hasattr(self, 'log_text') and self.log_text.winfo_exists():
            self.log_text.config(state="normal"); self.log_text.insert(tk.END, f"{time.strftime('%H:%M:%S')} - {msg}\n"); self.log_text.see(tk.END); self.log_text.config(state="disabled")
    def quit_application(self):
        self._add_log_message("Quit application requested.");
        if self.active_log_file and not self.active_log_file.closed: self.active_log_file.close()
        self.headless_run_active = False; self.stop_simulation()
        if self.root.winfo_exists(): self.root.quit(); self.root.destroy()
    
    def _add_slider_entry(self, parent, var, lbl_text, from_, to, row, res=None, lbl_width=20, entry_width=None):
        """Helper to add a Label, Scale (Slider), and Entry using the grid layout."""
        parent.grid_columnconfigure(1, weight=1) # Make the slider column expandable
        
        lbl = ttk.Label(parent, text=lbl_text, width=lbl_width)
        lbl.grid(row=row, column=0, sticky='w', padx=(0, 5))
        
        if res is None: res = 0.01 if isinstance(var, tk.DoubleVar) else 1
        
        s = ttk.Scale(parent, variable=var, from_=from_, to=to, orient="horizontal")
        s.grid(row=row, column=1, sticky='ew', padx=5)

        if entry_width is None: entry_width = 7 if isinstance(var, tk.DoubleVar) else 5
        entry = ttk.Entry(parent, textvariable=var, width=entry_width)
        entry.grid(row=row, column=2, sticky='e')
        return s, entry

    def _create_frequency_slider(self, parent, var, label_text, from_, to, row, res):
        """Helper to create a frequency slider with a period display, using grid."""
        parent.grid_columnconfigure(1, weight=1) # Make the slider column expandable
        
        ttk.Label(parent, text=label_text, width=15).grid(row=row, column=0, sticky='w')
        
        s = ttk.Scale(parent, variable=var, from_=from_, to=to, orient="horizontal")
        s.grid(row=row, column=1, sticky='ew', padx=5)
        
        ttk.Entry(parent, textvariable=var, width=7).grid(row=row, column=2, sticky='e')
        
        delay_var = tk.StringVar(value="")
        delay_label = ttk.Label(parent, textvariable=delay_var, foreground="gray")
        delay_label.grid(row=row + 1, column=1, sticky='n', pady=(0, 5))
        
        def update_delay(*args):
            try:
                freq = var.get()
                if freq > 1e-6: delay_var.set(f"(Cycle delay: {1.0/freq:.2f} s)")
                else: delay_var.set("(Cycle delay: infinite)")
            except (tk.TclError, ValueError): delay_var.set("")
        var.trace_add("write", update_delay); update_delay()

    def _build_controls_tab(self, parent_tab):
        parent_tab.grid_rowconfigure(0, weight=1)
        parent_tab.grid_columnconfigure(0, weight=1)

        # Create scrollable canvas
        self.controls_canvas = tk.Canvas(parent_tab, borderwidth=0, highlightthickness=0, bg='#f0f0f0')
        controls_scrollbar = ttk.Scrollbar(parent_tab, orient="vertical", command=self.controls_canvas.yview)
        self.controls_canvas.configure(yscrollcommand=controls_scrollbar.set)

        self.controls_canvas.grid(row=0, column=0, sticky='nsew')
        controls_scrollbar.grid(row=0, column=1, sticky='ns')

        main = ttk.Frame(self.controls_canvas, padding=10)
        self.controls_scrollable_frame_id = self.controls_canvas.create_window((0, 0), window=main, anchor="nw")
        main.grid_columnconfigure(0, weight=1)

        def _on_frame_configure(event):
            # Use frame's required height + 20% slack for scroll region
            req_height = main.winfo_reqheight()
            self.controls_canvas.configure(scrollregion=(0, 0, main.winfo_reqwidth(), int(req_height * 1.2)))

        def _on_canvas_configure(event):
            canvas_width = event.width
            self.controls_canvas.itemconfigure(self.controls_scrollable_frame_id, width=canvas_width)

        main.bind("<Configure>", _on_frame_configure)
        self.controls_canvas.bind("<Configure>", _on_canvas_configure)

        # Enable mousewheel scrolling only when mouse is over this canvas
        def _on_mousewheel(event):
            self.controls_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        def _bind_mousewheel(event):
            self.controls_canvas.bind_all("<MouseWheel>", _on_mousewheel)

        def _unbind_mousewheel(event):
            self.controls_canvas.unbind_all("<MouseWheel>")

        self.controls_canvas.bind("<Enter>", _bind_mousewheel)
        self.controls_canvas.bind("<Leave>", _unbind_mousewheel)

        sim_timing_frame = ttk.LabelFrame(main, text="Global Modifiers & Timing")
        sim_timing_frame.grid(row=0, column=0, sticky='ew', pady=5)
        sim_timing_frame.grid_columnconfigure(1, weight=1)
        self._add_slider_entry(sim_timing_frame, self._tk_vars["fps"], "Target FPS", 10, 240, 0, res=1.0, lbl_width=28)
        self._add_slider_entry(sim_timing_frame, self._tk_vars["global_speed_modifier"], "Global Speed Modifier", 0.1, 5.0, 1, res=0.1, lbl_width=28)

        physics_frame = ttk.LabelFrame(main, text="Core Motion Physics")
        physics_frame.grid(row=1, column=0, sticky='ew', pady=5)
        physics_frame.grid_columnconfigure(1, weight=1)
        self._add_slider_entry(physics_frame, self._tk_vars["agent_speed_scaling_factor"], "Speed Scaling Factor", 1, 100, 0, res=1.0, lbl_width=28)
        self._add_slider_entry(physics_frame, self._tk_vars["angular_proportionality_constant"], "Angular Proportionality", 0.1, 5.0, 1, res=0.1, lbl_width=28)
        
        world_frame = ttk.LabelFrame(main, text="World & Agent Setup")
        world_frame.grid(row=2, column=0, sticky='ew', pady=5)
        world_frame.grid_columnconfigure(1, weight=1)
        
        ttk.Button(world_frame, text="Import Preset...", command=self._import_preset_config).grid(row=0, column=0, columnspan=3, sticky='w', padx=5, pady=2)
        
        ttk.Label(world_frame, text="Agent Type", width=28).grid(row=1, column=0, sticky='w', pady=2)
        self.agent_type_var = tk.StringVar(value=list(main_agent_classes.keys())[0])
        self.agent_sel = ttk.Combobox(world_frame, values=list(main_agent_classes.keys()), state="readonly", textvariable=self.agent_type_var)
        self.agent_sel.grid(row=1, column=1, columnspan=2, sticky='ew', padx=5, pady=2)
        self.agent_type_var.trace_add("write", self._on_agent_type_change)

        self.specific_agent_params_frame = ttk.LabelFrame(main, text="Specific Agent Parameters")
        
        ttk.Label(world_frame, text="# Agents", width=28).grid(row=3, column=0, sticky='w', pady=2)
        ttk.Spinbox(world_frame, from_=1, to=150, textvariable=self._tk_vars["n_agents"]).grid(row=3, column=1, columnspan=2, sticky='ew', padx=5, pady=2)
        
        self._add_slider_entry(world_frame, self._tk_vars["spawn_x"], "Spawn X", 0, cfg.WINDOW_W, 4, res=1.0, lbl_width=28)
        self._add_slider_entry(world_frame, self._tk_vars["spawn_y"], "Spawn Y", 0, cfg.WINDOW_H, 5, res=1.0, lbl_width=28)
        self._add_slider_entry(world_frame, self._tk_vars["initial_direction_deg"], "Initial Orientation (°)", 0, 359, 6, res=1.0, lbl_width=28)
        
        ttk.Label(world_frame, text="Food Preset", width=28).grid(row=7, column=0, sticky='w', pady=2)
        self.food_sel = ttk.Combobox(world_frame, values=list(cfg.FOOD_PRESETS.keys()), state="readonly")
        self.food_sel.current(0)
        self.food_sel.grid(row=7, column=1, columnspan=2, sticky='ew', padx=5, pady=2)

        checkbox_frame = ttk.Frame(world_frame)
        checkbox_frame.grid(row=8, column=0, columnspan=3, sticky='w', pady=2)
        ttk.Checkbutton(checkbox_frame, text="Draw Agent Trace", variable=self.draw_trace_var).pack(side="left", padx=5)
        ttk.Checkbutton(checkbox_frame, text="Permanent Trace", variable=self.permanent_trace_var).pack(side="left", padx=5)
        ttk.Checkbutton(checkbox_frame, text="Enable Logging", variable=self.enable_logging_var).pack(side="left", padx=5)

        btns = ttk.Frame(main)
        btns.grid(row=4, column=0, sticky='ew', pady=(10, 10))
        btns.grid_columnconfigure((0, 1, 2), weight=1)
        self.start_button = ttk.Button(btns, text="Start Visual Sim", command=self.start_simulation)
        self.start_button.grid(row=0, column=0, sticky='ew', padx=5)
        self.stop_button = ttk.Button(btns, text="Stop Visual Sim", command=self.stop_simulation, state="disabled")
        self.stop_button.grid(row=0, column=1, sticky='ew', padx=5)
        ttk.Button(btns, text="Quit App", command=self.quit_application).grid(row=0, column=2, sticky='ew', padx=5)

        # Store reference to main frame for scroll updates
        self.controls_main_frame = main

        # Force initial canvas update after widgets are created
        def _init_canvas():
            main.update_idletasks()
            # Use frame's required height + 20% slack for scroll region
            req_height = main.winfo_reqheight()
            req_width = main.winfo_reqwidth()
            self.controls_canvas.configure(scrollregion=(0, 0, req_width, int(req_height * 1.2)))
            self.controls_canvas.itemconfigure(self.controls_scrollable_frame_id, width=self.controls_canvas.winfo_width())

        self.root.after(300, _init_canvas)

    def _on_agent_type_change(self, *args):
        sel_type = self.agent_type_var.get()
        self.notebook.tab(self.composite_config_tab, state="normal" if sel_type == "composite" else "disabled")
        
        # Always remove the old frame first
        if self.specific_agent_params_frame:
            self.specific_agent_params_frame.grid_forget()
            for w in self.specific_agent_params_frame.winfo_children(): w.destroy()

        if sel_type == "stochastic":
            self.specific_agent_params_frame.grid(row=3, column=0, sticky='ew', pady=(5,0), in_=self.agent_sel.master.master) # Place in main frame
            self.specific_agent_params_frame.grid_columnconfigure(1, weight=1)
            self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["stoch_update_interval_sec"], "Re-randomize Interval (s)", 0.1, 5.0, 0, 0.1)

        elif sel_type in ["inverse_turn", "composite"]:
            self.specific_agent_params_frame.grid(row=3, column=0, sticky='ew', pady=(5,0), in_=self.agent_sel.master.master)
            self.specific_agent_params_frame.grid_columnconfigure(0, weight=1)
            self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["turn_decision_interval_sec"], "Turn Update Interval (s)", 0.1, 5.0, 0, 0.1)
            
            if sel_type == "inverse_turn":
                thresh_frame = ttk.LabelFrame(self.specific_agent_params_frame, text="Thresholds")
                thresh_frame.grid(row=1, column=0, sticky='ew', padx=5, pady=3)
                thresh_frame.grid_columnconfigure(1, weight=1)
                self._add_slider_entry(thresh_frame, self._tk_vars["inv_threshold_r"], "Right Threshold (F/s)", 0.0, 20.0, 0, 0.1)
                self._add_slider_entry(thresh_frame, self._tk_vars["inv_threshold_l"], "Left Threshold (F/s)", 0.0, 20.0, 1, 0.1)
                
                sides_pane = ttk.PanedWindow(self.specific_agent_params_frame, orient=tk.HORIZONTAL)
                sides_pane.grid(row=2, column=0, sticky='nsew', padx=5, pady=5)
                self.specific_agent_params_frame.grid_rowconfigure(2, weight=1)
                
                right_frame = ttk.LabelFrame(sides_pane, text="Right Side Controls", padding=5)
                right_frame.grid_columnconfigure(0, weight=1)
                sides_pane.add(right_frame, weight=1)
                
                r_low_frame = ttk.LabelFrame(right_frame, text="Low Food Conditions")
                r_low_frame.grid(row=0, column=0, sticky='ew', padx=5, pady=3)
                r_low_frame.grid_columnconfigure(1, weight=1)
                self._create_frequency_slider(r_low_frame, self._tk_vars["inv_r_low_freq_hz"], "Frequency (Hz)", 0.1, 10.0, 0, 0.1)
                self._add_slider_entry(r_low_frame, self._tk_vars["inv_r_low_amp"], "Amplitude", 0.1, 20.0, 2, 0.1, lbl_width=15)
                
                r_high_frame = ttk.LabelFrame(right_frame, text="High Food Conditions")
                r_high_frame.grid(row=1, column=0, sticky='ew', padx=5, pady=3)
                r_high_frame.grid_columnconfigure(1, weight=1)
                self._create_frequency_slider(r_high_frame, self._tk_vars["inv_r_high_freq_hz"], "Frequency (Hz)", 0.1, 10.0, 0, 0.1)
                self._add_slider_entry(r_high_frame, self._tk_vars["inv_r_high_amp"], "Amplitude", 0.1, 20.0, 2, 0.1, lbl_width=15)

                left_frame = ttk.LabelFrame(sides_pane, text="Left Side Controls", padding=5)
                left_frame.grid_columnconfigure(0, weight=1)
                sides_pane.add(left_frame, weight=1)

                l_low_frame = ttk.LabelFrame(left_frame, text="Low Food Conditions")
                l_low_frame.grid(row=0, column=0, sticky='ew', padx=5, pady=3)
                l_low_frame.grid_columnconfigure(1, weight=1)
                self._create_frequency_slider(l_low_frame, self._tk_vars["inv_l_low_freq_hz"], "Frequency (Hz)", 0.1, 10.0, 0, 0.1)
                self._add_slider_entry(l_low_frame, self._tk_vars["inv_l_low_amp"], "Amplitude", 0.1, 20.0, 2, 0.1, lbl_width=15)

                l_high_frame = ttk.LabelFrame(left_frame, text="High Food Conditions")
                l_high_frame.grid(row=1, column=0, sticky='ew', padx=5, pady=3)
                l_high_frame.grid_columnconfigure(1, weight=1)
                self._create_frequency_slider(l_high_frame, self._tk_vars["inv_l_high_freq_hz"], "Frequency (Hz)", 0.1, 10.0, 0, 0.1)
                self._add_slider_entry(l_high_frame, self._tk_vars["inv_l_high_amp"], "Amplitude", 0.1, 20.0, 2, 0.1, lbl_width=15)

        # Update scroll region after adding/removing dynamic controls
        if hasattr(self, 'controls_canvas') and self.controls_canvas:
            def _update_scroll():
                if hasattr(self, 'controls_main_frame'):
                    self.controls_main_frame.update_idletasks()
                    req_height = self.controls_main_frame.winfo_reqheight()
                    req_width = self.controls_main_frame.winfo_reqwidth()
                    self.controls_canvas.configure(scrollregion=(0, 0, req_width, int(req_height * 1.2)))
            self.root.after(200, _update_scroll)

    def _rebuild_composite_gui(self):
        for w in self.composite_scrollable_frame.winfo_children(): w.destroy()
        for i, pair_vars in enumerate(self.composite_layer_pair_vars):
            pair_frame = ttk.LabelFrame(self.composite_scrollable_frame, text=f"Layer Pair {i+1}", padding=10)
            pair_frame.grid(row=i, column=0, sticky='ew', padx=10, pady=5)
            self.composite_scrollable_frame.grid_columnconfigure(0, weight=1)

            top_bar = ttk.Frame(pair_frame)
            top_bar.grid(row=0, column=0, sticky='ew')
            top_bar.grid_columnconfigure(0, weight=1)
            ttk.Button(top_bar, text="Remove This Pair (X)", command=lambda v=pair_vars: self._remove_composite_layer_pair(v)).grid(row=0, column=0, sticky='ne')
            
            paned = ttk.PanedWindow(pair_frame, orient=tk.HORIZONTAL)
            paned.grid(row=1, column=0, sticky='ew')
            pair_frame.grid_columnconfigure(0, weight=1)

            left_side_frame = ttk.LabelFrame(paned, text="Left Side", padding=5)
            left_side_frame.grid_columnconfigure(1, weight=1)
            self._add_slider_entry(left_side_frame, pair_vars['l_threshold_hz'], "Threshold (F/s)", 0.1, 30, 0, 0.1, lbl_width=15)
            self._add_slider_entry(left_side_frame, pair_vars['l_amp'], "Amplitude", -20, 20, 1, 0.1, lbl_width=15)
            self._create_frequency_slider(left_side_frame, pair_vars['l_frequency_hz'], "Frequency (Hz)", 0.1, 10.0, 2, 0.1)
            paned.add(left_side_frame, weight=1)
            
            right_side_frame = ttk.LabelFrame(paned, text="Right Side", padding=5)
            right_side_frame.grid_columnconfigure(1, weight=1)
            self._add_slider_entry(right_side_frame, pair_vars['r_threshold_hz'], "Threshold (F/s)", 0.1, 30, 0, 0.1, lbl_width=15)
            self._add_slider_entry(right_side_frame, pair_vars['r_amp'], "Amplitude", -20, 20, 1, 0.1, lbl_width=15)
            self._create_frequency_slider(right_side_frame, pair_vars['r_frequency_hz'], "Frequency (Hz)", 0.1, 10.0, 2, 0.1)
            paned.add(right_side_frame, weight=1)

    def _add_composite_layer_pair(self):
        new_pair = {'l_threshold_hz': VarD(2.0), 'l_amp': VarD(5.0), 'l_frequency_hz': VarD(1.0), 'r_threshold_hz': VarD(2.0), 'r_amp': VarD(5.0), 'r_frequency_hz': VarD(1.0)}
        self.composite_layer_pair_vars.append(new_pair); self._rebuild_composite_gui()
    def _get_main_agent_specific_params(self, headless=False) -> Dict[str, Any]:
        agent_type = self.agent_type_var.get() if not headless else self.headless_agent_type_var.get()
        params = {}
        if agent_type == "stochastic":
            params['update_interval_sec'] = self._tk_vars["stoch_update_interval_sec"].get()
        elif agent_type == "inverse_turn":
            params['turn_decision_interval_sec'] = self._tk_vars["turn_decision_interval_sec"].get()
            params['threshold_r'] = self._tk_vars["inv_threshold_r"].get()
            params['threshold_l'] = self._tk_vars["inv_threshold_l"].get()
            params['r1_amp'] = self._tk_vars["inv_r_low_amp"].get()
            params['r2_amp'] = self._tk_vars["inv_r_high_amp"].get()
            params['l1_amp'] = self._tk_vars["inv_l_low_amp"].get()
            params['l2_amp'] = self._tk_vars["inv_l_high_amp"].get()
            
            freq_map = {
                'r1_period_s': "inv_r_low_freq_hz", 'r2_period_s': "inv_r_high_freq_hz",
                'l1_period_s': "inv_l_low_freq_hz", 'l2_period_s': "inv_l_high_freq_hz",
            }
            for key_period, key_freq in freq_map.items():
                freq = self._tk_vars[key_freq].get()
                params[key_period] = 1.0 / freq if freq > 1e-6 else float('inf')

        elif agent_type == "composite":
            params['turn_decision_interval_sec'] = self._tk_vars["turn_decision_interval_sec"].get()
            layer_pairs_for_agent = []
            for gui_pair in self.composite_layer_pair_vars:
                agent_pair = {}
                for key, tk_var in gui_pair.items():
                    val = tk_var.get()
                    if key.endswith('_frequency_hz'):
                        period_key = key.replace('_frequency_hz', '_period_s')
                        agent_pair[period_key] = 1.0 / val if val > 1e-6 else float('inf')
                    else:
                        agent_pair[key] = val
                layer_pairs_for_agent.append(agent_pair)
            params['layer_pairs'] = layer_pairs_for_agent
        return params

    def _build_composite_config_tab(self, parent_tab):
        parent_tab.grid_rowconfigure(1, weight=1)
        parent_tab.grid_columnconfigure(0, weight=1)

        controls_frame = ttk.Frame(parent_tab)
        controls_frame.grid(row=0, column=0, columnspan=2, sticky='ew', padx=5, pady=5)
        ttk.Button(controls_frame, text="Add Layer Pair", command=self._add_composite_layer_pair).pack()
        
        self.composite_config_canvas = tk.Canvas(parent_tab, borderwidth=0)
        scrollbar = ttk.Scrollbar(parent_tab, orient="vertical", command=self.composite_config_canvas.yview)
        self.composite_config_canvas.configure(yscrollcommand=scrollbar.set)
        
        self.composite_config_canvas.grid(row=1, column=0, sticky='nsew')
        scrollbar.grid(row=1, column=1, sticky='ns')

        self.composite_scrollable_frame = ttk.Frame(self.composite_config_canvas)
        self.composite_scrollable_frame_id = self.composite_config_canvas.create_window((0, 0), window=self.composite_scrollable_frame, anchor="nw")
        
        def _configure_canvas(event):
            self.composite_config_canvas.configure(scrollregion=self.composite_config_canvas.bbox("all"))
            if self.composite_scrollable_frame.winfo_reqwidth() != self.composite_config_canvas.winfo_width():
                self.composite_config_canvas.itemconfigure(self.composite_scrollable_frame_id, width=self.composite_config_canvas.winfo_width())

        self.composite_scrollable_frame.bind("<Configure>", _configure_canvas)
        self._add_composite_layer_pair()

    def _remove_composite_layer_pair(self, vars_to_remove):
        if len(self.composite_layer_pair_vars) > 1: self.composite_layer_pair_vars.remove(vars_to_remove); self._rebuild_composite_gui()
        else: messagebox.showwarning("Cannot Remove", "The composite agent must have at least one layer pair.")

    def _build_headless_tab(self, parent_tab):
        parent_tab.grid_columnconfigure(0, weight=1)
        main = ttk.Frame(parent_tab, padding=10)
        main.grid(row=0, column=0, sticky='nsew')
        main.grid_columnconfigure(0, weight=1)
        
        conf = ttk.LabelFrame(main, text="Headless Instance Configuration")
        conf.grid(row=0, column=0, sticky='ew', pady=5, padx=5)
        conf.grid_columnconfigure(1, weight=1)

        row = 0
        ttk.Label(conf, text="Agent Type:", width=20).grid(row=row, column=0, sticky='w', pady=3)
        ttk.Combobox(conf, values=list(main_agent_classes.keys()), state="readonly", width=20, textvariable=self.headless_agent_type_var).grid(row=row, column=1, sticky='ew', padx=5)
        row += 1
        ttk.Label(conf, text="Expire Timer (s):", width=20).grid(row=row, column=0, sticky='w', pady=3)
        ttk.Spinbox(conf, from_=5, to=3600, textvariable=self.headless_expiry_timer_var, width=10).grid(row=row, column=1, sticky='ew', padx=5)
        row += 1
        ttk.Label(conf, text="# Agents/Instance:", width=20).grid(row=row, column=0, sticky='w', pady=3)
        ttk.Spinbox(conf, from_=1, to=200, textvariable=self.headless_num_agents_var, width=10).grid(row=row, column=1, sticky='ew', padx=5)
        row += 1
        ttk.Label(conf, text="# Instances:", width=20).grid(row=row, column=0, sticky='w', pady=3)
        ttk.Spinbox(conf, from_=1, to=1000, textvariable=self.headless_num_instances_var, width=10).grid(row=row, column=1, sticky='ew', padx=5)

        self.start_headless_button = ttk.Button(main, text="Start Headless Runs", command=self.start_headless_runs)
        self.start_headless_button.grid(row=1, column=0, sticky='ew', pady=10)
        
        res = ttk.LabelFrame(main, text="Results")
        res.grid(row=2, column=0, sticky='ew', pady=5, padx=5)
        ttk.Label(res, textvariable=self.headless_status_var).pack(anchor="w", pady=2)
        ttk.Label(res, textvariable=self.headless_avg_eff_var).pack(anchor="w", pady=2)
        ttk.Label(res, textvariable=self.headless_median_eff_var).pack(anchor="w", pady=2)

    def _build_status_tab(self, parent_tab):
        parent_tab.grid_rowconfigure(0, weight=1)
        parent_tab.grid_columnconfigure(0, weight=1)

        # Create scrollable canvas for status tab
        self.status_canvas = tk.Canvas(parent_tab, borderwidth=0, highlightthickness=0, bg='#f0f0f0')
        status_scrollbar = ttk.Scrollbar(parent_tab, orient="vertical", command=self.status_canvas.yview)
        self.status_canvas.configure(yscrollcommand=status_scrollbar.set)

        self.status_canvas.grid(row=0, column=0, sticky='nsew')
        status_scrollbar.grid(row=0, column=1, sticky='ns')

        main = ttk.Frame(self.status_canvas, padding=10)
        self.status_scrollable_frame_id = self.status_canvas.create_window((0, 0), window=main, anchor="nw")
        main.grid_columnconfigure(0, weight=1)

        def _on_status_frame_configure(event):
            # Use frame's required height + 20% slack for scroll region
            req_height = main.winfo_reqheight()
            self.status_canvas.configure(scrollregion=(0, 0, main.winfo_reqwidth(), int(req_height * 1.2)))

        def _on_status_canvas_configure(event):
            self.status_canvas.itemconfigure(self.status_scrollable_frame_id, width=event.width)

        main.bind("<Configure>", _on_status_frame_configure)
        self.status_canvas.bind("<Configure>", _on_status_canvas_configure)

        # Mousewheel scrolling for status tab
        def _on_status_mousewheel(event):
            self.status_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        def _bind_status_mousewheel(event):
            self.status_canvas.bind_all("<MouseWheel>", _on_status_mousewheel)

        def _unbind_status_mousewheel(event):
            self.status_canvas.unbind_all("<MouseWheel>")

        self.status_canvas.bind("<Enter>", _bind_status_mousewheel)
        self.status_canvas.bind("<Leave>", _unbind_status_mousewheel)

        # Live Sim Status
        stat = ttk.LabelFrame(main, text="Live Sim Status", padding=10)
        stat.grid(row=0, column=0, sticky='ew', pady=5)
        stat.grid_columnconfigure(0, weight=1)
        
        self.sim_status_var = tk.StringVar(value="Simulation: Idle")
        ttk.Label(stat, textvariable=self.sim_status_var, font=("Arial", 12)).grid(row=0, column=0, sticky='w', pady=5)
        self.total_food_var = tk.StringVar(value="Total Food: N/A")
        ttk.Label(stat, textvariable=self.total_food_var).grid(row=1, column=0, sticky='w', pady=2)
        self.agents_active_var = tk.StringVar(value="Active Agents: N/A")
        ttk.Label(stat, textvariable=self.agents_active_var).grid(row=2, column=0, sticky='w', pady=2)
        self.food_eaten_var = tk.StringVar(value="Food Eaten: N/A")
        ttk.Label(stat, textvariable=self.food_eaten_var).grid(row=3, column=0, sticky='w', pady=2)
        self.efficiency_var = tk.StringVar(value="Efficiency (F/s): N/A")
        ttk.Label(stat, textvariable=self.efficiency_var).grid(row=4, column=0, sticky='w', pady=2)
        self.agent_input_freq_var = tk.StringVar(value="Input Freq (Hz): N/A")
        ttk.Label(stat, textvariable=self.agent_input_freq_var).grid(row=5, column=0, sticky='w', pady=2)
        
        # Event Log
        log_f = ttk.LabelFrame(main, text="Event Log")
        log_f.grid(row=1, column=0, sticky='ew', pady=5)
        log_f.grid_rowconfigure(0, weight=1)
        log_f.grid_columnconfigure(0, weight=1)
        
        self.log_text = tk.Text(log_f, height=5, state="disabled", wrap="word")
        scroll = ttk.Scrollbar(log_f, command=self.log_text.yview)
        self.log_text.config(yscrollcommand=scroll.set)
        
        self.log_text.grid(row=0, column=0, sticky='nsew')
        scroll.grid(row=0, column=1, sticky='ns')

        # Analyze button
        ttk.Button(main, text="Analyze Log File...", command=self._analyze_log_file).grid(row=2, column=0, pady=5, sticky='ew')

        # Graph Area
        graph_frame = ttk.LabelFrame(main, text="Performance History")
        graph_frame.grid(row=3, column=0, sticky='ew', pady=5)
        graph_frame.grid_columnconfigure(0, weight=1)

        if HAS_MATPLOTLIB:
            self.fig = Figure(figsize=(6, 5), dpi=100)
            
            # Create 2 subplots vertically
            self.ax_counts = self.fig.add_subplot(211)
            self.ax_rate = self.fig.add_subplot(212, sharex=self.ax_counts)
            
            # Adjust spacing to prevent overlap
            self.fig.subplots_adjust(hspace=0.4, bottom=0.15, left=0.15)
            
            self.canvas = FigureCanvasTkAgg(self.fig, master=graph_frame)
            self.canvas.draw()
            self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.X, expand=False)
        else:
            ttk.Label(graph_frame, text="Matplotlib not found. Graphs disabled.").pack(expand=True)

        self._add_log_message("GUI Initialized.")

        # Store reference for scroll updates
        self.status_main_frame = main

        # Force initial canvas update
        def _init_status_canvas():
            main.update_idletasks()
            # Use frame's required height + 20% slack for scroll region
            req_height = main.winfo_reqheight()
            req_width = main.winfo_reqwidth()
            self.status_canvas.configure(scrollregion=(0, 0, req_width, int(req_height * 1.2)))
            self.status_canvas.itemconfigure(self.status_scrollable_frame_id, width=self.status_canvas.winfo_width())

        self.root.after(300, _init_status_canvas)

    def _periodic_status_update(self):
        if not self.root.winfo_exists(): return
        
        if self.running_simulation and not self.headless_run_active:
            stats = get_simulation_stats()
            self.total_food_var.set(f"Total Food: {stats.get('total_food', 'N/A')}")
            self.agents_active_var.set(f"Active Agents: {stats.get('active_agents', 'N/A')}")
            self.food_eaten_var.set(f"Food Eaten: {stats.get('food_eaten_run', 'N/A')}")
            
            agent_type = stats.get('agent_type')
            if agent_type in ['inverse_turn', 'composite']: 
                self.agent_input_freq_var.set(f"Input Freq (Hz): {stats.get('input_food_frequency', 0.0):.2f}")
            else: 
                self.agent_input_freq_var.set("Input Freq (Hz): N/A")
            
            if self.simulation_start_time_visual:
                elapsed = time.time() - self.simulation_start_time_visual
                self.efficiency_var.set(f"Efficiency (F/s): {stats.get('food_eaten_run', 0) / elapsed if elapsed > 0.01 else 0.0:.2f}")

            # NEW: Graph Update
            if HAS_MATPLOTLIB and self.fig and 'history' in stats:
                history = stats['history']
                
                # BUG FIX: Create snapshot copies of the lists to avoid threading race conditions
                # The main thread writes to these lists while the GUI reads them, so length can mismatch.
                raw_times = history.get('time', [])
                raw_eaten = history.get('food_eaten', [])
                raw_available = history.get('food_available', [])
                raw_decisions = history.get('decisions', [])

                # Calculate the safe minimum length in case one list was updated ahead of the others
                min_len = min(len(raw_times), len(raw_eaten), len(raw_available), len(raw_decisions))
                
                # Slice and list() to ensure we have a static copy for calculation/plotting
                times = list(raw_times[:min_len])
                eaten = list(raw_eaten[:min_len])
                available = list(raw_available[:min_len])
                decisions = list(raw_decisions[:min_len])

                if len(times) > 1:
                    # Calculate consumption rate (windowed derivative over ~1 sec)
                    window = 20
                    rates = []
                    for i in range(len(times)):
                        if i < window:
                            rates.append(0.0)
                        else:
                            dt = times[i] - times[i-window]
                            d_food = eaten[i] - eaten[i-window]
                            rate = d_food / dt if dt > 0 else 0.0
                            rates.append(rate)

                    self.ax_counts.clear()
                    self.ax_rate.clear()

                    # --- Top Plot: Cumulative Metrics ---
                    self.ax_counts.plot(times, eaten, label="Total Eaten", color="green")
                    self.ax_counts.plot(times, decisions, label="Decisions", color="blue", linestyle="--")
                    self.ax_counts.plot(times, available, label="Food Avail", color="gray", alpha=0.5)
                    
                    self.ax_counts.set_ylabel("Count")
                    self.ax_counts.set_title("Cumulative Metrics", fontsize=10)
                    self.ax_counts.legend(loc="upper left", fontsize="small")
                    self.ax_counts.grid(True, alpha=0.3)
                    
                    # --- Bottom Plot: Rates ---
                    self.ax_rate.plot(times, rates, label="Consumption Rate", color="red", linestyle="-.")
                    
                    self.ax_rate.set_ylabel("Rate (F/s)")
                    self.ax_rate.set_xlabel("Time (s)")
                    self.ax_rate.set_title("Performance Rate", fontsize=10)
                    self.ax_rate.legend(loc="upper left", fontsize="small")
                    self.ax_rate.grid(True, alpha=0.3)

                    self.canvas.draw()

        # Update every 500ms (graphing is expensive)
        self.root.after(500, self._periodic_status_update)

    def _apply_settings(self, for_headless=False) -> bool:
        self._add_log_message("Applying settings...")
        try:
            set_frame_dt(1.0 / self._tk_vars["fps"].get()); cfg.set_global_speed_modifier(self._tk_vars["global_speed_modifier"].get())
            set_agent_speed_scaling_factor(self._tk_vars["agent_speed_scaling_factor"].get()); set_angular_proportionality_constant(self._tk_vars["angular_proportionality_constant"].get())
            agent_type = self.agent_type_var.get() if not for_headless else self.headless_agent_type_var.get(); params = self._get_main_agent_specific_params(headless=for_headless)
            main_configure(
                food_preset=self.food_sel.get(), agent_type=agent_type,
                n_agents=int(self._tk_vars["n_agents"].get()) if not for_headless else self.headless_num_agents_var.get(),
                spawn_point=(self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()),
                initial_direction_deg=self._tk_vars["initial_direction_deg"].get(),
                agent_params_for_main_agent=params, draw_trace=self.draw_trace_var.get(),
                permanent_trace=self.permanent_trace_var.get(),
                log_file_handle=self.active_log_file
            )
            self._add_log_message(f"World configured for agent: {agent_type}"); return True
        except Exception as e: self._add_log_message(f"Config Error: {e}"); messagebox.showerror("Config Error", f"Failed to apply settings:\n{e}"); return False
    def start_simulation(self):
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run active."); return
        if self.running_simulation: self.stop_simulation()
        if self.enable_logging_var.get():
            ts = time.strftime('%Y%m%d-%H%M%S'); fname = f"raw_output_{self.agent_type_var.get()}_{self.food_sel.get()}_{ts}.log"
            try: self.active_log_file = open(fname, 'w'); self._add_log_message(f"Logging to {fname}"); self._write_log_header()
            except IOError as e: self._add_log_message(f"Error opening log file: {e}"); self.active_log_file = None
        if not self._apply_settings(): 
            if self.active_log_file: self.active_log_file.close(); self.active_log_file = None
            return
        self.running_simulation = True; self.simulation_start_time_visual = time.time(); self.start_button.config(state="disabled"); self.stop_button.config(state="normal"); self.sim_status_var.set("Simulation: Starting...")
        self.sim_thread = threading.Thread(target=self._simulation_loop_thread, daemon=True); self.sim_thread.start()
    def stop_simulation(self):
        if not self.running_simulation: return
        self.running_simulation = False
        if self.sim_thread and self.sim_thread.is_alive(): self.sim_thread.join(timeout=1.0)
        self.sim_thread = None; self.simulation_start_time_visual = None
        if self.active_log_file and not self.active_log_file.closed:
            self.active_log_file.write(f"# FINAL_STATS: food_eaten={get_simulation_stats().get('food_eaten_run', 0)}\n")
            self._add_log_message(f"Closing log file."); self.active_log_file.close(); self.active_log_file = None
        self.start_button.config(state="normal"); self.stop_button.config(state="disabled"); self.sim_status_var.set("Simulation: Idle")

    def _simulation_loop_thread(self):
        pygame.init()
        try:
            self.pygame_surface = pygame.display.set_mode((cfg.WINDOW_W, cfg.WINDOW_H))
            pygame.display.set_caption("Agent Sim")
            self.root.after(0, lambda: self.sim_status_var.set("Simulation: Running"))
        except pygame.error as e:
            self.root.after(0, lambda: messagebox.showerror("Pygame Error", str(e)))
            if pygame.get_init():
                pygame.quit()
            self.running_simulation = False
            return

        main_init_pygame_surface(self.pygame_surface)
        clock = pygame.time.Clock()

        while self.running_simulation:
            try:
                for event in pygame.event.get():
                    if event.type == pygame.QUIT:
                        self.root.after(0, self.stop_simulation)
                        break
                if not self.running_simulation:
                    break
                
                main_run_frame(is_visual=True)
                pygame.display.flip()
                
                try:
                    clock.tick(self._tk_vars["fps"].get())
                except tk.TclError:
                    self.running_simulation = False

            except Exception as e:
                tb_str = traceback.format_exc()
                error_msg = f"A critical error occurred in the simulation thread:\n\n{e}\n\n{tb_str}"
                print(error_msg) 
                self.root.after(0, lambda: messagebox.showerror("Simulation Runtime Error", error_msg))
                self.root.after(0, self.stop_simulation)
                break

        if pygame.get_init():
            pygame.quit()
        self.pygame_surface = None

    def start_headless_runs(self):
        if self.running_simulation or self.headless_run_active: messagebox.showwarning("Busy", "A simulation is already active."); return
        self.headless_run_active = True; self.start_headless_button.config(state="disabled"); self.headless_status_var.set("Status: Starting...")
        threading.Thread(target=self._headless_run_worker_thread, daemon=True).start()
    def _headless_run_worker_thread(self):
        try:
            num_instances, expiry_seconds, fps = self.headless_num_instances_var.get(), self.headless_expiry_timer_var.get(), self._tk_vars["fps"].get()
            frames_per_instance = math.ceil(expiry_seconds * fps) if fps > 0 else 0
            self.root.after(0, lambda: self._add_log_message(f"Headless: {num_instances} runs, {expiry_seconds}s each.")); scores = []
            for i in range(num_instances):
                if not self.headless_run_active: self.root.after(0, lambda: self._add_log_message("Headless run aborted.")); break
                self.root.after(0, lambda i=i: self.headless_status_var.set(f"Status: Running instance {i+1}/{num_instances}")); self._apply_settings(for_headless=True); main_init_pygame_surface(None)
                for _ in range(frames_per_instance): main_run_frame(is_visual=False)
                stats = get_simulation_stats(); eff = stats.get("food_eaten_run", 0) / expiry_seconds if expiry_seconds > 0 else 0; scores.append(eff)
                self.root.after(0, lambda f=stats.get("food_eaten_run",0), e=eff: self._add_log_message(f"Instance {i+1} done. Food: {f}, Eff: {e:.3f}"))
            if scores:
                avg, med = mean(scores), median(scores)
                self.root.after(0, lambda: self.headless_avg_eff_var.set(f"Avg. Efficiency: {avg:.3f} F/s")); self.root.after(0, lambda: self.headless_median_eff_var.set(f"Median Efficiency: {med:.3f} F/s"))
        except Exception as e: self.root.after(0, lambda: self._add_log_message(f"Error in headless run: {e}"))
        finally: self.root.after(0, lambda: self.start_headless_button.config(state="normal")); self.root.after(0, lambda: self.headless_status_var.set("Status: Idle")); self.headless_run_active = False
    def _write_log_header(self):
        if not self.active_log_file: return
        f = self.active_log_file; f.write(f"# Simulation Log - {time.strftime('%Y-%m-%d %H:%M:%S')}\n"); f.write(f"# --- CONFIGURATION ---\n"); agent_type = self.agent_type_var.get(); f.write(f"# Agent Type: {agent_type}\n"); food_preset = self.food_sel.get(); f.write(f"# Food Preset: {food_preset}\n")
        try: f.write(f"# initial_food_count: {len(cfg.FOOD_PRESETS[food_preset]())}\n")
        except Exception: f.write(f"# initial_food_count: 0\n")
        params = {"num_agents": self._tk_vars["n_agents"].get(), "fps": self._tk_vars["fps"].get(), "global_speed_modifier": self._tk_vars["global_speed_modifier"].get(), "agent_speed_scaling_factor": self._tk_vars["agent_speed_scaling_factor"].get(), "angular_proportionality_constant": self._tk_vars["angular_proportionality_constant"].get(), "spawn_point": (self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()), "initial_direction_deg": self._tk_vars["initial_direction_deg"].get(),}
        agent_specific_params = self._get_main_agent_specific_params()
        if agent_type == 'composite': f.write(f"# agent_params: \n# {json.dumps(agent_specific_params, indent=4).replace(chr(10), chr(10)+'# ')}\n")
        elif agent_specific_params: params.update(agent_specific_params)
        for key, value in params.items():
            if agent_type == 'composite' and key == 'agent_params': continue
            f.write(f"# {key}: {value}\n")
        f.write(f"# --- DATA ---\n"); f.write(f"# frame,agent_id,pos_x,pos_y,angle_rad,delta_dist,delta_theta_rad\n")
    def _analyze_log_file(self):
        filepath = filedialog.askopenfilename(title="Select a Log File to Analyze", filetypes=[("Log files", "*.log"), ("All files", "*.*")])
        if not filepath: return
        try:
            with open(filepath, 'r') as f: lines = f.readlines()
            header = [line for line in lines if line.startswith("#")]; data = [line for line in lines if not line.startswith("#")]
            fps_match = re.search(r'# fps: (\d+)', "".join(header)); food_eaten_match = re.search(r'# FINAL_STATS: food_eaten=(\d+)', "".join(header))
            if not fps_match or not food_eaten_match: raise ValueError("Log file missing required stats.")
            fps, food_eaten = int(fps_match.group(1)), int(food_eaten_match.group(1)); total_distance, last_frame = 0.0, 0
            for line in data:
                parts = line.strip().split(',');
                if len(parts) < 7: continue
                last_frame = int(parts[0])
                if int(parts[1]) == 0: total_distance += float(parts[5])
            total_time_sec = last_frame / fps if fps > 0 else 0; food_per_sec = food_eaten / total_time_sec if total_time_sec > 0 else 0; dist_per_sec = total_distance / total_time_sec if total_time_sec > 0 else 0
            food_per_1k_pixels = (food_eaten / total_distance) * 1000 if total_distance > 0 else 0
            result_str = (f"Log Analysis: {filepath.split('/')[-1]}\n------------------------------------\n"
                f"Total Time: {total_time_sec:.2f} seconds\nTotal Food Eaten: {food_eaten}\nTotal Distance Traveled: {total_distance:.2f} pixels\n"
                f"------------------------------------\nEfficiency (Food/Sec): {food_per_sec:.3f}\nPath Efficiency (Food/1000px): {food_per_1k_pixels:.3f}\n"
                f"Average Speed (px/sec): {dist_per_sec:.2f}\n")
            messagebox.showinfo("Log Analysis Results", result_str)
        except Exception as e: messagebox.showerror("Analysis Error", f"Could not parse the log file.\nError: {e}")
    def _import_preset_config(self):
        filepath = filedialog.askopenfilename(
            title="Select a Preset Config File",
            filetypes=[
                ("Preset & Log Files", ("*.txt", "*.log")),
                ("All files", "*.*")
            ]
        )
        if not filepath: return
        try:
            with open(filepath, 'r') as f: lines = f.readlines()
            self._parse_and_apply_preset(lines)
            self._add_log_message(f"Successfully loaded preset from {filepath.split('/')[-1]}")
            messagebox.showinfo("Preset Loaded", "Configuration loaded successfully.")
        except Exception as e:
            self._add_log_message(f"Error loading preset: {e}")
            traceback.print_exc()
            messagebox.showerror("Preset Load Error", f"Could not parse the preset file.\nError: {e}")

    def _parse_and_apply_preset(self, lines: List[str]):
        key_map = {
            "num_agents": ("n_agents", int), "fps": ("fps", int),
            "global_speed_modifier": ("global_speed_modifier", float),
            "agent_speed_scaling_factor": ("agent_speed_scaling_factor", float),
            "angular_proportionality_constant": ("angular_proportionality_constant", float),
            "initial_direction_deg": ("initial_direction_deg", float),
            "update_interval_sec": ("stoch_update_interval_sec", float),
            "turn_decision_interval_sec": ("turn_decision_interval_sec", float),
            "threshold_r": ("inv_threshold_r", float), "threshold_l": ("inv_threshold_l", float),
            "r1_amp": ("inv_r_low_amp", float), "r2_amp": ("inv_r_high_amp", float),
            "l1_amp": ("inv_l_low_amp", float), "l2_amp": ("inv_l_high_amp", float),
        }
        period_to_freq_map = {
            "r1_period_s": "inv_r_low_freq_hz", "r2_period_s": "inv_r_high_freq_hz",
            "l1_period_s": "inv_l_low_freq_hz", "l2_period_s": "inv_l_high_freq_hz",
        }
        
        in_composite_json = False
        json_str_list = []
        brace_level = 0
        found_first_brace = False

        for line in lines:
            if in_composite_json:
                stripped_line = line.strip()
                if not stripped_line.startswith("#"):
                    in_composite_json = False
                else:
                    json_content = stripped_line[1:].lstrip()
                    if not found_first_brace and '{' in json_content: found_first_brace = True
                    if not found_first_brace: continue
                    json_str_list.append(json_content)
                    brace_level += json_content.count('{')
                    brace_level -= json_content.count('}')
                    if found_first_brace and brace_level == 0:
                        full_json_str = "".join(json_str_list)
                        try:
                            params = json.loads(full_json_str)
                            if 'turn_decision_interval_sec' in params: self._tk_vars['turn_decision_interval_sec'].set(params['turn_decision_interval_sec'])
                            self.composite_layer_pair_vars.clear()
                            for layer in params.get('layer_pairs', []):
                                new_pair = {}
                                l_period = layer.get('l_period_s', float('inf')); r_period = layer.get('r_period_s', float('inf'))
                                new_pair['l_threshold_hz'] = VarD(layer.get('l_threshold_hz', 0.0)); new_pair['l_amp'] = VarD(layer.get('l_amp', 0.0)); new_pair['l_frequency_hz'] = VarD(1.0 / l_period if l_period > 1e-9 else 0.0)
                                new_pair['r_threshold_hz'] = VarD(layer.get('r_threshold_hz', 0.0)); new_pair['r_amp'] = VarD(layer.get('r_amp', 0.0)); new_pair['r_frequency_hz'] = VarD(1.0 / r_period if r_period > 1e-9 else 0.0)
                                self.composite_layer_pair_vars.append(new_pair)
                            self._rebuild_composite_gui(); self.notebook.select(self.composite_config_tab)
                        except json.JSONDecodeError as e:
                            self._add_log_message(f"JSON parse error in preset: {e}")
                            messagebox.showerror("Preset Load Error", f"Could not parse composite agent JSON.\nError: {e}")
                        in_composite_json = False
                continue
            
            match = re.match(r'#\s*([\w\s_]+):\s*(.*)', line)
            if not match: continue
            
            key, value_str = match.groups(); key = key.strip().replace(" ", "_"); value_str = value_str.strip()
            if key == "agent_params" and value_str == "":
                in_composite_json = True; json_str_list = []; brace_level = 0; found_first_brace = False
            elif key == "Agent_Type": self.agent_type_var.set(value_str)
            elif key == "Food_Preset":
                if value_str in cfg.FOOD_PRESETS: self.food_sel.set(value_str)
            elif key == "spawn_point":
                try: x, y = eval(value_str); self._tk_vars["spawn_x"].set(float(x)); self._tk_vars["spawn_y"].set(float(y))
                except: pass
            elif key in key_map: var_name, caster = key_map[key]; self._tk_vars[var_name].set(caster(value_str))
            elif key in period_to_freq_map: period = float(value_str); freq = 1.0 / period if period > 1e-9 else 0.0; self._tk_vars[period_to_freq_map[key]].set(freq)

if __name__ == "__main__":
    try: app = SimGUI(); app.root.mainloop()
    except Exception as e: print(f"An error occurred: {e}"); import traceback; traceback.print_exc(); sys.exit(1)