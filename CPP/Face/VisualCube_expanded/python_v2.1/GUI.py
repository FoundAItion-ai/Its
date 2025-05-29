from __future__ import annotations
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import pygame
from typing import Dict, List, Any, Sequence # Ensure all are imported

import config as cfg
from main import ( 
    configure as main_configure,
    run_frame as main_run_frame,
    AGENT_CLASSES as main_agent_classes, 
    init_pygame_surface as main_init_pygame_surface,
    get_simulation_stats
)
from agent_module import (
    set_parameters as set_agent_parameters,
    agent_base_movement_params, 
    AGENT_CONFIG as agent_module_AGENT_CONFIG,
    _ATOMIC_AGENT_CLASSES # For populating layer agent type comboboxes
)
from math_module import set_environment_density, set_frame_dt, FRAME_DT

# Helper classes for Tkinter variables with rounding for display
class VarD(tk.DoubleVar):
    def __init__(self, v: float): super().__init__(value=round(v, 4))
class VarI(tk.IntVar):
    def __init__(self, v: int): super().__init__(value=v)

_COMPOSITE_LAYER_AGENT_TYPES = ["None"] + list(_ATOMIC_AGENT_CLASSES.keys())


class SimGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Agent Sim Control Panel - Vector Physics")
        self.root.geometry("480x800+50+50") # Increased height for new tab

        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=5, pady=5)

        self.controls_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.controls_tab, text="Controls")
        
        self.composite_config_tab = ttk.Frame(self.notebook) # New tab
        self.notebook.add(self.composite_config_tab, text="Composite Config", state="disabled")

        self.physics_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.physics_tab, text="Physics")
        self.status_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.status_tab, text="Status")

        self._tk_vars = {
            "threshold": VarD(agent_module_AGENT_CONFIG.get("CONSTANT_THRESHOLD", 0.5)),
            "initial_thrust": VarD(agent_base_movement_params[0]),
            "base_turn_rate_dt": VarD(agent_base_movement_params[1]),
            "accel_step": VarD(agent_module_AGENT_CONFIG.get("ACCEL_STEP", 0.1)),
            "decel_step": VarD(agent_module_AGENT_CONFIG.get("DECEL_STEP", 0.1)),
            "fps": VarI(int(1 / FRAME_DT if FRAME_DT > 0 else 60)),
            "n_agents": VarI(1),
            "spawn_x": VarD(cfg.WINDOW_W / 2),
            "spawn_y": VarD(cfg.WINDOW_H / 2),
            "env_density": VarD(1.225),
            "agent_mass": VarD(cfg.AGENT_DEFAULT_MASS),
            "agent_max_thrust_force": VarD(cfg.AGENT_DEFAULT_MAX_THRUST),
            "agent_drag_coeff": VarD(cfg.AGENT_DEFAULT_DRAG_COEFF),
            "agent_frontal_area": VarD(cfg.AGENT_DEFAULT_FRONTAL_AREA),
            # These are now for atomic agents or the composite agent's *own* time scales
            "input_time_scale": VarI(agent_module_AGENT_CONFIG.get("INPUT_TIME_SCALE_FALLBACK", 30)),
            "output_time_scale": VarI(agent_module_AGENT_CONFIG.get("OUTPUT_TIME_SCALE_FALLBACK", 30)),
        }
        
        # Tkinter variables for composite agent layer configurations
        self.layer_configs_vars: List[Dict[str, tk.Variable]] = []
        for i in range(3): # Max 3 layers
            layer_vars = {
                "type": tk.StringVar(value="None"),
                "instances": tk.IntVar(value=1),
                "input_ts": tk.IntVar(value=agent_module_AGENT_CONFIG.get("INPUT_TIME_SCALE_FALLBACK", 30)),
                "output_ts": tk.IntVar(value=agent_module_AGENT_CONFIG.get("OUTPUT_TIME_SCALE_FALLBACK", 30)),
                # Default to slightly increasing for constant_threshold for demo
                "param_val": tk.DoubleVar(value=round(agent_module_AGENT_CONFIG.get("CONSTANT_THRESHOLD", 0.5) + i*0.1, 2)) 
            }
            self.layer_configs_vars.append(layer_vars)


        self._build_controls_tab(self.controls_tab)
        self._build_composite_config_tab(self.composite_config_tab) # New
        self._build_physics_tab(self.physics_tab)
        self._build_status_tab(self.status_tab)

        self.running_simulation = False
        self.sim_thread = None
        self.pygame_surface = None
        self.root.protocol("WM_DELETE_WINDOW", self.quit_application)
        self._periodic_status_update()

    def _add_slider_entry(self, parent, key_or_var, label_text, from_, to_, is_var_direct=False):
        row_frame = ttk.Frame(parent)
        row_frame.pack(fill="x", pady=2)
        ttk.Label(row_frame, text=label_text, width=28).pack(side="left")
        var = key_or_var if is_var_direct else self._tk_vars[key_or_var]
        # Determine if var is DoubleVar or IntVar for resolution
        resolution = 0.01 if isinstance(var, tk.DoubleVar) else 1
        ttk.Scale(row_frame, variable=var, from_=from_, to=to_, orient="horizontal").pack(side="left", fill="x", expand=True, padx=5)
        entry_width = 7 if isinstance(var, tk.DoubleVar) else 5
        ttk.Entry(row_frame, textvariable=var, width=entry_width).pack(side="left")

    def _build_controls_tab(self, parent_tab):
        main_frame = ttk.Frame(parent_tab, padding=10)
        main_frame.pack(fill="both", expand=True)

        agent_logic_frame = ttk.LabelFrame(main_frame, text="Global Agent Logic Defaults")
        agent_logic_frame.pack(fill="x", pady=5)
        self._add_slider_entry(agent_logic_frame, "threshold", "Food Sense Threshold (Global)", 0, 1)
        self._add_slider_entry(agent_logic_frame, "accel_step", "Accel Step (Global Default)", 0.01, 0.5)
        self._add_slider_entry(agent_logic_frame, "decel_step", "Decel Step (Global Default)", 0.01, 0.5)
        
        # These are for atomic agents, or composite agent's *own* time scales
        time_scale_frame = ttk.LabelFrame(main_frame, text="Agent/Composite Time Scales")
        time_scale_frame.pack(fill="x", pady=5)
        self._add_slider_entry(time_scale_frame, "input_time_scale", "Input Avg Time (frames)", 5, 100)
        self._add_slider_entry(time_scale_frame, "output_time_scale", "Output/Aggregation Time (frames)", 5, 100)

        agent_motion_frame = ttk.LabelFrame(main_frame, text="Base Agent Motion (Initial)")
        agent_motion_frame.pack(fill="x", pady=5)
        self._add_slider_entry(agent_motion_frame, "initial_thrust", "Initial Thrust (0-1)", 0.0, 1.0)
        self._add_slider_entry(agent_motion_frame, "base_turn_rate_dt", "Initial Base Turn Rate (°/frame)", 0.0, 30.0)

        world_setup_frame = ttk.LabelFrame(main_frame, text="World & Agent Setup")
        world_setup_frame.pack(fill="x", pady=5)
        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="# Agents", width=28).pack(side="left")
        ttk.Spinbox(f, from_=1, to=50, textvariable=self._tk_vars["n_agents"], width=10).pack(side="left", fill="x", expand=True, padx=5)
        self._add_slider_entry(world_setup_frame, "spawn_x", "Spawn X", 0, cfg.WINDOW_W)
        self._add_slider_entry(world_setup_frame, "spawn_y", "Spawn Y", 0, cfg.WINDOW_H)

        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Food Preset", width=28).pack(side="left")
        self.food_sel = ttk.Combobox(f, values=list(cfg.FOOD_PRESETS.keys()), state="readonly", width=18)
        self.food_sel.current(0); self.food_sel.pack(side="left", fill="x", expand=True, padx=5)

        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Agent Type", width=28).pack(side="left")
        self.agent_type_var = tk.StringVar() # Used to trace selection
        self.agent_sel = ttk.Combobox(f, values=list(main_agent_classes.keys()), 
                                      state="readonly", width=18, textvariable=self.agent_type_var)
        if list(main_agent_classes.keys()):
            self.agent_type_var.set(list(main_agent_classes.keys())[0]) # Set var, triggers trace
        self.agent_sel.pack(side="left", fill="x", expand=True, padx=5)
        self.agent_type_var.trace_add("write", self._on_agent_type_change)


        buttons_frame = ttk.Frame(main_frame)
        buttons_frame.pack(fill="x", pady=10, side="bottom")
        self.start_button = ttk.Button(buttons_frame, text="Start Simulation", command=self.start_simulation)
        self.start_button.pack(side="left", expand=True, fill="x", padx=5)
        self.stop_button = ttk.Button(buttons_frame, text="Stop Simulation", command=self.stop_simulation, state="disabled")
        self.stop_button.pack(side="left", expand=True, fill="x", padx=5)
        ttk.Button(buttons_frame, text="Quit App", command=self.quit_application).pack(side="left", expand=True, fill="x", padx=5)

    def _build_composite_config_tab(self, parent_tab):
        main_frame = ttk.Frame(parent_tab, padding=10)
        main_frame.pack(fill="both", expand=True)

        ttk.Label(main_frame, text="Configure layers for the composite agent.", justify="center").pack(pady=5)
        ttk.Label(main_frame, text="Layer Specific Param is 'Constant Threshold' for InverseTurn agents.", 
                  font=("Arial", 8), justify="center").pack(pady=(0,10))


        for i in range(3): # 3 Layers
            layer_frame = ttk.LabelFrame(main_frame, text=f"Layer {i+1}")
            layer_frame.pack(fill="x", expand=True, pady=5, padx=5)
            
            # Agent Type
            f = ttk.Frame(layer_frame); f.pack(fill="x", pady=2)
            ttk.Label(f, text="Agent Type:", width=15).pack(side="left")
            type_combo = ttk.Combobox(f, textvariable=self.layer_configs_vars[i]["type"], 
                                      values=_COMPOSITE_LAYER_AGENT_TYPES, state="readonly", width=15)
            type_combo.pack(side="left", padx=5)
            # Callback to enable/disable other widgets in this layer based on "None" selection
            type_combo.bind("<<ComboboxSelected>>", lambda e, l=i: self._toggle_layer_widgets(l))


            # Num Instances
            f = ttk.Frame(layer_frame); f.pack(fill="x", pady=2)
            ttk.Label(f, text="# Instances:", width=15).pack(side="left")
            self.layer_configs_vars[i]["instances_spinbox"] = ttk.Spinbox(f, from_=1, to=10, 
                               textvariable=self.layer_configs_vars[i]["instances"], width=5)
            self.layer_configs_vars[i]["instances_spinbox"].pack(side="left", padx=5)


            # Input Time Scale
            self._add_slider_entry(layer_frame, self.layer_configs_vars[i]["input_ts"], "Input Time Scale", 5, 100, is_var_direct=True)
            # Output Time Scale
            self._add_slider_entry(layer_frame, self.layer_configs_vars[i]["output_ts"], "Output Time Scale", 5, 100, is_var_direct=True)
            # Layer Specific Parameter
            self._add_slider_entry(layer_frame, self.layer_configs_vars[i]["param_val"], "Layer Spec. Param", 0.0, 1.0, is_var_direct=True)
            
            self.layer_configs_vars[i]["widgets_frame"] = layer_frame # Store frame to easily access children

        # Initial state based on default "None" selection for layers (or current selection)
        for i in range(3):
             self._toggle_layer_widgets(i)


    def _toggle_layer_widgets(self, layer_index: int):
        is_active = self.layer_configs_vars[layer_index]["type"].get() != "None"
        layer_widgets_frame = self.layer_configs_vars[layer_index]["widgets_frame"]
        
        # Iterate over child widgets of the layer_frame, skipping the first row (type selection)
        # This is a bit fragile; direct references to widgets would be better if stored.
        # For now, let's try to target specific widgets or all children after the type combo.
        # The spinbox was stored, so we can use that. Scales/Entries are children of row_frames.
        
        self.layer_configs_vars[layer_index]["instances_spinbox"].config(state=tk.NORMAL if is_active else tk.DISABLED)

        # For sliders (Scales and Entries are children of "row_frame"s inside layer_frame)
        # Children of layer_frame are: type_frame, instances_frame, 3x row_frames for sliders.
        for child_widget in layer_widgets_frame.winfo_children():
            if isinstance(child_widget, ttk.Frame): # These are the row_frames for sliders + type/instances frames
                # Check if it's one of the slider frames
                is_slider_row = False
                for grand_child in child_widget.winfo_children():
                    if isinstance(grand_child, ttk.Scale):
                        is_slider_row = True
                        break
                
                if is_slider_row:
                    for grand_child in child_widget.winfo_children(): # Scale, Entry, Label
                        try: # Combobox doesn't have 'state' like this
                            grand_child.config(state=(tk.NORMAL if is_active else tk.DISABLED))
                        except tk.TclError:
                            pass


    def _on_agent_type_change(self, *args):
        selected_type = self.agent_type_var.get()
        composite_tab_index = -1
        for i in range(self.notebook.index("end")):
            if self.notebook.tab(i, "text") == "Composite Config":
                composite_tab_index = i
                break
        
        if composite_tab_index != -1:
            if selected_type == "composite":
                self.notebook.tab(composite_tab_index, state="normal")
                self.notebook.select(composite_tab_index) # Optionally switch to the tab
            else:
                self.notebook.tab(composite_tab_index, state="disabled")

    def _build_physics_tab(self, parent_tab):
        main_frame = ttk.Frame(parent_tab, padding=10)
        main_frame.pack(fill="both", expand=True)

        env_frame = ttk.LabelFrame(main_frame, text="Environment Physics")
        env_frame.pack(fill="x", pady=5)
        self._add_slider_entry(env_frame, "fps", "Target FPS (sets ∆t)", 10, 240)
        self._add_slider_entry(env_frame, "env_density", "Environment Density (kg/m³)", 0.0, 2.0)

        agent_phys_frame = ttk.LabelFrame(main_frame, text="Agent Base Physics Props")
        agent_phys_frame.pack(fill="x", pady=5)
        self._add_slider_entry(agent_phys_frame, "agent_mass", "Agent Mass (kg)", 0.1, 10.0)
        self._add_slider_entry(agent_phys_frame, "agent_max_thrust_force", "Max Thrust Force (N)", 1.0, 100.0)
        self._add_slider_entry(agent_phys_frame, "agent_drag_coeff", "Drag Coeff (Cd)", 0.0, 2.0)
        self._add_slider_entry(agent_phys_frame, "agent_frontal_area", "Frontal Area (m²)", 0.001, 0.1)

    def _build_status_tab(self, parent_tab):
        status_frame = ttk.LabelFrame(parent_tab, text="Live Simulation Status", padding=10)
        status_frame.pack(fill="both", expand=True)
        self.sim_status_var = tk.StringVar(value="Simulation: Idle")
        ttk.Label(status_frame, textvariable=self.sim_status_var, font=("Arial", 12)).pack(anchor="w", pady=5)

        self.total_food_var = tk.StringVar(value="Total Food Items: N/A")
        ttk.Label(status_frame, textvariable=self.total_food_var).pack(anchor="w", pady=2)
        self.agents_active_var = tk.StringVar(value="Active Agents: N/A")
        ttk.Label(status_frame, textvariable=self.agents_active_var).pack(anchor="w", pady=2)
        self.food_eaten_var = tk.StringVar(value="Food Eaten This Run: N/A")
        ttk.Label(status_frame, textvariable=self.food_eaten_var).pack(anchor="w", pady=2)

        ttk.Separator(status_frame, orient='horizontal').pack(fill='x', pady=10)
        log_frame = ttk.LabelFrame(status_frame, text="Event Log")
        log_frame.pack(fill="both", expand=True, pady=5)
        self.log_text = tk.Text(log_frame, height=8, state="disabled", wrap="word")
        log_scroll = ttk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.config(yscrollcommand=log_scroll.set)
        log_scroll.pack(side="right", fill="y")
        self.log_text.pack(side="left", fill="both", expand=True)
        self._add_log_message("GUI Initialized.")

    def _add_log_message(self, message: str):
        if hasattr(self, 'log_text'):
            self.log_text.config(state="normal")
            self.log_text.insert(tk.END, f"{message}\n")
            self.log_text.see(tk.END)
            self.log_text.config(state="disabled")

    def _apply_settings(self):
        self._add_log_message("Applying settings...")

        set_agent_parameters(
            CONSTANT_THRESHOLD=self._tk_vars["threshold"].get(),
            ACCEL_STEP=self._tk_vars["accel_step"].get(),
            DECEL_STEP=self._tk_vars["decel_step"].get()
        )

        fps_val = self._tk_vars["fps"].get()
        set_frame_dt(1.0 / fps_val if fps_val > 0 else (1.0 / 60.0))
        set_environment_density(self._tk_vars["env_density"].get())

        cfg.AGENT_DEFAULT_MASS = self._tk_vars["agent_mass"].get()
        cfg.AGENT_DEFAULT_MAX_THRUST = self._tk_vars["agent_max_thrust_force"].get()
        cfg.AGENT_DEFAULT_DRAG_COEFF = self._tk_vars["agent_drag_coeff"].get()
        cfg.AGENT_DEFAULT_FRONTAL_AREA = self._tk_vars["agent_frontal_area"].get()

        initial_thrust_val = self._tk_vars["initial_thrust"].get()
        base_turn_rate_val = self._tk_vars["base_turn_rate_dt"].get()
        agent_base_movement_params[0] = initial_thrust_val
        agent_base_movement_params[1] = base_turn_rate_val

        # These are composite's own time scales, or atomic agent's time scales if not composite
        input_ts_val = self._tk_vars["input_time_scale"].get()
        output_ts_val = self._tk_vars["output_time_scale"].get()

        # Prepare arguments for main_configure
        main_configure_kwargs = {
            "food_preset": self.food_sel.get(),
            "agent_type": self.agent_type_var.get(),
            "n_agents": int(self._tk_vars["n_agents"].get()),
            "spawn_point": (self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()),
            "initial_thrust": initial_thrust_val,
            "base_turn_deg_dt": base_turn_rate_val,
            "input_time_scale": input_ts_val, # Composite's own ITS or atomic's ITS
            "output_time_scale": output_ts_val, # Composite's own OTS or atomic's OTS
            "layer_configs_for_composite": None # Default
        }

        if self.agent_type_var.get() == "composite":
            gui_layer_configs: List[Dict[str, Any]] = []
            last_inverse_turn_threshold = -float('inf')
            error_messages = []

            for i, layer_vars_dict in enumerate(self.layer_configs_vars):
                agent_type = layer_vars_dict["type"].get()
                if agent_type == "None":
                    continue

                num_instances = layer_vars_dict["instances"].get()
                layer_input_ts = layer_vars_dict["input_ts"].get()
                layer_output_ts = layer_vars_dict["output_ts"].get()
                param_val = layer_vars_dict["param_val"].get()

                current_agent_params: Dict[str, Any] = {
                    # Layers will use global accel/decel unless we add specific GUI for them.
                    # If needed, could add:
                    # "accel_step": layer_vars_dict["accel_step"].get(),
                    # "decel_step": layer_vars_dict["decel_step"].get(),
                }
                if agent_type == "inverse_turn":
                    current_agent_params["constant_threshold"] = param_val
                    if param_val <= last_inverse_turn_threshold:
                        error_messages.append(
                            f"Layer {i+1} (inverse_turn): Threshold ({param_val:.2f}) "
                            f"must be > previous inverse_turn layer's threshold ({last_inverse_turn_threshold:.2f})."
                        )
                    last_inverse_turn_threshold = param_val
                elif agent_type == "deceleration": # Example: make param_val influence turn for decel
                    # current_agent_params["turn_influence_on_decel"] = param_val * 0.1 # Scaled
                    pass # For now, param_val is primarily for constant_threshold

                gui_layer_configs.append({
                    "agent_type_name": agent_type,
                    "num_instances": num_instances,
                    "input_time_scale": layer_input_ts,
                    "output_time_scale": layer_output_ts,
                    "agent_params": current_agent_params
                })
            
            if error_messages:
                messagebox.showerror("Composite Config Error", "\n".join(error_messages))
                self._add_log_message("Composite configuration error. Simulation not started.")
                return 

            if not gui_layer_configs:
                 messagebox.showwarning("Composite Config Warning", 
                                        "Composite agent selected, but no layers are active.\n"
                                        "A default stochastic layer will be used.")
            
            main_configure_kwargs["layer_configs_for_composite"] = gui_layer_configs
        
        main_configure(**main_configure_kwargs)
        self._add_log_message(f"World configured. Agent Type: {self.agent_type_var.get()}. FPS: {fps_val}, "
                              f"Thrust: {initial_thrust_val:.2f}, Turn: {base_turn_rate_val:.2f}°/fr, "
                              f"ITS: {input_ts_val}, OTS: {output_ts_val}")

    def start_simulation(self):
        if self.running_simulation:
            self.stop_simulation() # Ensure proper cleanup before restarting
        
        # Call _apply_settings which now might return if there's a config error
        # Need to check if it returned early due to an error.
        # For simplicity, assume _apply_settings shows messagebox and logs, then returns.
        # The actual simulation start should only proceed if settings were applied successfully.
        # This requires _apply_settings to return a status or for start_simulation to check a flag.
        # For now, let's assume if _apply_settings finds an error, it shows a message box and
        # does not proceed to call main_configure(), thus the simulation setup would be incomplete or old.
        # The check for error_messages in _apply_settings handles this by returning.
        # So, if we reach here after _apply_settings, it means no critical GUI validation errors occurred.
        self._apply_settings()
        # A more robust way would be for _apply_settings to return True/False
        # if not self._apply_settings(): return # If apply_settings indicates failure

        self.running_simulation = True
        self.start_button.config(state="disabled")
        self.stop_button.config(state="normal")
        self.sim_status_var.set("Simulation: Starting...")
        self._add_log_message("Starting simulation thread...")
        self.sim_thread = threading.Thread(target=self._simulation_loop_thread, daemon=True)
        self.sim_thread.start()

    def stop_simulation(self):
        if not self.running_simulation:
            return
        self._add_log_message("Stopping simulation...")
        self.sim_status_var.set("Simulation: Stopping...")
        self.running_simulation = False
        if self.sim_thread and self.sim_thread.is_alive():
            try:
                self.sim_thread.join(timeout=1.0) # Reduced timeout
            except RuntimeError:
                pass # Thread might have already exited
        self.sim_thread = None
        # Do not nullify pygame_surface here, it might be needed if thread closes it async
        self.start_button.config(state="normal")
        self.stop_button.config(state="disabled")
        self.sim_status_var.set("Simulation: Idle")
        self._add_log_message("Simulation stopped.")

    def _simulation_loop_thread(self):
        self._add_log_message("Simulation thread active.")
        pygame.init()
        try:
            current_pygame_surface = pygame.display.set_mode((cfg.WINDOW_W, cfg.WINDOW_H))
            self.pygame_surface = current_pygame_surface # Store for main_init_pygame_surface
            pygame.display.set_caption("Agent Simulation Visuals")
            self.root.after(0, lambda: self.sim_status_var.set("Simulation: Running"))
            self._add_log_message(f"Pygame window ({cfg.WINDOW_W}x{cfg.WINDOW_H}) created.")
        except pygame.error as e:
            self.root.after(0, lambda: self._handle_pygame_error_gui(str(e)))
            if pygame.get_init(): pygame.quit()
            self.running_simulation = False # Ensure flag is set
            # Call _ensure_gui_stopped_state directly from here might be problematic due to threading
            # self.root.after(0, self._ensure_gui_stopped_state) is safer
            return

        main_init_pygame_surface(self.pygame_surface)
        clock = pygame.time.Clock()

        while self.running_simulation:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running_simulation = False # Signal loop to stop
                    # self.root.after(0, self.stop_simulation) # Request GUI to handle full stop
                    break 
            if not self.running_simulation: # Check flag again after event processing
                break
            
            main_run_frame() # Runs WORLD.step() and WORLD.draw()
            pygame.display.flip()
            
            try:
                current_fps_target = self._tk_vars["fps"].get()
            except tk.TclError: # GUI might be closing
                self.running_simulation = False 
                break
            clock.tick(current_fps_target)

        # Cleanup Pygame outside the loop
        if pygame.get_init():
            pygame.quit()
        self.pygame_surface = None # Clear reference
        self._add_log_message("Simulation thread finished, Pygame quit.")
        self.root.after(0, self._ensure_gui_stopped_state) # Ensure GUI reflects stopped state

    def _handle_pygame_error_gui(self, error_msg: str):
        self._add_log_message(f"Pygame Error: {error_msg}")
        messagebox.showerror("Pygame Error", f"Could not initialize Pygame display: {error_msg}\n"
                                           "Simulation visuals will not be available.")
        self._ensure_gui_stopped_state() # Make sure GUI is reset

    def _ensure_gui_stopped_state(self):
        # This function is called via self.root.after, so it's GUI thread safe
        self.running_simulation = False # Ensure flag is consistent
        self.start_button.config(state="normal")
        self.stop_button.config(state="disabled")
        if self.sim_status_var.get() != "Simulation: Error": # Don't overwrite error status
            self.sim_status_var.set("Simulation: Idle")

    def _periodic_status_update(self):
        if not self.root.winfo_exists():
            return
        if self.running_simulation:
            try:
                stats = get_simulation_stats()
                self.total_food_var.set(f"Total Food: {stats.get('total_food', 'N/A')}")
                self.agents_active_var.set(f"Active Agents: {stats.get('active_agents', 'N/A')}")
                self.food_eaten_var.set(f"Food Eaten: {stats.get('food_eaten_run', 'N/A')}")
            except Exception:
                pass # Avoid errors if world is in transient state
        else:
            self.total_food_var.set("Total Food: N/A (Idle)")
            self.agents_active_var.set("Active Agents: N/A (Idle)")
            self.food_eaten_var.set("Food Eaten: N/A (Idle)")
        self.root.after(1000, self._periodic_status_update)

    def quit_application(self):
        self._add_log_message("Quit application requested.")
        self.stop_simulation() # Attempt to stop simulation thread gracefully
        # Wait a very brief moment for thread to potentially join
        if self.sim_thread and self.sim_thread.is_alive():
            self.sim_thread.join(0.2) 
        self._destroy_root()

    def _destroy_root(self):
        if self.root.winfo_exists():
            self.root.quit()
            self.root.destroy()