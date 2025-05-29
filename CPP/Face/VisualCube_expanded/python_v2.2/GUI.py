from __future__ import annotations
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import pygame
from typing import Dict, List, Any, Sequence, Optional
import time 
from statistics import mean, median 

import config as cfg
from main import ( 
    configure as main_configure, run_frame as main_run_frame,
    AGENT_CLASSES as main_agent_classes, 
    init_pygame_surface as main_init_pygame_surface,
    get_simulation_stats, FRAME_DT
)
from agent_module import (
    agent_base_movement_params, 
    _ATOMIC_AGENT_CLASSES,
    AGENT_CONFIG as agent_module_AGENT_CONFIG 
)
from math_module import set_frame_dt

class VarD(tk.DoubleVar):
    def __init__(self, v: float): super().__init__(value=round(v, 4))
class VarI(tk.IntVar):
    def __init__(self, v: int): super().__init__(value=v)

_COMPOSITE_LAYER_AGENT_TYPES = ["None"] + list(_ATOMIC_AGENT_CLASSES.keys())

class SimGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Agent Sim - Configurable Inverse Turn")
        self.root.geometry("720x920+50+50") 

        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=5, pady=5)

        self.controls_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.controls_tab, text="Controls & Sim Params")
        
        self.composite_config_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.composite_config_tab, text="Composite Config", state="disabled")

        self.headless_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.headless_tab, text="Headless Runs")
        self.status_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.status_tab, text="Status")

        self._tk_vars = {
            "base_turn_rate_dt": VarD(agent_base_movement_params[1]), 
            "fps": VarI(int(1 / FRAME_DT if FRAME_DT > 0 else 60)),
            "n_agents": VarI(1),
            "spawn_x": VarD(cfg.WINDOW_W / 2),
            "spawn_y": VarD(cfg.WINDOW_H / 2),
            "initial_orientation_deg": VarD(0.0),
            "agent_constant_speed": VarD(cfg.AGENT_CONSTANT_SPEED),
            "input_time_scale": VarI(agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"]),
            "output_time_scale": VarI(agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]),
            "inverse_turn_freq_thresh": VarI(agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]),
        }
        
        self.composite_agent_configs_gui: List[Dict[str, Any]] = []
        self.composite_config_canvas_frame: Optional[ttk.Frame] = None
        self.specific_agent_params_frame: Optional[ttk.LabelFrame] = None 
        self.main_base_turn_rate_slider_frame: Optional[ttk.Frame] = None # To enable/disable

        self.headless_agent_type_var = tk.StringVar(); self.headless_agent_type_var.set(list(main_agent_classes.keys())[0] if main_agent_classes else "")
        self.headless_expiry_timer_var = VarI(30); self.headless_num_agents_var = VarI(10); self.headless_num_instances_var = VarI(5)
        self.headless_avg_eff_var = tk.StringVar(value="Avg. Efficiency: N/A"); self.headless_median_eff_var = tk.StringVar(value="Median Efficiency: N/A")
        self.headless_status_var = tk.StringVar(value="Status: Idle")

        self._build_controls_tab(self.controls_tab)
        self._build_composite_config_tab(self.composite_config_tab) 
        self._build_headless_tab(self.headless_tab)
        self._build_status_tab(self.status_tab)

        self.running_simulation = False; self.sim_thread = None; self.pygame_surface = None
        self.simulation_start_time_visual = None; self.headless_run_active = False
        self.root.protocol("WM_DELETE_WINDOW", self.quit_application)
        self._periodic_status_update()
        self._on_agent_type_change() 

    def _add_slider_entry(self, parent, tk_var_instance, label_text, from_, to_, resolution_override=None, width=28):
        row_frame = ttk.Frame(parent); row_frame.pack(fill="x", pady=2)
        ttk.Label(row_frame, text=label_text, width=width).pack(side="left")
        resolution = resolution_override if resolution_override is not None else (0.01 if isinstance(tk_var_instance, tk.DoubleVar) else 1)
        if "Speed" in label_text or "Orientation" in label_text or "Freq" in label_text : resolution = 1.0
        if "Turn Rate" in label_text : resolution = 0.1 
        if "Param" in label_text and not isinstance(tk_var_instance, tk.IntVar): resolution = 0.01


        scale = ttk.Scale(row_frame, variable=tk_var_instance, from_=from_, to=to_, orient="horizontal")
        try: scale.configure(resolution=resolution) 
        except tk.TclError: pass 
        scale.pack(side="left", fill="x", expand=True, padx=5)
        entry_width = 7 if isinstance(tk_var_instance, tk.DoubleVar) else 5
        ttk.Entry(row_frame, textvariable=tk_var_instance, width=entry_width).pack(side="left")
        return row_frame

    def _build_controls_tab(self, parent_tab):
        main_frame = ttk.Frame(parent_tab, padding=10); main_frame.pack(fill="both", expand=True)

        time_scale_frame = ttk.LabelFrame(main_frame, text="Agent/Composite Top-Level Time Scales")
        time_scale_frame.pack(fill="x", pady=5)
        self._add_slider_entry(time_scale_frame, self._tk_vars["input_time_scale"], "Input Avg Time (frames)\n(Not for Composite Agent)", 5, 100, width=28) # Clarified label
        self._add_slider_entry(time_scale_frame, self._tk_vars["output_time_scale"], "Output/Aggregation Time (frames)\n(For Composite Agent)", 5, 100, width=28) # Clarified label

        agent_motion_timing_frame = ttk.LabelFrame(main_frame, text="Agent Motion & Simulation Timing")
        agent_motion_timing_frame.pack(fill="x", pady=5)
        # Store the frame for the main base turn rate slider to enable/disable it
        self.main_base_turn_rate_slider_frame = self._add_slider_entry(agent_motion_timing_frame, 
                                                                      self._tk_vars["base_turn_rate_dt"], 
                                                                      "Base Turn Rate (°/frame)\n(Max for Composite)", 
                                                                      0.0, 45.0, width=28) # Clarified label

        self._add_slider_entry(agent_motion_timing_frame, self._tk_vars["agent_constant_speed"], "Agent Constant Speed (px/s)", 10, 300)
        self._add_slider_entry(agent_motion_timing_frame, self._tk_vars["fps"], "Target FPS (sets ∆t)", 10, 240)

        world_setup_frame = ttk.LabelFrame(main_frame, text="World & Agent Setup")
        world_setup_frame.pack(fill="x", pady=5)
        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Agent Type (Visual Sim)", width=28).pack(side="left")
        self.agent_type_var = tk.StringVar() 
        self.agent_sel = ttk.Combobox(f, values=list(main_agent_classes.keys()), state="readonly", width=18, textvariable=self.agent_type_var)
        if list(main_agent_classes.keys()): self.agent_type_var.set(list(main_agent_classes.keys())[0])
        self.agent_sel.pack(side="left", fill="x", expand=True, padx=5)
        self.agent_type_var.trace_add("write", self._on_agent_type_change) 
        
        self.specific_agent_params_frame = ttk.LabelFrame(main_frame, text="Specific Agent Parameters")
        
        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2) 
        ttk.Label(f, text="# Agents", width=28).pack(side="left")
        ttk.Spinbox(f, from_=1, to=150, textvariable=self._tk_vars["n_agents"], width=10).pack(side="left", fill="x", expand=True, padx=5)
        self._add_slider_entry(world_setup_frame, self._tk_vars["spawn_x"], "Spawn X", 0, cfg.WINDOW_W)
        self._add_slider_entry(world_setup_frame, self._tk_vars["spawn_y"], "Spawn Y", 0, cfg.WINDOW_H)
        self._add_slider_entry(world_setup_frame, self._tk_vars["initial_orientation_deg"], "Initial Orientation (°)", 0, 359)
        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Food Preset", width=28).pack(side="left")
        self.food_sel = ttk.Combobox(f, values=list(cfg.FOOD_PRESETS.keys()), state="readonly", width=18)
        self.food_sel.current(0); self.food_sel.pack(side="left", fill="x", expand=True, padx=5)

        buttons_frame = ttk.Frame(main_frame); buttons_frame.pack(fill="x", pady=10, side="bottom")
        self.start_button = ttk.Button(buttons_frame, text="Start Visual Sim", command=self.start_simulation)
        self.start_button.pack(side="left", expand=True, fill="x", padx=5)
        self.stop_button = ttk.Button(buttons_frame, text="Stop Visual Sim", command=self.stop_simulation, state="disabled")
        self.stop_button.pack(side="left", expand=True, fill="x", padx=5)
        ttk.Button(buttons_frame, text="Quit App", command=self.quit_application).pack(side="left", expand=True, fill="x", padx=5)

    def _build_composite_config_tab(self, parent_tab): 
        control_frame = ttk.Frame(parent_tab, padding=(10,5,10,0)); control_frame.pack(fill="x")
        ttk.Button(control_frame, text="Add Agent Config Block", command=self._add_composite_agent_config_gui_block).pack(side="left", padx=5)
        canvas = tk.Canvas(parent_tab, borderwidth=0)
        scrollbar = ttk.Scrollbar(parent_tab, orient="vertical", command=canvas.yview)
        self.composite_config_canvas_frame = ttk.Frame(canvas, padding=5)
        self.composite_config_canvas_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=self.composite_config_canvas_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True); scrollbar.pack(side="right", fill="y")
        self._add_composite_agent_config_gui_block()

    def _add_composite_agent_config_gui_block(self, type_val="stochastic", instances_val=1, 
                                            input_ts_val=30, output_ts_val=30, 
                                            param_val=agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"],
                                            layer_turn_rate_val=agent_base_movement_params[1] 
                                            ):
        if not self.composite_config_canvas_frame: return
        block_id = len(self.composite_agent_configs_gui)
        block_frame = ttk.LabelFrame(self.composite_config_canvas_frame, text=f"Agent Config Block {block_id + 1}")
        block_frame.pack(fill="x", expand=True, pady=7, padx=5)
        
        current_param_val_for_rebuild = int(param_val) if type_val == "inverse_turn" else float(param_val)

        tk_vars = { "type": tk.StringVar(value=type_val), "instances": tk.IntVar(value=instances_val),
                    "input_ts": tk.IntVar(value=input_ts_val), "output_ts": tk.IntVar(value=output_ts_val),
                    "layer_base_turn_rate": VarD(layer_turn_rate_val) 
                  }
        widgets = {"frame": block_frame}
        
        f = ttk.Frame(block_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Agent Type:", width=15).pack(side="left")
        type_combo = ttk.Combobox(f, textvariable=tk_vars["type"], values=_COMPOSITE_LAYER_AGENT_TYPES, state="readonly", width=15)
        type_combo.pack(side="left", padx=5); widgets["type_combo"] = type_combo
        type_combo.bind("<<ComboboxSelected>>", lambda e, b_id=block_id: self._update_composite_block_param_type_and_toggle(b_id))
        
        f = ttk.Frame(block_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="# Instances:", width=15).pack(side="left")
        instances_spinbox = ttk.Spinbox(f, from_=1, to=50, textvariable=tk_vars["instances"], width=5)
        instances_spinbox.pack(side="left", padx=5); widgets["instances_spinbox"] = instances_spinbox
        
        slider_width = 16 
        widgets["input_ts_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["input_ts"], "Input Time Scale", 5, 100, width=slider_width)
        widgets["output_ts_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["output_ts"], "Output Time Scale", 5, 100, width=slider_width)
        
        widgets["layer_turn_rate_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["layer_base_turn_rate"], 
                                                                        "Layer Turn Rate (°/fr)", 0.0, 45.0, 
                                                                        width=slider_width)

        param_frame = ttk.LabelFrame(block_frame, text="Type-Specific Parameter") 
        param_frame.pack(fill="x", expand=True, pady=3)
        widgets["param_val_slider_container"] = param_frame
        
        self._rebuild_param_slider(block_id, tk_vars, widgets, current_param_val_for_rebuild)
        
        remove_button = ttk.Button(block_frame, text="Remove Block", command=lambda b_id=block_id: self._remove_composite_agent_config_gui_block(b_id))
        remove_button.pack(pady=5); widgets["remove_button"] = remove_button
        
        self.composite_agent_configs_gui.append({"id": block_id, "tk_vars": tk_vars, "widgets": widgets})
        self._toggle_composite_block_widgets(block_id)

    def _rebuild_param_slider(self, block_id: int, tk_vars_dict: Dict[str, Any], widgets_dict: Dict[str, Any], current_value: float | int):
        container = widgets_dict.get("param_val_slider_container")
        if not container: return

        for widget in container.winfo_children(): widget.destroy() 
        widgets_dict["param_val_slider_actual_frame"] = None 
        widgets_dict["no_param_info_label"] = None 

        selected_type = tk_vars_dict["type"].get()
        slider_width = 16 

        if selected_type == "inverse_turn":
            if not isinstance(tk_vars_dict.get("param_val"), tk.IntVar):
                tk_vars_dict["param_val"] = tk.IntVar()
            tk_vars_dict["param_val"].set(int(current_value))
            
            label_text = "Freq. Thresh (frames)"
            from_ = 1
            to_ = 100
            resolution = 1
            
            slider_row = self._add_slider_entry(container, tk_vars_dict["param_val"], label_text, from_, to_, resolution_override=resolution, width=slider_width)
            widgets_dict["param_val_slider_actual_frame"] = slider_row
        
        else: 
            if "param_val" not in tk_vars_dict or not isinstance(tk_vars_dict.get("param_val"), tk.DoubleVar):
                 tk_vars_dict["param_val"] = tk.DoubleVar(value=0.0) 
            
            no_param_label = ttk.Label(container, text="No type-specific parameters for this agent type.")
            no_param_label.pack(pady=5, padx=5, anchor="center")
            widgets_dict["no_param_info_label"] = no_param_label


    def _update_composite_block_param_type_and_toggle(self, block_id: int): 
        block_data = next((b for b in self.composite_agent_configs_gui if b["id"] == block_id), None)
        if not block_data: return

        selected_type = block_data["tk_vars"]["type"].get()
        
        try:
            current_param_value = block_data["tk_vars"]["param_val"].get()
        except (tk.TclError, AttributeError, KeyError): 
            current_param_value = agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"] if selected_type == "inverse_turn" else 0.1 

        if selected_type == "inverse_turn":
            current_param_value = int(float(current_param_value)) 
        else: 
            current_param_value = float(current_param_value)

        self._rebuild_param_slider(block_id, block_data["tk_vars"], block_data["widgets"], current_param_value)
        self._toggle_composite_block_widgets(block_id)


    def _remove_composite_agent_config_gui_block(self, block_id_to_remove: int): 
        block_to_remove = None
        for i, block_data in enumerate(self.composite_agent_configs_gui):
            if block_data["id"] == block_id_to_remove: block_to_remove = block_data; self.composite_agent_configs_gui.pop(i); break
        if block_to_remove and block_to_remove["widgets"]["frame"].winfo_exists(): block_to_remove["widgets"]["frame"].destroy()
        for i, block_data in enumerate(self.composite_agent_configs_gui): 
            if block_data["widgets"]["frame"].winfo_exists(): 
                 try: block_data["widgets"]["frame"].config(text=f"Agent Config Block {i + 1}")
                 except tk.TclError: pass

    def _toggle_composite_block_widgets(self, block_id: int): 
        block_data = next((b for b in self.composite_agent_configs_gui if b["id"] == block_id), None)
        if not block_data: return

        selected_agent_type_in_block = block_data["tk_vars"]["type"].get()
        is_block_logically_active = selected_agent_type_in_block != "None"
        
        widgets_to_toggle_based_on_block_activity = [
            block_data["widgets"]["instances_spinbox"],
        ]
        
        for key in ["input_ts_slider_frame", "output_ts_slider_frame", "layer_turn_rate_slider_frame"]: 
            slider_main_frame = block_data["widgets"].get(key)
            if slider_main_frame and hasattr(slider_main_frame, 'winfo_children'):
                for widget_in_row in slider_main_frame.winfo_children():
                    widgets_to_toggle_based_on_block_activity.append(widget_in_row)

        for widget in widgets_to_toggle_based_on_block_activity:
            if widget and hasattr(widget, 'winfo_exists') and widget.winfo_exists():
                try: widget.config(state=(tk.NORMAL if is_block_logically_active else tk.DISABLED))
                except tk.TclError: pass
        
        param_actual_slider_frame = block_data["widgets"].get("param_val_slider_actual_frame")
        no_param_info_label = block_data["widgets"].get("no_param_info_label")

        is_param_area_relevant = is_block_logically_active and (param_actual_slider_frame is not None)

        if param_actual_slider_frame and hasattr(param_actual_slider_frame, 'winfo_children'):
            for child_widget in param_actual_slider_frame.winfo_children():
                if hasattr(child_widget, 'winfo_exists') and child_widget.winfo_exists():
                    try: child_widget.config(state=(tk.NORMAL if is_param_area_relevant else tk.DISABLED))
                    except tk.TclError: pass
        
        if no_param_info_label and hasattr(no_param_info_label, 'winfo_exists') and no_param_info_label.winfo_exists():
             try: no_param_info_label.config(state=(tk.NORMAL if is_block_logically_active and not param_actual_slider_frame else tk.DISABLED))
             except tk.TclError: pass
    
    def _on_agent_type_change(self, *args): 
        selected_type = self.agent_type_var.get()
        
        # Enable/disable Composite Config tab
        composite_tab_index = -1
        try:
            for i in range(self.notebook.index("end")): 
                if self.notebook.tab(i, "text") == "Composite Config":
                    composite_tab_index = i; break
        except tk.TclError: return 
        
        if composite_tab_index != -1:
            self.notebook.tab(composite_tab_index, state="normal" if selected_type == "composite" else "disabled")

        # Show/hide specific agent parameters for main agent
        if self.specific_agent_params_frame:
            for widget in self.specific_agent_params_frame.winfo_children(): widget.destroy()

            if selected_type == "inverse_turn":
                self._add_slider_entry(self.specific_agent_params_frame, 
                                       self._tk_vars["inverse_turn_freq_thresh"],
                                       "Frequency Threshold (frames)", 1, 100, resolution_override=1)
                self.specific_agent_params_frame.pack(fill="x", pady=(5,0), padx=0, after=self.agent_sel.master.master)
            else:
                self.specific_agent_params_frame.pack_forget()

    def _build_headless_tab(self, parent_tab): 
        main_frame = ttk.Frame(parent_tab, padding=10); main_frame.pack(fill="both", expand=True)
        config_frame = ttk.LabelFrame(main_frame, text="Headless Instance Configuration"); config_frame.pack(fill="x", pady=5, padx=5)
        f = ttk.Frame(config_frame); f.pack(fill="x", pady=3); ttk.Label(f, text="Agent Type:", width=20).pack(side="left")
        headless_agent_sel = ttk.Combobox(f, values=list(main_agent_classes.keys()), state="readonly", width=20, textvariable=self.headless_agent_type_var)
        headless_agent_sel.pack(side="left", fill="x", expand=True, padx=5)
        f = ttk.Frame(config_frame); f.pack(fill="x", pady=3); ttk.Label(f, text="Expire Timer (s):", width=20).pack(side="left")
        ttk.Spinbox(f, from_=5, to=3600, textvariable=self.headless_expiry_timer_var, width=10).pack(side="left", padx=5)
        f = ttk.Frame(config_frame); f.pack(fill="x", pady=3); ttk.Label(f, text="# Agents/Instance:", width=20).pack(side="left")
        ttk.Spinbox(f, from_=1, to=200, textvariable=self.headless_num_agents_var, width=10).pack(side="left", padx=5)
        f = ttk.Frame(config_frame); f.pack(fill="x", pady=3); ttk.Label(f, text="# Instances:", width=20).pack(side="left")
        ttk.Spinbox(f, from_=1, to=1000, textvariable=self.headless_num_instances_var, width=10).pack(side="left", padx=5)
        self.start_headless_button = ttk.Button(main_frame, text="Start Headless Runs", command=self.start_headless_runs)
        self.start_headless_button.pack(fill="x", pady=10)
        results_frame = ttk.LabelFrame(main_frame, text="Results"); results_frame.pack(fill="x", pady=5, padx=5)
        ttk.Label(results_frame, textvariable=self.headless_status_var).pack(anchor="w", pady=2)
        ttk.Label(results_frame, textvariable=self.headless_avg_eff_var).pack(anchor="w", pady=2)
        ttk.Label(results_frame, textvariable=self.headless_median_eff_var).pack(anchor="w", pady=2)

    def _build_status_tab(self, parent_tab): 
        status_frame = ttk.LabelFrame(parent_tab, text="Live Simulation Status", padding=10); status_frame.pack(fill="both", expand=True)
        self.sim_status_var = tk.StringVar(value="Simulation: Idle"); ttk.Label(status_frame, textvariable=self.sim_status_var, font=("Arial", 12)).pack(anchor="w", pady=5)
        self.total_food_var = tk.StringVar(value="Total Food Items: N/A"); ttk.Label(status_frame, textvariable=self.total_food_var).pack(anchor="w", pady=2)
        self.agents_active_var = tk.StringVar(value="Active Agents: N/A"); ttk.Label(status_frame, textvariable=self.agents_active_var).pack(anchor="w", pady=2)
        self.food_eaten_var = tk.StringVar(value="Food Eaten This Run: N/A"); ttk.Label(status_frame, textvariable=self.food_eaten_var).pack(anchor="w", pady=2)
        self.efficiency_var = tk.StringVar(value="Efficiency (F/s): N/A"); ttk.Label(status_frame, textvariable=self.efficiency_var).pack(anchor="w", pady=2)
        ttk.Separator(status_frame, orient='horizontal').pack(fill='x', pady=10)
        log_frame = ttk.LabelFrame(status_frame, text="Event Log"); log_frame.pack(fill="both", expand=True, pady=5)
        self.log_text = tk.Text(log_frame, height=8, state="disabled", wrap="word"); log_scroll = ttk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.config(yscrollcommand=log_scroll.set); log_scroll.pack(side="right", fill="y"); self.log_text.pack(side="left", fill="both", expand=True)
        self._add_log_message("GUI Initialized.")

    def _add_log_message(self, message: str): 
        if hasattr(self, 'log_text') and self.log_text.winfo_exists():
            self.log_text.config(state="normal"); self.log_text.insert(tk.END, f"{time.strftime('%H:%M:%S')} - {message}\n")
            self.log_text.see(tk.END); self.log_text.config(state="disabled")

    def _apply_settings(self, for_headless=False) -> bool:
        self._add_log_message("Applying settings...")
        fps_val = self._tk_vars["fps"].get()
        current_dt = 1.0 / fps_val if fps_val > 0 else (1.0 / 60.0)
        set_frame_dt(current_dt)
        cfg.AGENT_CONSTANT_SPEED = self._tk_vars["agent_constant_speed"].get()
        
        # This is the base_turn_rate_deg_dt for the SimAgent's kinematics
        # For non-composite, it's also the controller's base turn rate.
        # For composite, it's the composite's *maximum* turn rate.
        sim_agent_base_turn_rate_val = self._tk_vars["base_turn_rate_dt"].get() 
        
        # The composite agent uses its own output_time_scale. Input_time_scale is not used by composite.
        # For non-composite agents, these are their direct time scales.
        controller_input_ts_val = self._tk_vars["input_time_scale"].get()
        controller_output_ts_val = self._tk_vars["output_time_scale"].get()

        main_configure_kwargs = {
            "food_preset": self.food_sel.get(),
            "n_agents": int(self._tk_vars["n_agents"].get()) if not for_headless else self.headless_num_agents_var.get(),
            "spawn_point": (self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()),
            "initial_orientation_deg": self._tk_vars["initial_orientation_deg"].get(),
            "base_turn_deg_dt": sim_agent_base_turn_rate_val, 
            "input_time_scale": controller_input_ts_val, 
            "output_time_scale": controller_output_ts_val,
            "layer_configs_for_composite": None,
            "agent_constant_speed": cfg.AGENT_CONSTANT_SPEED,
            "agent_params_for_main_agent": None 
        }
        current_agent_type = self.agent_type_var.get() if not for_headless else self.headless_agent_type_var.get()
        main_configure_kwargs["agent_type"] = current_agent_type

        if current_agent_type == "inverse_turn": # Applies if inverse_turn is the main agent
            main_configure_kwargs["agent_params_for_main_agent"] = {
                "frequency_threshold": self._tk_vars["inverse_turn_freq_thresh"].get() 
            }
        # For headless inverse_turn, this specific param is handled in _headless_run_worker_thread
        
        if current_agent_type == "composite": 
            gui_layer_configs_data = self._get_current_layer_configs_for_composite(current_agent_type)
            if not gui_layer_configs_data and not for_headless:
                 messagebox.showwarning("Composite Config Warning", "Composite agent: no active agent blocks. Will use default.")
            main_configure_kwargs["layer_configs_for_composite"] = gui_layer_configs_data
        
        main_configure(**main_configure_kwargs)
        self._add_log_message(f"World configured. Agent: {current_agent_type}, Top Speed: {cfg.AGENT_CONSTANT_SPEED:.1f}px/s, SimAgent Turn Cap: {sim_agent_base_turn_rate_val:.1f}°/fr")
        return True

    def start_simulation(self):
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run active."); return
        if self.running_simulation: self.stop_simulation() 
        if not self._apply_settings(): return
        self.running_simulation = True; self.simulation_start_time_visual = time.time()
        self.start_button.config(state="disabled"); self.stop_button.config(state="normal")
        self.sim_status_var.set("Simulation: Starting...")
        self._add_log_message("Starting visual simulation thread...")
        self.sim_thread = threading.Thread(target=self._simulation_loop_thread, daemon=True); self.sim_thread.start()

    def stop_simulation(self):
        if not self.running_simulation: return
        self._add_log_message("Stopping visual simulation..."); self.sim_status_var.set("Simulation: Stopping...")
        self.running_simulation = False
        if self.sim_thread and self.sim_thread.is_alive():
            try: self.sim_thread.join(timeout=1.0)
            except RuntimeError: pass
        self.sim_thread = None; self.simulation_start_time_visual = None
        self.start_button.config(state="normal"); self.stop_button.config(state="disabled")
        self.sim_status_var.set("Simulation: Idle"); self._add_log_message("Visual simulation stopped.")

    def _simulation_loop_thread(self):
        self._add_log_message("Visual simulation thread active.")
        pygame.init()
        try:
            self.pygame_surface = pygame.display.set_mode((cfg.WINDOW_W, cfg.WINDOW_H))
            pygame.display.set_caption("Agent Simulation Visuals")
            self.root.after(0, lambda: self.sim_status_var.set("Simulation: Running"))
            self._add_log_message(f"Pygame window created.")
        except pygame.error as e:
            self.root.after(0, lambda: self._handle_pygame_error_gui(str(e)))
            if pygame.get_init(): pygame.quit()
            self.running_simulation = False; self.root.after(0, self._ensure_gui_stopped_state); return
        main_init_pygame_surface(self.pygame_surface); clock = pygame.time.Clock()
        while self.running_simulation:
            for event in pygame.event.get():
                if event.type == pygame.QUIT: self.running_simulation = False; break
            if not self.running_simulation: break
            main_run_frame(is_visual=True); pygame.display.flip()
            try: current_fps_target = self._tk_vars["fps"].get()
            except tk.TclError: self.running_simulation = False; break 
            if current_fps_target <=0: current_fps_target = 1 
            clock.tick(current_fps_target)
        if pygame.get_init(): pygame.quit()
        self.pygame_surface = None; self._add_log_message("Visual simulation thread finished.")
        self.root.after(0, self._ensure_gui_stopped_state)

    def start_headless_runs(self):
        if self.running_simulation: messagebox.showwarning("Busy", "Visual sim active."); return
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run in progress."); return
        
        original_n_agents = self._tk_vars["n_agents"].get() 
        original_agent_type = self.agent_type_var.get()

        self._tk_vars["n_agents"].set(self.headless_num_agents_var.get()) 
        self.agent_type_var.set(self.headless_agent_type_var.get()) # Temporarily set main agent type for FRAME_DT settings
        apply_ok = self._apply_settings(for_headless=True) # This sets FRAME_DT based on main GUI's FPS
        
        self._tk_vars["n_agents"].set(original_n_agents) 
        self.agent_type_var.set(original_agent_type) # Restore

        if not apply_ok:
             self._add_log_message("Headless pre-config (for FRAME_DT) failed."); self.headless_status_var.set("Status: Config Error")
             return

        self.headless_run_active = True; self.start_headless_button.config(state="disabled")
        self.headless_status_var.set("Status: Starting...")
        self._add_log_message("Starting headless runs...")
        
        headless_thread = threading.Thread(target=self._headless_run_worker_thread, daemon=True); headless_thread.start()

    def _headless_run_worker_thread(self):
        try:
            num_instances = self.headless_num_instances_var.get()
            expiry_seconds = self.headless_expiry_timer_var.get()
            current_frame_dt = FRAME_DT 
            if current_frame_dt <= 0: raise ValueError("FRAME_DT is zero or negative.")
            frames_per_instance = int(expiry_seconds / current_frame_dt)
            self.root.after(0, lambda: self._add_log_message(f"Headless: {num_instances} instances, {expiry_seconds}s/instance at DT={current_frame_dt:.4f}s."))
            efficiency_scores = []

            headless_agent_type_val = self.headless_agent_type_var.get()
            agent_params_for_headless_main_agent = None
            if headless_agent_type_val == "inverse_turn": # If the *overall* agent for headless is inverse_turn
                 agent_params_for_headless_main_agent = {
                    # Use the value from the main GUI's specific param section for inverse_turn
                    "frequency_threshold": self._tk_vars["inverse_turn_freq_thresh"].get() 
                }

            for i in range(num_instances):
                if not self.headless_run_active: self.root.after(0, lambda: self._add_log_message("Headless aborted.")); break
                instance_msg = f"Instance {i+1}/{num_instances}..."; self.root.after(0, lambda s=instance_msg: self.headless_status_var.set(f"Status: {s}"))
                
                layer_configs_for_this_run = self._get_current_layer_configs_for_composite(headless_agent_type_val)

                # SimAgent's base_turn_deg_dt (from main GUI) is the "max turn rate" for the agent.
                # SimAgent's input/output_time_scale (from main GUI) are for the top-level controller.
                main_configure( 
                    food_preset=self.food_sel.get(), agent_type=headless_agent_type_val,
                    n_agents=self.headless_num_agents_var.get(), 
                    spawn_point=(self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()),
                    initial_orientation_deg=self._tk_vars["initial_orientation_deg"].get(), 
                    base_turn_deg_dt=self._tk_vars["base_turn_rate_dt"].get(), 
                    input_time_scale=self._tk_vars["input_time_scale"].get(), 
                    output_time_scale=self._tk_vars["output_time_scale"].get(),
                    layer_configs_for_composite=layer_configs_for_this_run, 
                    agent_constant_speed=self._tk_vars["agent_constant_speed"].get(),
                    agent_params_for_main_agent=agent_params_for_headless_main_agent 
                )
                main_init_pygame_surface(None) 
                for frame_num in range(frames_per_instance):
                    main_run_frame(is_visual=False)
                    if frame_num % (frames_per_instance // 20 if frames_per_instance > 20 else 1) == 0 : 
                        progress_msg = f"Instance {i+1}: {frame_num*100//frames_per_instance}%"; self.root.after(0, lambda s=progress_msg: self.headless_status_var.set(f"Status: {s}"))
                stats = get_simulation_stats(); food_eaten = stats.get("food_eaten_run", 0)
                efficiency = food_eaten / expiry_seconds if expiry_seconds > 0 else 0; efficiency_scores.append(efficiency)
                self.root.after(0, lambda f=food_eaten, e=efficiency: self._add_log_message(f"Instance done. Food: {f}, Eff: {e:.3f} F/s."))
            
            if efficiency_scores:
                avg_eff, med_eff = mean(efficiency_scores), median(efficiency_scores)
                self.root.after(0, lambda: self.headless_avg_eff_var.set(f"Avg. Efficiency: {avg_eff:.3f} F/s"))
                self.root.after(0, lambda: self.headless_median_eff_var.set(f"Median Efficiency: {med_eff:.3f} F/s"))
                self._generate_trace_file(efficiency_scores)
                self.root.after(0, lambda: self._add_log_message(f"Headless complete. AvgEff: {avg_eff:.3f}, MedEff: {med_eff:.3f}"))
            else: self.root.after(0, lambda: self._add_log_message("No headless scores.")); self.root.after(0, lambda: self.headless_avg_eff_var.set("Avg. Eff: N/A")); self.root.after(0, lambda: self.headless_median_eff_var.set("Median Eff: N/A"))
            self.root.after(0, lambda: self.headless_status_var.set("Status: Idle"))
        except Exception as e:
            error_msg = f"Error in headless: {e}"; self.root.after(0, lambda: self._add_log_message(error_msg))
            import traceback; self.root.after(0, lambda: self._add_log_message(traceback.format_exc()))
            self.root.after(0, lambda: self.headless_status_var.set("Status: Error"))
        finally: self.root.after(0, lambda: self.start_headless_button.config(state="normal")); self.headless_run_active = False

    def _get_current_layer_configs_for_composite(self, agent_type_for_run: str) -> Optional[List[Dict[str, Any]]]: 
        if agent_type_for_run != "composite": return None
        active_layer_configs = []
        for block_data in self.composite_agent_configs_gui:
            agent_type = block_data["tk_vars"]["type"].get()
            if agent_type == "None": continue
            
            current_agent_params: Dict[str, Any] = {}
            if agent_type == "inverse_turn":
                try:
                    freq_thresh_val = block_data["tk_vars"]["param_val"].get()
                    current_agent_params["frequency_threshold"] = int(freq_thresh_val) 
                except (tk.TclError, AttributeError, KeyError): 
                    current_agent_params["frequency_threshold"] = agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]
                    self._add_log_message(f"Warning: Could not get freq_thresh_val for composite layer {agent_type}, using default.")
            
            try:
                layer_base_turn_rate = block_data["tk_vars"]["layer_base_turn_rate"].get()
            except (tk.TclError, AttributeError, KeyError):
                layer_base_turn_rate = agent_base_movement_params[1] 
                self._add_log_message(f"Warning: Could not get layer_base_turn_rate for composite layer {agent_type}, using default {layer_base_turn_rate}.")


            active_layer_configs.append({ 
                "agent_type_name": agent_type, 
                "num_instances": block_data["tk_vars"]["instances"].get(),
                "input_time_scale": block_data["tk_vars"]["input_ts"].get(), 
                "output_time_scale": block_data["tk_vars"]["output_ts"].get(),
                "agent_params": current_agent_params,
                "base_turn_rate_deg_dt": layer_base_turn_rate 
            })
        return active_layer_configs if active_layer_configs else None

    def _generate_trace_file(self, scores: List[float]):
        try:
            filepath = filedialog.asksaveasfilename(defaultextension=".txt", filetypes=[("Text files", "*.txt"), ("All files", "*.*")], title="Save Headless Run Trace File")
            if not filepath: self._add_log_message("Trace file saving cancelled."); return
            with open(filepath, "w") as f:
                f.write(f"Headless Simulation Trace - {time.strftime('%Y-%m-%d %H:%M:%S')}\n"); f.write(f"Agent Type: {self.headless_agent_type_var.get()}\n")
                f.write(f"#Agents/Instance: {self.headless_num_agents_var.get()}\n"); f.write(f"Expire Timer/Instance (s): {self.headless_expiry_timer_var.get()}\n")
                f.write(f"Target FPS: {self._tk_vars['fps'].get()} (FRAME_DT: {FRAME_DT:.6f}s)\n"); f.write("--- Efficiency Scores (Food/Second) ---\n")
                for i, score in enumerate(scores): f.write(f"Instance {i+1}: {score:.4f}\n")
            self._add_log_message(f"Trace file saved to {filepath}")
        except Exception as e: self._add_log_message(f"Error saving trace file: {e}"); messagebox.showerror("File Save Error", f"Could not save trace: {e}")

    def _handle_pygame_error_gui(self, error_msg: str):
        self._add_log_message(f"Pygame Error: {error_msg}"); messagebox.showerror("Pygame Error", f"Pygame display error: {error_msg}\nVisuals unavailable.")
        self._ensure_gui_stopped_state()

    def _ensure_gui_stopped_state(self):
        self.running_simulation = False; self.start_button.config(state="normal"); self.stop_button.config(state="disabled")
        if hasattr(self, 'sim_status_var') and self.sim_status_var.get() != "Simulation: Error": self.sim_status_var.set("Simulation: Idle")

    def _periodic_status_update(self):
        if not self.root.winfo_exists(): return
        if self.running_simulation and not self.headless_run_active:
            try:
                stats = get_simulation_stats()
                self.total_food_var.set(f"Total Food: {stats.get('total_food', 'N/A')}"); self.agents_active_var.set(f"Active Agents: {stats.get('active_agents', 'N/A')}")
                self.food_eaten_var.set(f"Food Eaten: {stats.get('food_eaten_run', 'N/A')}")
                if self.simulation_start_time_visual is not None:
                    elapsed = time.time() - self.simulation_start_time_visual
                    eff = stats.get('food_eaten_run', 0) / elapsed if elapsed > 0.01 else 0.0
                    self.efficiency_var.set(f"Efficiency (F/s): {eff:.2f}")
                else: self.efficiency_var.set("Efficiency (F/s): N/A")
            except Exception: pass 
        elif not self.headless_run_active : 
            self.total_food_var.set("Total Food: N/A (Idle)"); self.agents_active_var.set("Active Agents: N/A (Idle)")
            self.food_eaten_var.set("Food Eaten: N/A (Idle)"); self.efficiency_var.set("Efficiency (F/s): N/A (Idle)")
        self.root.after(1000, self._periodic_status_update)

    def quit_application(self):
        self._add_log_message("Quit application requested.")
        self.headless_run_active = False; self.stop_simulation() 
        if self.sim_thread and self.sim_thread.is_alive(): self.sim_thread.join(0.2) 
        self._destroy_root()

    def _destroy_root(self):
        if self.root.winfo_exists(): self.root.quit(); self.root.destroy()

if __name__ == "__main__":
    try: app = SimGUI(); app.root.mainloop()
    except Exception as e: import traceback; print(f"Fatal app error: {e}"); traceback.print_exc(); sys.exit(1)