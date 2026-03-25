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
from inverter import NNN_SINGLE_PRESET, NNN_COMPOSITE_PRESET, recalculate_composite

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
    get_simulation_stats, teleport_agent
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
            "spawn_x": VarD(120), "spawn_y": VarD(cfg.WINDOW_H / 2),  # Just at box edge
            "initial_direction_deg": VarD(-1.0), "stoch_update_interval_sec": VarD(1.0),
            # Single inverter params (C1-C4)
            "inverter_C1": VarD(NNN_SINGLE_PRESET['C1']), "inverter_C2": VarD(NNN_SINGLE_PRESET['C2']),
            "inverter_C3": VarD(NNN_SINGLE_PRESET['C3']), "inverter_C4": VarD(NNN_SINGLE_PRESET['C4']),
        }
        self.draw_trace_var = tk.BooleanVar(value=False); self.enable_logging_var = tk.BooleanVar(value=False)
        self.permanent_trace_var = tk.BooleanVar(value=False)
        # Inverter display mode: False = period (sec), True = frequency (Hz)
        self.inverter_freq_mode = tk.BooleanVar(value=False)
        # NNN Composite: list of inverter configs (C1, C2, C3, C4, crossed)
        self.composite_inverter_vars: List[Dict[str, tk.Variable]] = []
        self._f1_trace_ids: List[Tuple[tk.Variable, str]] = []  # Track f1 variable traces
        self._recalc_in_progress: bool = False  # Guard against recursive recalc
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
        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)
        self.root.protocol("WM_DELETE_WINDOW", self.quit_application); self._periodic_status_update(); self._on_agent_type_change()

    def _on_tab_changed(self, event):
        """Bind mousewheel scrolling to the active tab's canvas."""
        self.notebook.unbind_all("<MouseWheel>")
        selected = self.notebook.select()
        if selected == str(self.composite_config_tab) and hasattr(self, '_on_composite_mousewheel'):
            self.notebook.bind_all("<MouseWheel>", self._on_composite_mousewheel)
        elif selected == str(self.controls_tab) and hasattr(self, 'controls_canvas'):
            def _mw(e): self.controls_canvas.yview_scroll(int(-1 * (e.delta / 120)), "units")
            self.notebook.bind_all("<MouseWheel>", _mw)
        elif selected == str(self.status_tab) and hasattr(self, 'status_canvas'):
            def _mw(e): self.status_canvas.yview_scroll(int(-1 * (e.delta / 120)), "units")
            self.notebook.bind_all("<MouseWheel>", _mw)

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
        self._add_slider_entry(world_frame, self._tk_vars["initial_direction_deg"], "Initial Orientation (°, -1=rnd)", -1, 359, 6, res=1.0, lbl_width=28)
        
        ttk.Label(world_frame, text="Food Preset", width=28).grid(row=7, column=0, sticky='w', pady=2)
        self.food_sel = ttk.Combobox(world_frame, values=list(cfg.FOOD_PRESETS.keys()), state="readonly")
        self.food_sel.current(0)
        self.food_sel.grid(row=7, column=1, columnspan=2, sticky='ew', padx=5, pady=2)
        self.food_sel.bind("<<ComboboxSelected>>", self._on_food_preset_change)

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

        elif sel_type in ["inverter", "composite"]:
            self.specific_agent_params_frame.grid(row=3, column=0, sticky='ew', pady=(5,0), in_=self.agent_sel.master.master)
            self.specific_agent_params_frame.grid_columnconfigure(0, weight=1)
            self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["turn_decision_interval_sec"], "Turn Update Interval (s)", 0.1, 5.0, 0, 0.1)

            if sel_type == "inverter":
                # Single Inverter: base NNN unit with C1-C4 parameters
                inv_frame = ttk.LabelFrame(self.specific_agent_params_frame, text="Single Inverter (C1-C4)")
                inv_frame.grid(row=1, column=0, sticky='ew', padx=5, pady=3)
                inv_frame.grid_columnconfigure(1, weight=1)

                ttk.Checkbutton(inv_frame, text="Show as Freq (Hz)", variable=self.inverter_freq_mode,
                                command=self._on_freq_mode_toggle).grid(row=0, column=0, columnspan=3, sticky='w', padx=5)

                hz = self.inverter_freq_mode.get()
                unit = "Hz" if hz else "sec"
                lo, hi = (0.2, 10.0) if hz else (0.1, 5.0)
                self._add_slider_entry(inv_frame, self._tk_vars["inverter_C1"], f"C1 (L period/thr) {unit}", lo, hi, 1, 0.1)
                self._add_slider_entry(inv_frame, self._tk_vars["inverter_C2"], f"C2 (L high mode) {unit}", lo, hi, 2, 0.1)
                self._add_slider_entry(inv_frame, self._tk_vars["inverter_C3"], f"C3 (R period) {unit}", lo, hi, 3, 0.1)
                self._add_slider_entry(inv_frame, self._tk_vars["inverter_C4"], f"C4 (R high mode) {unit}", lo, hi, 4, 0.1)

        # Update scroll region after adding/removing dynamic controls
        if hasattr(self, 'controls_canvas') and self.controls_canvas:
            def _update_scroll():
                if hasattr(self, 'controls_main_frame'):
                    self.controls_main_frame.update_idletasks()
                    req_height = self.controls_main_frame.winfo_reqheight()
                    req_width = self.controls_main_frame.winfo_reqwidth()
                    self.controls_canvas.configure(scrollregion=(0, 0, req_width, int(req_height * 1.2)))
            self.root.after(200, _update_scroll)

    def _on_food_preset_change(self, event=None):
        preset = self.food_sel.get()
        hint = cfg.FOOD_SPAWN_HINTS.get(preset)
        if hint:
            self._tk_vars["spawn_x"].set(hint[0])
            self._tk_vars["spawn_y"].set(hint[1] * cfg.WINDOW_H)

    def _rebuild_composite_gui(self):
        """Rebuild GUI for NNN Inverter-based composite model."""
        for w in self.composite_scrollable_frame.winfo_children(): w.destroy()

        # Freq/period toggle at top of composite tab
        toggle_frame = ttk.Frame(self.composite_scrollable_frame)
        toggle_frame.grid(row=0, column=0, sticky='ew', padx=10, pady=(5, 0))
        ttk.Checkbutton(toggle_frame, text="Show as Freq (Hz)", variable=self.inverter_freq_mode,
                        command=self._on_freq_mode_toggle).grid(row=0, column=0, sticky='w')

        hz = self.inverter_freq_mode.get()
        unit = "Hz" if hz else "sec"

        # Shared slider scale across all inverters so progression is visually clear
        global_max = 1.0
        for inv_vars in self.composite_inverter_vars:
            for key in ['C1', 'C2', 'C3', 'C4']:
                global_max = max(global_max, inv_vars[key].get())
        hi = max(10.0, global_max * 1.3)
        lo = 0.01

        for i, inv_vars in enumerate(self.composite_inverter_vars):
            name = inv_vars['name'].get() or f"Inverter {i+1}"
            inv_frame = ttk.LabelFrame(self.composite_scrollable_frame, text=name, padding=10)
            inv_frame.grid(row=i + 1, column=0, sticky='ew', padx=10, pady=5)
            self.composite_scrollable_frame.grid_columnconfigure(0, weight=1)
            inv_frame.grid_columnconfigure(1, weight=1)

            top_bar = ttk.Frame(inv_frame)
            top_bar.grid(row=0, column=0, columnspan=4, sticky='ew')
            top_bar.grid_columnconfigure(0, weight=1)
            ttk.Checkbutton(top_bar, text="Crossed", variable=inv_vars['crossed']).grid(row=0, column=0, sticky='w')
            ttk.Button(top_bar, text="Remove (X)", command=lambda v=inv_vars: self._remove_inverter(v)).grid(row=0, column=1, sticky='ne')

            # C1-C4 parameters — shared scale across all inverters
            self._add_slider_entry(inv_frame, inv_vars['C1'], f"C1 (thr/low L) {unit}", lo, hi, 1, 0.1, lbl_width=20)
            self._add_slider_entry(inv_frame, inv_vars['C2'], f"C2 (high L) {unit}", lo, hi, 2, 0.1, lbl_width=20)
            self._add_slider_entry(inv_frame, inv_vars['C3'], f"C3 (low R) {unit}", lo, hi, 3, 0.1, lbl_width=20)
            self._add_slider_entry(inv_frame, inv_vars['C4'], f"C4 (high R) {unit}", lo, hi, 4, 0.1, lbl_width=20)

        # Update scroll region after rebuilding widgets
        self.composite_scrollable_frame.update_idletasks()
        req_height = self.composite_scrollable_frame.winfo_reqheight()
        req_width = self.composite_scrollable_frame.winfo_reqwidth()
        self.composite_config_canvas.configure(scrollregion=(0, 0, req_width, int(req_height * 1.2)))

        # Auto-recalc: trace f1's C1-C4 variables so crossed inverters update automatically
        # Remove old traces first
        for var, tid in self._f1_trace_ids:
            try:
                var.trace_remove('write', tid)
            except Exception:
                pass
        self._f1_trace_ids.clear()

        if self.composite_inverter_vars:
            f1_vars = self.composite_inverter_vars[0]
            for key in ['C1', 'C2', 'C3', 'C4']:
                var = f1_vars[key]
                tid = var.trace_add('write', lambda *_: self._auto_recalc_composite())
                self._f1_trace_ids.append((var, tid))

    def _on_freq_mode_toggle(self):
        """Toggle between Hz and sec display for all C1-C4 values."""
        # Convert all C values: new = 1/old
        for key in ["inverter_C1", "inverter_C2", "inverter_C3", "inverter_C4"]:
            old = self._tk_vars[key].get()
            if old > 0:
                self._tk_vars[key].set(round(1.0 / old, 4))

        for inv_vars in self.composite_inverter_vars:
            for key in ['C1', 'C2', 'C3', 'C4']:
                old = inv_vars[key].get()
                if old > 0:
                    inv_vars[key].set(round(1.0 / old, 4))

        # Rebuild GUI to update labels and slider ranges
        self._on_agent_type_change()
        if self.composite_inverter_vars:
            self._rebuild_composite_gui()

    def _c_display_to_period(self, val: float) -> float:
        """Convert a displayed C value to period (sec). If in Hz mode, invert."""
        if self.inverter_freq_mode.get() and val > 0:
            return 1.0 / val
        return val

    def _add_inverter(self, C1=None, C2=None, C3=None, C4=None, crossed=None, name=None):
        """Add a new inverter to the composite model.

        When called without arguments (e.g. from the Add Inverter button),
        auto-configures as a crossed inverter with progressive C1 and
        C2/C3/C4 derived from f1.
        """
        idx = len(self.composite_inverter_vars)

        # Smart defaults for new inverters added via button
        if C1 is None:
            if idx == 0:
                # First inverter: use single preset as f1
                C1 = NNN_SINGLE_PRESET['C1']
                C2 = NNN_SINGLE_PRESET['C2']
                C3 = NNN_SINGLE_PRESET['C3']
                C4 = NNN_SINGLE_PRESET['C4']
                crossed = False
            else:
                # Progressive C1: each new crossed inverter activates later
                # Use additive progression (~3-5s increments), not exponential
                prev_C1 = self.composite_inverter_vars[-1]['C1'].get()
                step = max(3.0, prev_C1 * 0.5)
                C1 = round(prev_C1 + step, 1)
                C2, C3, C4 = C1 * 1.33, C1 * 0.9, C1 * 2.66  # placeholders
                crossed = True
        if name is None:
            name = f"f{idx + 1}"
        if crossed is None:
            crossed = idx > 0

        new_inv = {
            'C1': VarD(C1),
            'C2': VarD(C2),
            'C3': VarD(C3),
            'C4': VarD(C4),
            'crossed': tk.BooleanVar(value=crossed),
            'name': tk.StringVar(value=name),
        }
        self.composite_inverter_vars.append(new_inv)
        # Auto-recalculate the new crossed inverter from f1
        if crossed and idx > 0:
            self._recalculate_composite_from_f1()
        else:
            self._rebuild_composite_gui()
    def _get_main_agent_specific_params(self, headless=False) -> Dict[str, Any]:
        agent_type = self.agent_type_var.get() if not headless else self.headless_agent_type_var.get()
        params = {}
        if agent_type == "stochastic":
            params['update_interval_sec'] = self._tk_vars["stoch_update_interval_sec"].get()
        elif agent_type == "inverter":
            params['turn_decision_interval_sec'] = self._tk_vars["turn_decision_interval_sec"].get()
            params['C1'] = self._c_display_to_period(self._tk_vars["inverter_C1"].get())
            params['C2'] = self._c_display_to_period(self._tk_vars["inverter_C2"].get())
            params['C3'] = self._c_display_to_period(self._tk_vars["inverter_C3"].get())
            params['C4'] = self._c_display_to_period(self._tk_vars["inverter_C4"].get())

        elif agent_type == "composite":
            params['turn_decision_interval_sec'] = self._tk_vars["turn_decision_interval_sec"].get()
            inverters_for_agent = []
            for inv_vars in self.composite_inverter_vars:
                inv = {
                    'C1': self._c_display_to_period(inv_vars['C1'].get()),
                    'C2': self._c_display_to_period(inv_vars['C2'].get()),
                    'C3': self._c_display_to_period(inv_vars['C3'].get()),
                    'C4': self._c_display_to_period(inv_vars['C4'].get()),
                    'crossed': inv_vars['crossed'].get(),
                    'name': inv_vars['name'].get(),
                }
                inverters_for_agent.append(inv)
            params['inverters'] = inverters_for_agent
        return params

    def _build_composite_config_tab(self, parent_tab):
        parent_tab.grid_rowconfigure(1, weight=1)
        parent_tab.grid_columnconfigure(0, weight=1)

        controls_frame = ttk.Frame(parent_tab)
        controls_frame.grid(row=0, column=0, columnspan=2, sticky='ew', padx=5, pady=5)
        ttk.Button(controls_frame, text="Add Inverter", command=lambda: self._add_inverter()).pack(side='left', padx=5)
        ttk.Button(controls_frame, text="Load NNN Preset", command=self._load_nnn_composite_preset).pack(side='left', padx=5)
        ttk.Button(controls_frame, text="Reset to Defaults", command=self._load_nnn_composite_preset).pack(side='left', padx=5)

        self.composite_config_canvas = tk.Canvas(parent_tab, borderwidth=0)
        scrollbar = ttk.Scrollbar(parent_tab, orient="vertical", command=self.composite_config_canvas.yview)
        self.composite_config_canvas.configure(yscrollcommand=scrollbar.set)

        self.composite_config_canvas.grid(row=1, column=0, sticky='nsew')
        scrollbar.grid(row=1, column=1, sticky='ns')

        self.composite_scrollable_frame = ttk.Frame(self.composite_config_canvas)
        self.composite_scrollable_frame_id = self.composite_config_canvas.create_window((0, 0), window=self.composite_scrollable_frame, anchor="nw")

        def _configure_composite_frame(event):
            req_height = self.composite_scrollable_frame.winfo_reqheight()
            req_width = self.composite_scrollable_frame.winfo_reqwidth()
            self.composite_config_canvas.configure(scrollregion=(0, 0, req_width, int(req_height * 1.2)))

        def _configure_composite_canvas(event):
            self.composite_config_canvas.itemconfigure(
                self.composite_scrollable_frame_id, width=event.width)

        self.composite_scrollable_frame.bind("<Configure>", _configure_composite_frame)
        self.composite_config_canvas.bind("<Configure>", _configure_composite_canvas)

        def _on_composite_mousewheel(event):
            self.composite_config_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")
            return "break"
        self._on_composite_mousewheel = _on_composite_mousewheel

        # Load NNN 3x1x2 preset by default
        self._load_nnn_composite_preset()

    def _remove_inverter(self, vars_to_remove):
        if len(self.composite_inverter_vars) > 1: self.composite_inverter_vars.remove(vars_to_remove); self._rebuild_composite_gui()
        else: messagebox.showwarning("Cannot Remove", "The composite agent must have at least one inverter.")

    def _load_nnn_composite_preset(self):
        """Load the NNN 3x1x2 preset configuration from inverter.py."""
        self.composite_inverter_vars.clear()
        for p in NNN_COMPOSITE_PRESET:
            self._add_inverter(
                C1=p['C1'], C2=p['C2'], C3=p['C3'], C4=p['C4'],
                crossed=p['crossed'], name=p['name']
            )

    def _auto_recalc_composite(self):
        """Auto-recalculate crossed inverters when f1 values change (trace callback)."""
        if self._recalc_in_progress:
            return
        self._recalc_in_progress = True
        try:
            self._recalculate_composite_from_f1(rebuild_gui=False)
        finally:
            self._recalc_in_progress = False

    def _recalculate_composite_from_f1(self, rebuild_gui=True):
        """Recalculate crossed inverters' C2/C3/C4 based on f1's current values."""
        if not self.composite_inverter_vars:
            return
        # Build a preset list from current GUI values
        preset = []
        hz = self.inverter_freq_mode.get()
        for inv_vars in self.composite_inverter_vars:
            C1 = inv_vars['C1'].get()
            C2 = inv_vars['C2'].get()
            C3 = inv_vars['C3'].get()
            C4 = inv_vars['C4'].get()
            # Convert from Hz to period if in freq mode
            if hz:
                C1 = 1.0 / C1 if C1 > 0 else C1
                C2 = 1.0 / C2 if C2 > 0 else C2
                C3 = 1.0 / C3 if C3 > 0 else C3
                C4 = 1.0 / C4 if C4 > 0 else C4
            preset.append({
                'C1': C1, 'C2': C2, 'C3': C3, 'C4': C4,
                'crossed': inv_vars['crossed'].get(),
                'name': inv_vars['name'].get(),
            })

        # Recalculate
        recalculate_composite(preset)

        # Write back to GUI vars (convert to Hz if needed)
        for inv_vars, p in zip(self.composite_inverter_vars, preset):
            if inv_vars['crossed'].get():
                if hz:
                    inv_vars['C2'].set(round(1.0 / p['C2'], 4) if p['C2'] > 0 else 0)
                    inv_vars['C3'].set(round(1.0 / p['C3'], 4) if p['C3'] > 0 else 0)
                    inv_vars['C4'].set(round(1.0 / p['C4'], 4) if p['C4'] > 0 else 0)
                else:
                    inv_vars['C2'].set(p['C2'])
                    inv_vars['C3'].set(p['C3'])
                    inv_vars['C4'].set(p['C4'])

        if rebuild_gui:
            self._rebuild_composite_gui()
            self._add_log_message("Composite: recalculated crossed inverters from f1")

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
        self.excursion_dist_var = tk.StringVar(value="Excursion Dist (px): N/A")
        ttk.Label(stat, textvariable=self.excursion_dist_var).grid(row=6, column=0, sticky='w', pady=2)
        
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
            
            # Persistent line artists for incremental updates
            self._line_eaten, = self.ax_counts.plot([], [], label="Total Eaten", color="green")
            self._line_decisions, = self.ax_counts.plot([], [], label="Decisions", color="blue", linestyle="--")
            self._line_available, = self.ax_counts.plot([], [], label="Food Avail", color="gray", alpha=0.5)
            self.ax_counts.set_ylabel("Count")
            self.ax_counts.set_title("Cumulative Metrics", fontsize=10)
            self.ax_counts.legend(loc="upper left", fontsize="small")
            self.ax_counts.grid(True, alpha=0.3)

            self._line_rate, = self.ax_rate.plot([], [], label="Consumption Rate (F/s)", color="red", linestyle="-.")
            self._ax_exc = self.ax_rate.twinx()
            self._line_excursion, = self._ax_exc.plot([], [], label="Excursion Dist (px)", color="orange", alpha=0.7)
            self._ax_exc.set_ylabel("Excursion Dist (px)", color="orange")
            self._ax_exc.tick_params(axis='y', labelcolor='orange')
            self.ax_rate.set_ylabel("Rate (F/s)")
            self.ax_rate.set_xlabel("Time (s)")
            self.ax_rate.set_title("Performance Rate & Excursion Distance", fontsize=10)
            lines1, labels1 = self.ax_rate.get_legend_handles_labels()
            lines2, labels2 = self._ax_exc.get_legend_handles_labels()
            self.ax_rate.legend(lines1 + lines2, labels1 + labels2, loc="upper left", fontsize="small")
            self.ax_rate.grid(True, alpha=0.3)

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
            if agent_type in ['inverter', 'composite']: 
                self.agent_input_freq_var.set(f"Input Freq (Hz): {stats.get('input_food_frequency', 0.0):.2f}")
            else: 
                self.agent_input_freq_var.set("Input Freq (Hz): N/A")
            
            if self.simulation_start_time_visual:
                elapsed = time.time() - self.simulation_start_time_visual
                self.efficiency_var.set(f"Efficiency (F/s): {stats.get('food_eaten_run', 0) / elapsed if elapsed > 0.01 else 0.0:.2f}")

            # Excursion distance from latest history
            history = stats.get('history', {})
            exc_list = history.get('excursion_dist', [])
            if exc_list and exc_list[-1] > 0:
                self.excursion_dist_var.set(f"Excursion Dist (px): {exc_list[-1]:.1f}")
            else:
                self.excursion_dist_var.set("Excursion Dist (px): N/A")

            # Graph Update (incremental — no clear/replot)
            if HAS_MATPLOTLIB and self.fig and 'history' in stats:
                history = stats['history']

                # Snapshot copies to avoid threading race conditions
                raw_times = history.get('time', [])
                raw_eaten = history.get('food_eaten', [])
                raw_available = history.get('food_available', [])
                raw_decisions = history.get('decisions', [])
                raw_excursion = history.get('excursion_dist', [])

                min_len = min(len(raw_times), len(raw_eaten), len(raw_available), len(raw_decisions), len(raw_excursion))

                # deques don't support slicing — convert to list first, then truncate
                times = list(raw_times)[:min_len]
                eaten = list(raw_eaten)[:min_len]
                available = list(raw_available)[:min_len]
                decisions = list(raw_decisions)[:min_len]
                excursion = list(raw_excursion)[:min_len]

                if len(times) > 1:
                    window = 20
                    rates = []
                    for i in range(len(times)):
                        if i < window:
                            rates.append(0.0)
                        else:
                            dt = times[i] - times[i-window]
                            d_food = eaten[i] - eaten[i-window]
                            rates.append(d_food / dt if dt > 0 else 0.0)

                    # Update persistent line data instead of clear+replot
                    self._line_eaten.set_data(times, eaten)
                    self._line_decisions.set_data(times, decisions)
                    self._line_available.set_data(times, available)
                    self.ax_counts.relim()
                    self.ax_counts.autoscale_view()

                    self._line_rate.set_data(times, rates)
                    self._line_excursion.set_data(times, excursion)
                    self.ax_rate.relim()
                    self.ax_rate.autoscale_view()
                    self._ax_exc.relim()
                    self._ax_exc.autoscale_view()

                    self.canvas.draw_idle()

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
            try: self.active_log_file = open(fname, 'w', buffering=65536); self._add_log_message(f"Logging to {fname}"); self._write_log_header()
            except IOError as e: self._add_log_message(f"Error opening log file: {e}"); self.active_log_file = None
        self.running_simulation = True; self.simulation_start_time_visual = time.time(); self.start_button.config(state="disabled"); self.stop_button.config(state="normal"); self.sim_status_var.set("Simulation: Starting...")
        self.sim_thread = threading.Thread(target=self._simulation_loop_thread, daemon=True); self.sim_thread.start()
    def stop_simulation(self):
        if not self.running_simulation: return
        self.running_simulation = False
        if self.sim_thread and self.sim_thread.is_alive(): self.sim_thread.join(timeout=1.0)
        self.sim_thread = None; self.simulation_start_time_visual = None
        if self.active_log_file and not self.active_log_file.closed:
            stats = get_simulation_stats()
            history = stats.get('history', {})
            exc_list = history.get('excursion_dist', [])
            elapsed = stats.get('history', {}).get('time', [0])[-1] if history.get('time') else 0
            f = self.active_log_file
            f.write(f"# --- FINAL_STATS ---\n")
            f.write(f"# food_eaten={stats.get('food_eaten_run', 0)}\n")
            f.write(f"# food_remaining={stats.get('total_food', 0)}\n")
            f.write(f"# sim_time={elapsed:.3f}\n")
            f.write(f"# efficiency={stats.get('food_eaten_run', 0) / elapsed:.3f}\n" if elapsed > 0 else "# efficiency=0\n")
            f.write(f"# excursion_dist={exc_list[-1]:.1f}\n" if exc_list and exc_list[-1] > 0 else "# excursion_dist=N/A\n")
            f.write(f"# input_freq={stats.get('input_food_frequency', 0.0):.2f}\n")
            self._add_log_message(f"Closing log file."); f.close(); self.active_log_file = None
        self.start_button.config(state="normal"); self.stop_button.config(state="disabled"); self.sim_status_var.set("Simulation: Idle")

    def _simulation_loop_thread(self):
        pygame.init()
        try:
            info = pygame.display.Info()
            cfg.WINDOW_W = min(cfg.WINDOW_W, info.current_w - 50)
            cfg.WINDOW_H = min(cfg.WINDOW_H, info.current_h - 80)
            self.pygame_surface = pygame.display.set_mode((cfg.WINDOW_W, cfg.WINDOW_H))
            # Re-apply spawn hint for actual window height
            hint = cfg.FOOD_SPAWN_HINTS.get(self.food_sel.get())
            if hint:
                self._tk_vars["spawn_y"].set(hint[1] * cfg.WINDOW_H)
            else:
                self._tk_vars["spawn_y"].set(cfg.WINDOW_H / 2)
            pygame.display.set_caption("Agent Sim")
            if not self._apply_settings():
                if self.active_log_file: self.active_log_file.close(); self.active_log_file = None
                self.running_simulation = False; pygame.quit()
                self.root.after(0, lambda: self.start_button.config(state="normal"))
                self.root.after(0, lambda: self.stop_button.config(state="disabled"))
                return
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
                    elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                        teleport_agent(0, event.pos[0], event.pos[1])
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
        if agent_type == 'composite':
            # Write as JSON block so import can round-trip it
            json_obj = {
                'turn_decision_interval_sec': agent_specific_params.get('turn_decision_interval_sec', 0),
                'inverters': agent_specific_params.get('inverters', []),
            }
            f.write(f"# agent_params: \n")
            for json_line in json.dumps(json_obj, indent=4).splitlines():
                f.write(f"# {json_line}\n")
        elif agent_specific_params:
            params.update(agent_specific_params)
        # Write params, formatting C1-C4 with both units
        c_keys = {'C1', 'C2', 'C3', 'C4'}
        for key, value in params.items():
            if agent_type == 'composite' and key in ('agent_params', 'inverters', 'turn_decision_interval_sec'): continue
            if key in c_keys and isinstance(value, (int, float)) and value > 0:
                freq = 1.0 / value if value > 0 else 0
                f.write(f"# {key}: {freq:.4f} Hz, {value:.4f} sec\n")
            else:
                f.write(f"# {key}: {value}\n")
        f.write(f"# --- DATA ---\n")
        base_cols = "frame,agent_id,pos_x,pos_y,angle_rad,delta_dist,delta_theta_rad,food_freq,mode,speed"
        if agent_type == 'inverter':
            sig_cols = ",inv0_L,inv0_R"
        elif agent_type == 'composite':
            n = len(self.composite_inverter_vars)
            sig_cols = "," + ",".join(f"inv{i}_L,inv{i}_R" for i in range(n)) if n > 0 else ""
        else:
            sig_cols = ""
        f.write(f"# {base_cols}{sig_cols}\n")
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
        }
        # C1-C4 exported as "# C1: 3.3333 Hz, 0.3000 sec" — extract period (sec)
        c_period_map = {
            "C1": "inverter_C1", "C2": "inverter_C2",
            "C3": "inverter_C3", "C4": "inverter_C4",
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
                            self.composite_inverter_vars.clear()
                            # New inverters format
                            if 'inverters' in params:
                                for inv in params['inverters']:
                                    new_inv = {
                                        'C1': VarD(inv.get('C1', 2.0)),
                                        'C2': VarD(inv.get('C2', 1.0)),
                                        'C3': VarD(inv.get('C3', 4.0)),
                                        'C4': VarD(inv.get('C4', 0.5)),
                                        'crossed': tk.BooleanVar(value=inv.get('crossed', False)),
                                        'name': tk.StringVar(value=inv.get('name', '')),
                                    }
                                    self.composite_inverter_vars.append(new_inv)
                            # Legacy oscillators format (load as default)
                            elif 'oscillators' in params or 'layer_pairs' in params:
                                self._load_nnn_composite_preset()
                                return
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
            elif key in c_period_map:
                # Parse "3.3333 Hz, 0.3000 sec" → extract period (sec value)
                sec_match = re.search(r'([\d.]+)\s*sec', value_str)
                if sec_match:
                    self._tk_vars[c_period_map[key]].set(float(sec_match.group(1)))

if __name__ == "__main__":
    try: app = SimGUI(); app.root.mainloop()
    except Exception as e: print(f"An error occurred: {e}"); import traceback; traceback.print_exc(); sys.exit(1)