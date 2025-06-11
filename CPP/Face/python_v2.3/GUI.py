from __future__ import annotations
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import pygame
from typing import Dict, List, Any, Sequence, Optional
import time
from statistics import mean, median
import math # Import math for ceiling if needed

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
    AGENT_CONFIG as agent_module_AGENT_CONFIG # Correctly imported and aliased
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
            # Controller-specific defaults for the MAIN agent - Use the aliased name
            "input_time_scale": VarI(agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"]),
            "output_time_scale": VarI(agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]),
            "decay_timer_frames": VarI(agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"]), # New global decay timer
            "inverse_turn_freq_thresh": VarI(agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]),
            "composite_signal_threshold": VarD(agent_module_AGENT_CONFIG["DEFAULT_COMPOSITE_SIGNAL_THRESHOLD"]), # New composite threshold
        }

        self.composite_agent_configs_gui: List[Dict[str, Any]] = []
        self.composite_config_canvas_frame: Optional[ttk.Frame] = None
        self.specific_agent_params_frame: Optional[ttk.LabelFrame] = None
        self.main_base_turn_rate_slider_frame: Optional[ttk.Frame] = None

        self.headless_agent_type_var = tk.StringVar(); self.headless_agent_type_var.set(list(main_agent_classes.keys())[0] if main_agent_classes else "")
        self.headless_expiry_timer_var = VarI(30); self.headless_num_agents_var = VarI(10); self.headless_num_instances_var = VarI(5)
        self.headless_avg_eff_var = tk.StringVar(value="Avg. Efficiency: N/A"); self.headless_median_eff_var = tk.StringVar(value="Median Efficiency: N/A")
        self.headless_status_var = tk.StringVar(value="Status: Idle")

        # --- Build the GUI tabs ---
        # Call status tab building first, as it sets up the log widget and message function
        self._build_status_tab(self.status_tab)
        self._build_controls_tab(self.controls_tab)
        self._build_composite_config_tab(self.composite_config_tab)
        self._build_headless_tab(self.headless_tab)


        self.running_simulation = False; self.sim_thread = None; self.pygame_surface = None
        self.simulation_start_time_visual = None; self.headless_run_active = False
        self.root.protocol("WM_DELETE_WINDOW", self.quit_application)
        self._periodic_status_update()
        self._on_agent_type_change()

    # --- Utility Methods (Moved earlier) ---
    def _add_log_message(self, message: str):
        """Adds a timestamped message to the status log text widget."""
        # Ensure log_text exists before trying to use it
        if hasattr(self, 'log_text') and self.log_text.winfo_exists():
            self.log_text.config(state="normal") # Enable editing
            self.log_text.insert(tk.END, f"{time.strftime('%H:%M:%S')} - {message}\n")
            self.log_text.see(tk.END) # Scroll to the end
            self.log_text.config(state="disabled") # Disable editing

    def _handle_pygame_error_gui(self, error_msg: str):
        """Displays a Pygame error message and stops the visual sim."""
        self._add_log_message(f"Pygame Error: {error_msg}") # Use the now-defined log method
        messagebox.showerror("Pygame Error", f"Pygame display error: {error_msg}\nVisuals unavailable.")
        self._ensure_gui_stopped_state() # Ensure GUI buttons/status reflect stopped state

    def _ensure_gui_stopped_state(self):
        """Forces the GUI buttons and status to reflect a stopped simulation state."""
        self.running_simulation = False
        self.start_button.config(state="normal")
        self.stop_button.config(state="disabled")
        # Only set status to Idle if it's not already showing an error
        if hasattr(self, 'sim_status_var') and self.sim_status_var.get() != "Simulation: Error":
            self.sim_status_var.set("Simulation: Idle")

    def _periodic_status_update(self):
        """Updates the status tab labels periodically."""
        # Check if the main window still exists
        if not self.root.winfo_exists(): return

        # Update status only if visual sim is running and headless is not active
        if self.running_simulation and not self.headless_run_active:
            try:
                stats = get_simulation_stats()
                self.total_food_var.set(f"Total Food: {stats.get('total_food', 'N/A')}")
                self.agents_active_var.set(f"Active Agents: {stats.get('active_agents', 'N/A')}")
                self.food_eaten_var.set(f"Food Eaten This Run: {stats.get('food_eaten_run', 'N/A')}")

                # Calculate efficiency based on simulation time elapsed
                if self.simulation_start_time_visual is not None:
                    elapsed = time.time() - self.simulation_start_time_visual
                    eff = stats.get('food_eaten_run', 0) / elapsed if elapsed > 0.01 else 0.0 # Avoid division by near-zero
                    self.efficiency_var.set(f"Efficiency (F/s): {eff:.2f}")
                else:
                    self.efficiency_var.set("Efficiency (F/s): N/A (Starting...)") # Indicate starting or awaiting time
            except Exception:
                # Ignore errors during status update to keep GUI responsive
                pass
        elif not self.headless_run_active :
            # Reset status variables when not running visual sim or headless
            self.total_food_var.set("Total Food: N/A (Idle)")
            self.agents_active_var.set("Active Agents: N/A (Idle)")
            self.food_eaten_var.set("Food Eaten: N/A (Idle)")
            self.efficiency_var.set("Efficiency (F/s): N/A (Idle)")

        # Schedule the next update
        self.root.after(1000, self._periodic_status_update) # Update every 1000ms (1 second)

    def quit_application(self):
        """Gracefully shuts down the application."""
        self._add_log_message("Quit application requested.") # Use the now-defined log method
        # Signal headless runs to stop if active
        self.headless_run_active = False
        # Stop the visual simulation if active
        self.stop_simulation()

        # Wait briefly for threads to finish if they are still alive
        # (sim_thread is handled by stop_simulation)
        # Add check for headless thread if it were stored separately
        # For now, daemon=True means it won't block exit, but join is cleaner if possible

        # Destroy the root window
        self._destroy_root()

    def _destroy_root(self):
        """Destroys the main Tkinter window."""
        if self.root.winfo_exists():
            # Use root.quit() for the mainloop, then root.destroy()
            self.root.quit()
            self.root.destroy()
    # --- End Utility Methods ---


    def _add_slider_entry(self, parent, tk_var_instance, label_text, from_, to_, resolution_override=None, width=28, entry_width_override=None):
        row_frame = ttk.Frame(parent); row_frame.pack(fill="x", pady=2)
        ttk.Label(row_frame, text=label_text, width=width).pack(side="left")

        # Default resolution logic
        resolution = resolution_override
        if resolution is None:
             if isinstance(tk_var_instance, tk.DoubleVar):
                 # Use 0.01 for typical floats, but 1.0 for things like speed or orientation as per previous logic
                 # Add specific checks for labels to use 1.0
                 if "Speed" in label_text or "Orientation" in label_text or "Freq" in label_text or "Timer" in label_text or "#" in label_text:
                     resolution = 1.0
                 else: # Default float resolution
                     resolution = 0.01
             else: # IntVar
                 resolution = 1

        scale = ttk.Scale(row_frame, variable=tk_var_instance, from_=from_, to=to_, orient="horizontal")
        try: scale.configure(resolution=resolution)
        except tk.TclError: pass
        scale.pack(side="left", fill="x", expand=True, padx=5)
        entry_width = entry_width_override if entry_width_override is not None else (7 if isinstance(tk_var_instance, tk.DoubleVar) else 5)
        ttk.Entry(row_frame, textvariable=tk_var_instance, width=entry_width).pack(side="left")
        return row_frame

    def _build_controls_tab(self, parent_tab):
        main_frame = ttk.Frame(parent_tab, padding=10); main_frame.pack(fill="both", expand=True)

        time_decay_frame = ttk.LabelFrame(main_frame, text="Timing & Decay (Top Level)")
        time_decay_frame.pack(fill="x", pady=5)
        # These apply to the main agent selected, and the composite agent's overall timing/decay
        self._add_slider_entry(time_decay_frame, self._tk_vars["input_time_scale"], "Input Time Scale (frames)\n(Not for Composite Agg.)", 5, 100, width=28)
        self._add_slider_entry(time_decay_frame, self._tk_vars["output_time_scale"], "Output Time Scale (frames)\n(Composite Agg. Freq.)", 5, 100, width=28)
        self._add_slider_entry(time_decay_frame, self._tk_vars["decay_timer_frames"], "Decay Timer Max (frames)\n(Overall Decay Duration)", 1, 300, resolution_override=1, width=28) # New global decay timer control

        agent_motion_timing_frame = ttk.LabelFrame(main_frame, text="Agent Motion & Simulation Timing")
        agent_motion_timing_frame.pack(fill="x", pady=5)
        self.main_base_turn_rate_slider_frame = self._add_slider_entry(agent_motion_timing_frame,
                                                                      self._tk_vars["base_turn_rate_dt"],
                                                                      "Base Turn Rate (°/frame)\n(Agent Max Turn Cap)", # Clarified label
                                                                      0.0, 45.0, resolution_override=0.1, width=28)

        self._add_slider_entry(agent_motion_timing_frame, self._tk_vars["agent_constant_speed"], "Agent Constant Speed (px/s)", 10, 300, resolution_override=1.0)
        self._add_slider_entry(agent_motion_timing_frame, self._tk_vars["fps"], "Target FPS (sets ∆t)", 10, 240, resolution_override=1.0)

        world_setup_frame = ttk.LabelFrame(main_frame, text="World & Agent Setup")
        world_setup_frame.pack(fill="x", pady=5)
        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Agent Type (Visual Sim)", width=28).pack(side="left")
        self.agent_type_var = tk.StringVar()
        self.agent_sel = ttk.Combobox(f, values=list(main_agent_classes.keys()), state="readonly", width=18, textvariable=self.agent_type_var)
        if list(main_agent_classes.keys()): self.agent_type_var.set(list(main_agent_classes.keys())[0])
        self.agent_sel.pack(side="left", fill="x", expand=True, padx=5)
        self.agent_type_var.trace_add("write", self._on_agent_type_change)

        # Specific agent parameters frame placeholder
        self.specific_agent_params_frame = ttk.LabelFrame(main_frame, text="Specific Agent Parameters")

        f = ttk.Frame(world_setup_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="# Agents", width=28).pack(side="left")
        ttk.Spinbox(f, from_=1, to=150, textvariable=self._tk_vars["n_agents"], width=10).pack(side="left", fill="x", expand=True, padx=5)
        self._add_slider_entry(world_setup_frame, self._tk_vars["spawn_x"], "Spawn X", 0, cfg.WINDOW_W, resolution_override=1.0)
        self._add_slider_entry(world_setup_frame, self._tk_vars["spawn_y"], "Spawn Y", 0, cfg.WINDOW_H, resolution_override=1.0)
        self._add_slider_entry(world_setup_frame, self._tk_vars["initial_orientation_deg"], "Initial Orientation (°)", 0, 359, resolution_override=1.0)
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

        # New: Composite specific threshold control
        comp_thresh_frame = ttk.LabelFrame(control_frame, text="Composite Aggregation Settings")
        comp_thresh_frame.pack(fill="x", expand=True, pady=5)
        self._add_slider_entry(comp_thresh_frame, self._tk_vars["composite_signal_threshold"],
                               "Signal Sum Threshold (Magnitude)", 0.0, 5.0, resolution_override=0.01, width=30)

        ttk.Button(control_frame, text="Add Agent Config Block", command=self._add_composite_agent_config_gui_block).pack(side="left", padx=5, pady=5)

        canvas = tk.Canvas(parent_tab, borderwidth=0)
        scrollbar = ttk.Scrollbar(parent_tab, orient="vertical", command=canvas.yview)
        self.composite_config_canvas_frame = ttk.Frame(canvas, padding=5)
        self.composite_config_canvas_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=self.composite_config_canvas_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True); scrollbar.pack(side="right", fill="y")

        # Add an initial block - Use the aliased name
        self._add_composite_agent_config_gui_block(
            input_ts_val=agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
            output_ts_val=agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
            decay_val=agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"],
            param_val=agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]
        )


    def _add_composite_agent_config_gui_block(self, type_val="stochastic", instances_val=1,
                                            input_ts_val=None, # Use defaults if None
                                            output_ts_val=None,
                                            decay_val=None,
                                            param_val=None,
                                            weight_val=1.0
                                            ):
        """Adds a new configuration block for a layer in the Composite agent."""
        if not self.composite_config_canvas_frame: return
        block_id = len(self.composite_agent_configs_gui)
        block_frame = ttk.LabelFrame(self.composite_config_canvas_frame, text=f"Agent Config Block {block_id + 1}")
        block_frame.pack(fill="x", expand=True, pady=7, padx=5)

        # Use provided defaults or fallbacks from agent_module_AGENT_CONFIG
        input_ts = input_ts_val if input_ts_val is not None else agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"]
        output_ts = output_ts_val if output_ts_val is not None else agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]
        decay_frames = decay_val if decay_val is not None else agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"]
        # Handle param_val defaults based on assumed type before rebuilding
        initial_param_val_for_rebuild = param_val
        if initial_param_val_for_rebuild is None:
             initial_param_val_for_rebuild = agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"] if type_val == "inverse_turn" else 0.1


        # TK Vars for this block
        tk_vars = {
            "type": tk.StringVar(value=type_val),
            "instances": tk.IntVar(value=instances_val),
            "input_ts": tk.IntVar(value=input_ts),
            "output_ts": tk.IntVar(value=output_ts),
            "decay_frames": VarI(decay_frames), # New: Decay frames for this layer
            "weight": VarD(weight_val),      # New: Weight for this layer
            # param_val tk var will be added by _rebuild_param_slider based on type
        }
        widgets = {"frame": block_frame}

        # Row for Agent Type and Instances
        f = ttk.Frame(block_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Agent Type:", width=15).pack(side="left")
        type_combo = ttk.Combobox(f, textvariable=tk_vars["type"], values=_COMPOSITE_LAYER_AGENT_TYPES, state="readonly", width=15)
        type_combo.pack(side="left", padx=5); widgets["type_combo"] = type_combo
        # Bind event to update parameter slider and toggle widgets
        type_combo.bind("<<ComboboxSelected>>", lambda e, b_id=block_id: self._update_composite_block_param_type_and_toggle(b_id))

        ttk.Label(f, text="# Instances:", width=10).pack(side="left", padx=(10,0))
        instances_spinbox = ttk.Spinbox(f, from_=1, to=50, textvariable=tk_vars["instances"], width=5)
        instances_spinbox.pack(side="left", padx=5); widgets["instances_spinbox"] = instances_spinbox

        # Sliders for common layer parameters
        slider_width = 18
        widgets["input_ts_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["input_ts"], "Input Time Scale", 5, 100, width=slider_width)
        widgets["output_ts_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["output_ts"], "Output Time Scale", 5, 100, width=slider_width)
        widgets["decay_frames_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["decay_frames"], "Decay Max (frames)", 1, 300, resolution_override=1, width=slider_width) # New decay control for layer
        widgets["weight_slider_frame"] = self._add_slider_entry(block_frame, tk_vars["weight"], "Weight (Contribution)", 0.0, 5.0, resolution_override=0.01, width=slider_width) # New weight control

        # Frame specifically for the type-specific parameter slider
        param_frame = ttk.LabelFrame(block_frame, text="Type-Specific Parameter")
        param_frame.pack(fill="x", expand=True, pady=3)
        widgets["param_val_slider_container"] = param_frame

        # Build the appropriate parameter slider based on initial type
        self._rebuild_param_slider(block_id, tk_vars, widgets, initial_param_val_for_rebuild)

        remove_button = ttk.Button(block_frame, text="Remove Block", command=lambda b_id=block_id: self._remove_composite_agent_config_gui_block(b_id))
        remove_button.pack(pady=5); widgets["remove_button"] = remove_button

        # Store the block data and apply initial widget toggling
        self.composite_agent_configs_gui.append({"id": block_id, "tk_vars": tk_vars, "widgets": widgets})
        self._toggle_composite_block_widgets(block_id)

    def _rebuild_param_slider(self, block_id: int, tk_vars_dict: Dict[str, Any], widgets_dict: Dict[str, Any], current_value: float | int):
        """Destroys and rebuilds the type-specific parameter slider/widget."""
        container = widgets_dict.get("param_val_slider_container")
        if not container: return

        # Destroy old widgets in the container
        for widget in container.winfo_children(): widget.destroy()
        # Clear old references
        widgets_dict["param_val_slider_actual_frame"] = None
        widgets_dict["no_param_info_label"] = None
        # Remove old param_val var reference
        if "param_val" in tk_vars_dict: del tk_vars_dict["param_val"]

        selected_type = tk_vars_dict["type"].get()
        slider_width = 16 # Consistent width for sliders

        if selected_type == "inverse_turn":
            # Inverse Turn uses an Integer Var for frequency threshold
            tk_vars_dict["param_val"] = tk.IntVar(value=int(current_value))

            label_text = "Freq. Thresh (food signals)"
            from_ = 1
            to_ = 100
            resolution = 1

            slider_row = self._add_slider_entry(container, tk_vars_dict["param_val"], label_text, from_, to_, resolution_override=resolution, width=slider_width)
            widgets_dict["param_val_slider_actual_frame"] = slider_row

        else: # No type-specific parameters for other current types (stochastic, None)
            # Add a dummy DoubleVar to track 'param_val' key consistently, but it won't be used by the agent
            # Initialize with the passed current_value even if not used by the specific agent type
            tk_vars_dict["param_val"] = tk.DoubleVar(value=float(current_value))

            no_param_label = ttk.Label(container, text="No type-specific parameters for this agent type.")
            no_param_label.pack(pady=5, padx=5, anchor="center")
            widgets_dict["no_param_info_label"] = no_param_label


    def _update_composite_block_param_type_and_toggle(self, block_id: int):
        """Called when a block's agent type combobox selection changes."""
        block_data = next((b for b in self.composite_agent_configs_gui if b["id"] == block_id), None)
        if not block_data: return

        selected_type = block_data["tk_vars"]["type"].get()

        # Get current param value safely before rebuilding the Tk var
        current_param_value = None
        if "param_val" in block_data["tk_vars"]:
             try:
                current_param_value = block_data["tk_vars"]["param_val"].get()
             except tk.TclError:
                current_param_value = None # Failed to get current value

        # Set a sensible default if current_value is None or the type changed significantly
        if current_param_value is None or (selected_type == "inverse_turn" and not isinstance(current_param_value, int)) or (selected_type != "inverse_turn" and not isinstance(current_param_value, float)):
             # Use aliased AGENT_CONFIG
             current_param_value = agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"] if selected_type == "inverse_turn" else 0.1


        # Rebuild the parameter slider and its associated Tk variable
        self._rebuild_param_slider(block_id, block_data["tk_vars"], block_data["widgets"], current_param_value)

        # Toggle the state of widgets based on the new selected type ("None" vs active)
        self._toggle_composite_block_widgets(block_id)


    def _remove_composite_agent_config_gui_block(self, block_id_to_remove: int):
        """Removes a composite agent config block from the GUI and internal list."""
        block_to_remove = None
        # Find the block by ID and remove it from the list
        for i, block_data in enumerate(self.composite_agent_configs_gui):
            if block_data["id"] == block_id_to_remove: block_to_remove = block_data; self.composite_agent_configs_gui.pop(i); break

        # Destroy the GUI frame if it exists
        if block_to_remove and block_to_remove["widgets"]["frame"].winfo_exists():
            block_to_remove["widgets"]["frame"].destroy()

        # Update the text label for the remaining blocks
        for i, block_data in enumerate(self.composite_agent_configs_gui):
            if block_data["widgets"]["frame"].winfo_exists():
                 try: block_data["widgets"]["frame"].config(text=f"Agent Config Block {i + 1}")
                 except tk.TclError: pass # Frame might be in process of destruction

    def _toggle_composite_block_widgets(self, block_id: int):
        """Enables/disables widgets within a composite block based on its selected agent type."""
        block_data = next((b for b in self.composite_agent_configs_gui if b["id"] == block_id), None)
        if not block_data: return

        selected_agent_type_in_block = block_data["tk_vars"]["type"].get()
        is_block_logically_active = selected_agent_type_in_block != "None"

        # List of widgets to toggle state based on block activation
        widgets_to_toggle_based_on_block_activity = [
            block_data["widgets"]["instances_spinbox"],
            block_data["widgets"]["type_combo"], # Type combo is active even if "None" is selected, but including here for clarity
        ]

        # Add widgets from the common sliders (input/output time scale, decay, weight)
        for key in ["input_ts_slider_frame", "output_ts_slider_frame", "decay_frames_slider_frame", "weight_slider_frame"]:
            slider_main_frame = block_data["widgets"].get(key)
            if slider_main_frame and hasattr(slider_main_frame, 'winfo_children'):
                for widget_in_row in slider_main_frame.winfo_children():
                    widgets_to_toggle_based_on_block_activity.append(widget_in_row)

        # Apply state to common widgets
        for widget in widgets_to_toggle_based_on_block_activity:
            if widget and hasattr(widget, 'winfo_exists') and widget.winfo_exists():
                try: widget.config(state=(tk.NORMAL if is_block_logically_active else tk.DISABLED))
                except tk.TclError: pass

        # Handle the type-specific parameter area state
        param_actual_slider_frame = block_data["widgets"].get("param_val_slider_actual_frame")
        no_param_info_label = block_data["widgets"].get("no_param_info_label")

        # The parameter area is relevant and enabled only if the block is active AND it has a param slider
        is_param_area_relevant = is_block_logically_active and (param_actual_slider_frame is not None)

        # Toggle widgets within the actual parameter slider frame
        if param_actual_slider_frame and hasattr(param_actual_slider_frame, 'winfo_children'):
            for child_widget in param_actual_slider_frame.winfo_children():
                if hasattr(child_widget, 'winfo_exists') and child_widget.winfo_exists():
                    try: child_widget.config(state=(tk.NORMAL if is_param_area_relevant else tk.DISABLED))
                    except tk.TclError: pass

        # Toggle the "No parameter info" label state
        if no_param_info_label and hasattr(no_param_info_label, 'winfo_exists') and no_param_info_label.winfo_exists():
             # Show label if block is active AND it has NO param slider, otherwise hide/disable
             label_state = tk.NORMAL if is_block_logically_active and not param_actual_slider_frame else tk.DISABLED
             try: no_param_info_label.config(state=label_state)
             except tk.TclError: pass

    def _on_agent_type_change(self, *args):
        """Called when the main agent type selection changes."""
        selected_type = self.agent_type_var.get()

        # Enable/disable Composite Config tab
        composite_tab_index = -1
        try:
            # Find the index of the Composite Config tab
            for i in range(self.notebook.index("end")):
                if self.notebook.tab(i, "text") == "Composite Config":
                    composite_tab_index = i; break
        except tk.TclError: return # Handle case where notebook is somehow invalid

        if composite_tab_index != -1:
            self.notebook.tab(composite_tab_index, state="normal" if selected_type == "composite" else "disabled")

        # Show/hide specific agent parameters for the main agent
        # Clear existing widgets in the specific params frame
        if self.specific_agent_params_frame:
            for widget in self.specific_agent_params_frame.winfo_children(): widget.destroy()

            # Add parameters based on the selected agent type
            if selected_type == "inverse_turn":
                # Inverse Turn needs Frequency Threshold
                self._add_slider_entry(self.specific_agent_params_frame,
                                       self._tk_vars["inverse_turn_freq_thresh"],
                                       "Frequency Threshold (food signals)", 1, 100, resolution_override=1)
                # Ensure the frame is packed and visible
                self.specific_agent_params_frame.pack(fill="x", pady=(5,0), padx=0, after=self.agent_sel.master.master)
            elif selected_type == "composite":
                 # Composite agent has its own specific params in the Composite Config tab (threshold)
                 # and the overall decay timer applies to it. No extra params here.
                 self.specific_agent_params_frame.pack_forget() # Hide the frame
            else: # For agents like stochastic (no specific params in this section)
                self.specific_agent_params_frame.pack_forget() # Hide the frame

    def _build_headless_tab(self, parent_tab):
        main_frame = ttk.Frame(parent_tab, padding=10); main_frame.pack(fill="both", expand=True)
        config_frame = ttk.LabelFrame(main_frame, text="Headless Instance Configuration"); config_frame.pack(fill="x", pady=5, padx=5)

        # Agent Type selection for headless runs
        f = ttk.Frame(config_frame); f.pack(fill="x", pady=3); ttk.Label(f, text="Agent Type:", width=20).pack(side="left")
        headless_agent_sel = ttk.Combobox(f, values=list(main_agent_classes.keys()), state="readonly", width=20, textvariable=self.headless_agent_type_var)
        headless_agent_sel.pack(side="left", fill="x", expand=True, padx=5)

        # Other headless run parameters
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
        """Builds the status tab with live stats and event log."""
        status_frame = ttk.LabelFrame(parent_tab, text="Live Simulation Status", padding=10); status_frame.pack(fill="both", expand=True)
        self.sim_status_var = tk.StringVar(value="Simulation: Idle"); ttk.Label(status_frame, textvariable=self.sim_status_var, font=("Arial", 12)).pack(anchor="w", pady=5)
        self.total_food_var = tk.StringVar(value="Total Food Items: N/A"); ttk.Label(status_frame, textvariable=self.total_food_var).pack(anchor="w", pady=2)
        self.agents_active_var = tk.StringVar(value="Active Agents: N/A"); ttk.Label(status_frame, textvariable=self.agents_active_var).pack(anchor="w", pady=2)
        self.food_eaten_var = tk.StringVar(value="Food Eaten This Run: N/A"); ttk.Label(status_frame, textvariable=self.food_eaten_var).pack(anchor="w", pady=2)
        self.efficiency_var = tk.StringVar(value="Efficiency (F/s): N/A"); ttk.Label(status_frame, textvariable=self.efficiency_var).pack(anchor="w", pady=2)
        ttk.Separator(status_frame, orient='horizontal').pack(fill='x', pady=10)
        log_frame = ttk.LabelFrame(status_frame, text="Event Log"); log_frame.pack(fill="both", expand=True, pady=5)

        # --- Create the log_text widget BEFORE calling _add_log_message ---
        self.log_text = tk.Text(log_frame, height=8, state="disabled", wrap="word");
        log_scroll = ttk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.config(yscrollcommand=log_scroll.set)
        log_scroll.pack(side="right", fill="y")
        self.log_text.pack(side="left", fill="both", expand=True)
        # --- End reorder ---

        # Now this call should work because self.log_text exists and _add_log_message is defined earlier
        self._add_log_message("GUI Initialized.")


    def _apply_settings(self, for_headless=False) -> bool:
        """Gathers settings from GUI and calls main.configure."""
        self._add_log_message("Applying settings...")

        # --- Simulation Timing ---
        fps_val = self._tk_vars["fps"].get()
        current_dt = 1.0 / fps_val if fps_val > 0 else (1.0 / 60.0)
        set_frame_dt(current_dt)
        cfg.AGENT_CONSTANT_SPEED = self._tk_vars["agent_constant_speed"].get()

        # --- Top-Level Agent/SimAgent Configuration ---
        # This turn rate applies to the SimAgent kinematics and is the maximum for controllers
        sim_agent_base_turn_rate_val = self._tk_vars["base_turn_rate_dt"].get()

        # These apply to the top-level controller (whether atomic or composite)
        controller_input_ts_val = self._tk_vars["input_time_scale"].get()
        controller_output_ts_val = self._tk_vars["output_time_scale"].get()
        # This decay timer applies to the top-level controller (whether atomic or composite)
        decay_timer_frames_val = self._tk_vars["decay_timer_frames"].get()

        # --- Agent Type and Specific Parameters ---
        current_agent_type = self.agent_type_var.get() if not for_headless else self.headless_agent_type_var.get()

        agent_params_for_main_agent: Optional[Dict[str, Any]] = None
        layer_configs_for_composite: Optional[Sequence[Dict[str, Any]]] = None
        composite_signal_threshold: float = self._tk_vars["composite_signal_threshold"].get() # Composite threshold is top-level config

        if current_agent_type == "inverse_turn":
            # Parameters for Inverse Turn agent if it's the main agent
            agent_params_for_main_agent = {
                "frequency_threshold": self._tk_vars["inverse_turn_freq_thresh"].get()
            }
        elif current_agent_type == "composite":
             # Layer configurations and weights for the composite agent
             gui_layer_configs_data = self._get_current_layer_configs_for_composite()
             if not gui_layer_configs_data and not for_headless:
                 messagebox.showwarning("Composite Config Warning", "Composite agent: no active agent blocks configured. Using default single stochastic layer.")
                 # Fallback: Provide a default layer config if none are active - Use the aliased name
                 layer_configs_for_composite = [{
                     "agent_type_name": "stochastic", "num_instances": 1,
                     "input_time_scale": agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                     "output_time_scale": agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
                     "decay_timer_frames": agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"],
                     "weight": 1.0, "agent_params": {}
                 }]
             else:
                 layer_configs_for_composite = gui_layer_configs_data

        # --- Configure Main Simulation World ---
        main_configure_kwargs = {
            "food_preset": self.food_sel.get(),
            "agent_type": current_agent_type,
            "n_agents": int(self._tk_vars["n_agents"].get()) if not for_headless else self.headless_num_agents_var.get(),
            "spawn_point": (self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()),
            "initial_orientation_deg": self._tk_vars["initial_orientation_deg"].get(),
            "base_turn_deg_dt": sim_agent_base_turn_rate_val,
            "input_time_scale": controller_input_ts_val,
            "output_time_scale": controller_output_ts_val,
            "decay_timer_frames": decay_timer_frames_val, # Pass decay timer
            "composite_signal_threshold": composite_signal_threshold, # Pass composite threshold
            "layer_configs_for_composite": layer_configs_for_composite, # Pass layer configs
            "agent_constant_speed": cfg.AGENT_CONSTANT_SPEED,
            "agent_params_for_main_agent": agent_params_for_main_agent
        }

        try:
            main_configure(**main_configure_kwargs)
            self._add_log_message(f"World configured. Agent: {current_agent_type}, Speed: {cfg.AGENT_CONSTANT_SPEED:.1f}px/s, Max Turn: {sim_agent_base_turn_rate_val:.1f}°/fr, Decay Max: {decay_timer_frames_val}fr")
            if current_agent_type == "composite":
                 self._add_log_message(f"Composite Config: Threshold={composite_signal_threshold:.2f}, Layers={len(layer_configs_for_composite) if layer_configs_for_composite else 0}")
            return True
        except Exception as e:
            self._add_log_message(f"Configuration Error: {e}")
            import traceback; self._add_log_message(traceback.format_exc())
            messagebox.showerror("Configuration Error", f"Failed to apply settings:\n{e}")
            return False


    def start_simulation(self):
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run active."); return
        if self.running_simulation: self.stop_simulation()
        if not self._apply_settings(): return # Apply settings before starting

        self.running_simulation = True; self.simulation_start_time_visual = time.time()
        self.start_button.config(state="disabled"); self.stop_button.config(state="normal")
        self.sim_status_var.set("Simulation: Starting...")
        self._add_log_message("Starting visual simulation thread...")

        # Start the simulation loop in a separate thread
        self.sim_thread = threading.Thread(target=self._simulation_loop_thread, daemon=True)
        self.sim_thread.start()

    def stop_simulation(self):
        if not self.running_simulation: return
        self._add_log_message("Stopping visual simulation..."); self.sim_status_var.set("Simulation: Stopping...")
        self.running_simulation = False # Set flag to stop the thread loop

        # Wait briefly for the thread to finish
        if self.sim_thread and self.sim_thread.is_alive():
            try: self.sim_thread.join(timeout=1.0) # Give it 1 second to shut down gracefully
            except RuntimeError: pass # Ignore if thread couldn't be joined (e.g., already closing)
        self.sim_thread = None; self.simulation_start_time_visual = None

        # Ensure GUI buttons are reset
        self.start_button.config(state="normal"); self.stop_button.config(state="disabled")
        self.sim_status_var.set("Simulation: Idle"); self._add_log_message("Visual simulation stopped.")

    def _simulation_loop_thread(self):
        """The main loop for the visual simulation run."""
        self._add_log_message("Visual simulation thread active.")
        pygame.init()
        try:
            self.pygame_surface = pygame.display.set_mode((cfg.WINDOW_W, cfg.WINDOW_H))
            pygame.display.set_caption("Agent Simulation Visuals")
            # Update GUI status from the main thread using after()
            self.root.after(0, lambda: self.sim_status_var.set("Simulation: Running"))
            self._add_log_message(f"Pygame window created.")
        except pygame.error as e:
            # Handle Pygame errors on the GUI thread
            self.root.after(0, lambda: self._handle_pygame_error_gui(str(e)))
            if pygame.get_init(): pygame.quit()
            self.running_simulation = False # Ensure simulation stops
            self.root.after(0, self._ensure_gui_stopped_state) # Update GUI state
            return

        # Initialize the main simulation world's surface
        main_init_pygame_surface(self.pygame_surface)
        clock = pygame.time.Clock()

        while self.running_simulation:
            # Process events (like closing the window)
            for event in pygame.event.get():
                if event.type == pygame.QUIT: self.running_simulation = False; break
            if not self.running_simulation: break # Break if QUIT event was received

            # Run one step of the simulation and draw if needed
            main_run_frame(is_visual=True)
            pygame.display.flip()

            # Control frame rate
            try: current_fps_target = self._tk_vars["fps"].get()
            except tk.TclError: self.running_simulation = False; break # Stop if GUI var is invalid
            if current_fps_target <=0: current_fps_target = 1
            clock.tick(current_fps_target)

        # Simulation loop ended, clean up Pygame
        if pygame.get_init(): pygame.quit()
        self.pygame_surface = None # Clear surface reference
        self._add_log_message("Visual simulation thread finished.")
        # Update GUI state on the main thread
        self.root.after(0, self._ensure_gui_stopped_state)


    def start_headless_runs(self):
        """Initiates multiple simulation runs without a visual display."""
        if self.running_simulation: messagebox.showwarning("Busy", "Visual sim active."); return
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run in progress."); return

        # Store original GUI settings temporarily
        original_n_agents = self._tk_vars["n_agents"].get()
        original_agent_type = self.agent_type_var.get()

        # Temporarily set main GUI values to headless values for configuration purposes
        # This ensures FRAME_DT and potentially other global configs (like speed) match headless settings
        self._tk_vars["n_agents"].set(self.headless_num_agents_var.get())
        self.agent_type_var.set(self.headless_agent_type_var.get())

        # Apply settings using headless counts/type but main GUI params (FPS, speed, turn rate, decay, threshold)
        # This sets the global FRAME_DT correctly for the headless runs
        apply_ok = self._apply_settings(for_headless=True)

        # Restore original GUI values
        self._tk_vars["n_agents"].set(original_n_agents)
        self.agent_type_var.set(original_agent_type)

        if not apply_ok:
             self._add_log_message("Headless pre-configuration failed (check settings).");
             self.headless_status_var.set("Status: Config Error")
             return

        # Start the headless worker thread
        self.headless_run_active = True; self.start_headless_button.config(state="disabled")
        self.headless_status_var.set("Status: Starting...")
        self._add_log_message("Starting headless runs worker thread...")

        headless_thread = threading.Thread(target=self._headless_run_worker_thread, daemon=True)
        headless_thread.start()

    def _headless_run_worker_thread(self):
        """Worker thread that performs the multiple headless simulation runs."""
        try:
            num_instances = self.headless_num_instances_var.get()
            expiry_seconds = self.headless_expiry_timer_var.get()
            current_frame_dt = FRAME_DT # Use the globally set FRAME_DT

            if current_frame_dt <= 1e-9: # Check for near-zero or zero DT
                 error_msg = "FRAME_DT is zero or too small for headless duration calculation."
                 self.root.after(0, lambda: self._add_log_message(error_msg)); raise ValueError(error_msg)

            frames_per_instance = math.ceil(expiry_seconds / current_frame_dt) # Use ceil to ensure at least the duration

            self.root.after(0, lambda: self._add_log_message(f"Headless: Running {num_instances} instances, {expiry_seconds}s/instance (~{frames_per_instance} frames) at DT={current_frame_dt:.6f}s."))

            efficiency_scores = []

            # Get configuration details that apply to ALL headless instances
            # These come from the main GUI controls, applied once before the loop
            headless_agent_type_val = self.headless_agent_type_var.get()

            # Specific parameters for the main agent type (if not composite)
            agent_params_for_headless_main_agent = None
            if headless_agent_type_val == "inverse_turn":
                 agent_params_for_headless_main_agent = {
                    "frequency_threshold": self._tk_vars["inverse_turn_freq_thresh"].get()
                 }

            # Composite layer configurations (if agent type is composite)
            layer_configs_for_this_run = None
            composite_signal_threshold_val = self._tk_vars["composite_signal_threshold"].get()
            if headless_agent_type_val == "composite":
                 layer_configs_for_this_run = self._get_current_layer_configs_for_composite()
                 if not layer_configs_for_this_run:
                     self.root.after(0, lambda: self._add_log_message("Headless Composite: no active layer configs. Using default layer."))
                     # Use aliased AGENT_CONFIG
                     layer_configs_for_this_run = [{
                         "agent_type_name": "stochastic", "num_instances": 1,
                         "input_time_scale": agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                         "output_time_scale": agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
                         "decay_timer_frames": agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"], # Default layer decay
                         "weight": 1.0, # Default layer weight
                         "agent_params": {}
                     }]


            for i in range(num_instances):
                if not self.headless_run_active: # Check the stop flag
                    self.root.after(0, lambda: self._add_log_message("Headless run aborted by user request."))
                    break

                instance_msg = f"Running instance {i+1}/{num_instances}...";
                # Update GUI status from the main thread
                self.root.after(0, lambda s=instance_msg: self.headless_status_var.set(f"Status: {s}"))

                # Configure the simulation world for this instance
                main_configure(
                    food_preset=self.food_sel.get(), # Use food preset from GUI
                    agent_type=headless_agent_type_val,
                    n_agents=self.headless_num_agents_var.get(), # Use headless specific agent count
                    spawn_point=(self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()), # Use spawn from GUI
                    initial_orientation_deg=self._tk_vars["initial_orientation_deg"].get(), # Use orientation from GUI
                    base_turn_deg_dt=self._tk_vars["base_turn_rate_dt"].get(), # Use base turn rate from GUI
                    input_time_scale=self._tk_vars["input_time_scale"].get(), # Use input time scale from GUI
                    output_time_scale=self._tk_vars["output_time_scale"].get(), # Use output time scale from GUI
                    decay_timer_frames=self._tk_vars["decay_timer_frames"].get(), # Use decay timer from GUI (applies to top-level)
                    composite_signal_threshold=composite_signal_threshold_val, # Use composite threshold from GUI
                    layer_configs_for_composite=layer_configs_for_this_run, # Use composite layer configs
                    agent_constant_speed=self._tk_vars["agent_constant_speed"].get(), # Use speed from GUI
                    agent_params_for_main_agent=agent_params_for_headless_main_agent # Use specific params if not composite
                )

                # Initialize world without a surface
                main_init_pygame_surface(None)

                # Run the simulation frames for this instance
                for frame_num in range(frames_per_instance):
                    main_run_frame(is_visual=False)
                    # Optional: update progress periodically
                    if frames_per_instance > 0 and frame_num % (max(1, frames_per_instance // 50)) == 0 :
                        progress_msg = f"Instance {i+1}: {frame_num*100//frames_per_instance}%";
                        self.root.after(0, lambda s=progress_msg: self.headless_status_var.set(f"Status: {s}"))

                # Collect stats for this instance
                stats = get_simulation_stats()
                food_eaten = stats.get("food_eaten_run", 0)
                efficiency = food_eaten / expiry_seconds if expiry_seconds > 0 else 0
                efficiency_scores.append(efficiency)

                self.root.after(0, lambda f=food_eaten, e=efficiency: self._add_log_message(f"Instance {i+1} done. Food: {f}, Eff: {e:.3f} F/s."))

            # --- Headless Runs Complete ---
            if efficiency_scores:
                avg_eff, med_eff = mean(efficiency_scores), median(efficiency_scores)
                self.root.after(0, lambda: self.headless_avg_eff_var.set(f"Avg. Efficiency: {avg_eff:.3f} F/s"))
                self.root.after(0, lambda: self.headless_median_eff_var.set(f"Median Efficiency: {med_eff:.3f} F/s"))
                self._generate_trace_file(efficiency_scores) # Offer to save results
                self.root.after(0, lambda: self._add_log_message(f"Headless runs complete. AvgEff: {avg_eff:.3f}, MedEff: {med_eff:.3f}"))
            else:
                self.root.after(0, lambda: self._add_log_message("No headless scores recorded."))
                self.root.after(0, lambda: self.headless_avg_eff_var.set("Avg. Eff: N/A"))
                self.root.after(0, lambda: self.headless_median_eff_var.set("Median Eff: N/A"))

            self.root.after(0, lambda: self.headless_status_var.set("Status: Idle"))

        except Exception as e:
            # Log any exceptions that occur during headless runs
            error_msg = f"Error during headless run: {e}"
            self.root.after(0, lambda: self._add_log_message(error_msg))
            import traceback
            self.root.after(0, lambda: self._add_log_message(traceback.format_exc()))
            self.root.after(0, lambda: self.headless_status_var.set("Status: Error"))

        finally:
            # Always re-enable the start button and clear the active flag
            self.root.after(0, lambda: self.start_headless_button.config(state="normal"))
            self.headless_run_active = False

    def _get_current_layer_configs_for_composite(self) -> Optional[List[Dict[str, Any]]]:
        """Gathers the composite layer configurations from the GUI."""
        active_layer_configs = []
        for block_data in self.composite_agent_configs_gui:
            # Ensure block_data and its tk_vars exist and frame is packed
            if not block_data or "tk_vars" not in block_data or not block_data["widgets"]["frame"].winfo_exists(): continue

            agent_type = block_data["tk_vars"]["type"].get()
            if agent_type == "None": continue # Skip inactive blocks

            # Collect parameters for this layer instance
            current_agent_params: Dict[str, Any] = {}
            # Get type-specific parameter value
            if agent_type == "inverse_turn":
                try:
                    # Retrieve the value from the appropriate Tk var type (IntVar for inverse_turn)
                    # The param_val key is added by _rebuild_param_slider
                    if "param_val" in block_data["tk_vars"]:
                         freq_thresh_val = block_data["tk_vars"]["param_val"].get()
                         current_agent_params["frequency_threshold"] = int(freq_thresh_val)
                    else:
                         # Fallback if param_val Tk var is somehow missing - Use aliased AGENT_CONFIG
                         current_agent_params["frequency_threshold"] = agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]
                         self._add_log_message(f"Warning: 'param_val' Tk var missing for composite layer '{agent_type}', using default frequency_threshold {current_agent_params['frequency_threshold']}.")

                except (tk.TclError, AttributeError, KeyError) as e:
                    # Fallback if retrieval fails - Use aliased AGENT_CONFIG
                    current_agent_params["frequency_threshold"] = agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]
                    self._add_log_message(f"Warning: Could not get frequency_threshold value for composite layer '{agent_type}' ({e}), using default {current_agent_params['frequency_threshold']}.")


            # Collect common layer parameters
            try:
                num_instances = block_data["tk_vars"]["instances"].get()
                input_ts = block_data["tk_vars"]["input_ts"].get()
                output_ts = block_data["tk_vars"]["output_ts"].get()
                decay_frames = block_data["tk_vars"]["decay_frames"].get() # Get layer's decay setting
                weight = block_data["tk_vars"]["weight"].get()             # Get layer's weight setting

            except (tk.TclError, AttributeError, KeyError) as e:
                 self._add_log_message(f"Error getting common params for composite layer '{agent_type}': {e}. Skipping block.")
                 continue # Skip this block if essential params can't be retrieved


            # Add the configured layer block to the list
            # Note: base_turn_rate_deg_dt is NOT configured per layer anymore.
            # The composite agent uses its own base_turn_rate_deg_dt (from the main tab)
            # and the layer weights/their own decayed outputs determine their relative influence.
            active_layer_configs.append({
                "agent_type_name": agent_type,
                "num_instances": num_instances,
                "input_time_scale": input_ts,
                "output_time_scale": output_ts,
                "decay_timer_frames": decay_frames, # Pass layer's decay setting
                "weight": weight,                   # Pass layer's weight setting
                "agent_params": current_agent_params,
            })

        return active_layer_configs if active_layer_configs else None

    def _generate_trace_file(self, scores: List[float]):
        """Prompts user to save headless run results to a trace file."""
        try:
            # Ask user for a file path to save the results
            filepath = filedialog.asksaveasfilename(
                defaultextension=".txt",
                filetypes=[("Text files", "*.txt"), ("All files", "*.*")],
                title="Save Headless Run Trace File"
            )
            if not filepath:
                self._add_log_message("Trace file saving cancelled by user."); return

            # Write simulation details and scores to the file
            with open(filepath, "w") as f:
                f.write(f"Headless Simulation Trace - {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Agent Type: {self.headless_agent_type_var.get()}\n")
                f.write(f"Main Agent Specific Params: {self._get_main_agent_specific_params()}\n") # Include main agent params
                if self.headless_agent_type_var.get() == "composite":
                     comp_config = {
                         "threshold": self._tk_vars["composite_signal_threshold"].get(),
                         "layers": self._get_current_layer_configs_for_composite()
                     }
                     # Convert layer configs for printing (handles lists of dicts)
                     import json
                     f.write("Composite Config:\n")
                     f.write(json.dumps(comp_config, indent=2))
                     f.write("\n")

                f.write(f"# Agents/Instance: {self.headless_num_agents_var.get()}\n")
                f.write(f"Expire Timer/Instance (s): {self.headless_expiry_timer_var.get()}\n")
                f.write(f"Target FPS: {self._tk_vars['fps'].get()} (FRAME_DT: {FRAME_DT:.6f}s)\n")
                f.write(f"Overall Decay Max Frames: {self._tk_vars['decay_timer_frames'].get()}\n") # Include global decay
                f.write(f"Agent Speed (px/s): {self._tk_vars['agent_constant_speed'].get()}\n") # Include speed
                f.write(f"Agent Max Turn Rate (°/frame): {self._tk_vars['base_turn_rate_dt'].get()}\n") # Include max turn
                f.write("--- Efficiency Scores (Food/Second) ---\n")
                for i, score in enumerate(scores): f.write(f"Instance {i+1}: {score:.4f}\n")
                f.write("---\n")
                f.write(f"Average Efficiency: {mean(scores):.4f} F/s\n")
                f.write(f"Median Efficiency: {median(scores):.4f} F/s\n")

            self._add_log_message(f"Trace file saved to {filepath}")

        except Exception as e:
            self._add_log_message(f"Error saving trace file: {e}")
            messagebox.showerror("File Save Error", f"Could not save trace file:\n{e}")

    def _get_main_agent_specific_params(self) -> Dict[str, Any]:
         """Helper to get specific params for the currently selected main agent type."""
         agent_type = self.agent_type_var.get() # Use the current main agent type
         if agent_type == "inverse_turn":
             # Use aliased AGENT_CONFIG for fallback if TK var is somehow missing
             freq_thresh = self._tk_vars["inverse_turn_freq_thresh"].get() if "inverse_turn_freq_thresh" in self._tk_vars else agent_module_AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]
             return {"frequency_threshold": freq_thresh}
         # Add conditions for other agent types with specific params here
         return {} # Return empty dict if no specific params


# Main execution block
if __name__ == "__main__":
    try:
        app = SimGUI()
        app.root.mainloop() # Start the Tkinter event loop
    except ImportError:
        # Handle cases where GUI dependencies (like tkinter) might be missing
        print("Error: Could not import SimGUI. Tkinter or other GUI dependencies might be missing.")
        print("Run the application using the GUI.py script directly or ensure dependencies are installed.")
        sys.exit(1) # Exit if GUI fails to start
    except Exception as e:
        # Catch any other unexpected errors
        import traceback
        print(f"An unexpected error occurred: {e}")
        traceback.print_exc()
        sys.exit(1) # Exit on unexpected error
