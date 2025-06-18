from __future__ import annotations
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import pygame
from typing import Dict, List, Any, Sequence, Optional
import time
from statistics import mean, median
import math
import json

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
        self.root.title("Agent Sim - Dual Signal Control")
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
            "fps": VarI(int(1 / FRAME_DT if FRAME_DT > 0 else 60)), "n_agents": VarI(1),
            "spawn_x": VarD(cfg.WINDOW_W / 2), "spawn_y": VarD(cfg.WINDOW_H / 2),
            "initial_orientation_deg": VarD(0.0), "agent_constant_speed": VarD(cfg.AGENT_CONSTANT_SPEED),
            "input_time_scale": VarI(agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"]),
            "output_time_scale": VarI(agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]),
            "composite_signal_threshold": VarD(agent_module_AGENT_CONFIG["DEFAULT_COMPOSITE_SIGNAL_THRESHOLD"]),
            # New specific params for top-level agent
            "inv_turn_left_thresh": VarI(agent_module_AGENT_CONFIG["DEFAULT_LEFT_THRESHOLD"]),
            "inv_turn_right_thresh": VarI(agent_module_AGENT_CONFIG["DEFAULT_RIGHT_THRESHOLD"]),
            "stoch_left_rand_freq": VarD(agent_module_AGENT_CONFIG["DEFAULT_STOCHASTIC_FREQ"]),
            "stoch_right_rand_freq": VarD(agent_module_AGENT_CONFIG["DEFAULT_STOCHASTIC_FREQ"]),
        }

        self.composite_agent_configs_gui: List[Dict[str, Any]] = []
        self.composite_config_canvas_frame: Optional[ttk.Frame] = None
        self.specific_agent_params_frame: Optional[ttk.LabelFrame] = None
        self.main_base_turn_rate_slider_frame: Optional[ttk.Frame] = None

        self.headless_agent_type_var = tk.StringVar(); self.headless_agent_type_var.set(list(main_agent_classes.keys())[0] if main_agent_classes else "")
        self.headless_expiry_timer_var = VarI(30); self.headless_num_agents_var = VarI(10); self.headless_num_instances_var = VarI(5)
        self.headless_avg_eff_var = tk.StringVar(value="Avg. Efficiency: N/A"); self.headless_median_eff_var = tk.StringVar(value="Median Efficiency: N/A")
        self.headless_status_var = tk.StringVar(value="Status: Idle")

        self._build_status_tab(self.status_tab)
        self._build_controls_tab(self.controls_tab)
        self._build_composite_config_tab(self.composite_config_tab)
        self._build_headless_tab(self.headless_tab)

        self.running_simulation = False; self.sim_thread = None; self.pygame_surface = None
        self.simulation_start_time_visual = None; self.headless_run_active = False
        self.root.protocol("WM_DELETE_WINDOW", self.quit_application)
        self._periodic_status_update()
        self._on_agent_type_change()

    def _add_log_message(self, message: str):
        if hasattr(self, 'log_text') and self.log_text.winfo_exists():
            self.log_text.config(state="normal")
            self.log_text.insert(tk.END, f"{time.strftime('%H:%M:%S')} - {message}\n")
            self.log_text.see(tk.END); self.log_text.config(state="disabled")

    def quit_application(self):
        self._add_log_message("Quit application requested.")
        self.headless_run_active = False; self.stop_simulation()
        if self.root.winfo_exists(): self.root.quit(); self.root.destroy()

    def _add_slider_entry(self, parent, tk_var, label, from_, to, res=None, width=28, entry_w=None):
        f = ttk.Frame(parent); f.pack(fill="x", pady=2)
        ttk.Label(f, text=label, width=width).pack(side="left")
        if res is None: res = 0.01 if isinstance(tk_var, tk.DoubleVar) else 1
        s = ttk.Scale(f, variable=tk_var, from_=from_, to=to, orient="horizontal")
        try: s.configure(resolution=res)
        except tk.TclError: pass
        s.pack(side="left", fill="x", expand=True, padx=5)
        entry_width = entry_w if entry_w is not None else (7 if isinstance(tk_var, tk.DoubleVar) else 5)
        ttk.Entry(f, textvariable=tk_var, width=entry_width).pack(side="left")
        return f

    def _build_controls_tab(self, parent_tab):
        main = ttk.Frame(parent_tab, padding=10); main.pack(fill="both", expand=True)
        timing_frame = ttk.LabelFrame(main, text="Agent Timing (Top Level)")
        timing_frame.pack(fill="x", pady=5)
        self._add_slider_entry(timing_frame, self._tk_vars["output_time_scale"], "Output Time Scale (frames)\n(Decision Frequency)", 5, 200, width=28)

        motion_frame = ttk.LabelFrame(main, text="Agent Motion & Simulation Timing")
        motion_frame.pack(fill="x", pady=5)
        self._add_slider_entry(motion_frame, self._tk_vars["base_turn_rate_dt"], "Base Turn Rate (°/frame)\n(Agent Turn Amount)", 0.0, 45.0, res=0.1, width=28)
        self._add_slider_entry(motion_frame, self._tk_vars["agent_constant_speed"], "Agent Constant Speed (px/s)", 10, 300, res=1.0)
        self._add_slider_entry(motion_frame, self._tk_vars["fps"], "Target FPS (sets ∆t)", 10, 240, res=1.0)

        world_frame = ttk.LabelFrame(main, text="World & Agent Setup")
        world_frame.pack(fill="x", pady=5)
        f = ttk.Frame(world_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Agent Type (Visual Sim)", width=28).pack(side="left")
        self.agent_type_var = tk.StringVar()
        self.agent_sel = ttk.Combobox(f, values=list(main_agent_classes.keys()), state="readonly", width=18, textvariable=self.agent_type_var)
        if list(main_agent_classes.keys()): self.agent_type_var.set(list(main_agent_classes.keys())[0])
        self.agent_sel.pack(side="left", fill="x", expand=True, padx=5)
        self.agent_type_var.trace_add("write", self._on_agent_type_change)

        self.specific_agent_params_frame = ttk.LabelFrame(main, text="Specific Agent Parameters")

        f = ttk.Frame(world_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="# Agents", width=28).pack(side="left")
        ttk.Spinbox(f, from_=1, to=150, textvariable=self._tk_vars["n_agents"], width=10).pack(side="left", fill="x", expand=True, padx=5)
        self._add_slider_entry(world_frame, self._tk_vars["spawn_x"], "Spawn X", 0, cfg.WINDOW_W, res=1.0)
        self._add_slider_entry(world_frame, self._tk_vars["spawn_y"], "Spawn Y", 0, cfg.WINDOW_H, res=1.0)
        self._add_slider_entry(world_frame, self._tk_vars["initial_orientation_deg"], "Initial Orientation (°)", 0, 359, res=1.0)
        f = ttk.Frame(world_frame); f.pack(fill="x", pady=2)
        ttk.Label(f, text="Food Preset", width=28).pack(side="left")
        self.food_sel = ttk.Combobox(f, values=list(cfg.FOOD_PRESETS.keys()), state="readonly", width=18)
        self.food_sel.current(0); self.food_sel.pack(side="left", fill="x", expand=True, padx=5)

        btns = ttk.Frame(main); btns.pack(fill="x", pady=10, side="bottom")
        self.start_button = ttk.Button(btns, text="Start Visual Sim", command=self.start_simulation)
        self.start_button.pack(side="left", expand=True, fill="x", padx=5)
        self.stop_button = ttk.Button(btns, text="Stop Visual Sim", command=self.stop_simulation, state="disabled")
        self.stop_button.pack(side="left", expand=True, fill="x", padx=5)
        ttk.Button(btns, text="Quit App", command=self.quit_application).pack(side="left", expand=True, fill="x", padx=5)

    def _on_agent_type_change(self, *args):
        selected_type = self.agent_type_var.get()
        try:
            comp_tab_idx = self.notebook.tabs().index(self.composite_config_tab._w)
            self.notebook.tab(comp_tab_idx, state="normal" if selected_type == "composite" else "disabled")
        except (ValueError, tk.TclError): pass

        if self.specific_agent_params_frame:
            for widget in self.specific_agent_params_frame.winfo_children(): widget.destroy()
            
            if selected_type == "inverse_turn":
                self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["inv_turn_left_thresh"], "Left Freq. Threshold", 1, 100, res=1)
                self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["inv_turn_right_thresh"], "Right Freq. Threshold", 1, 100, res=1)
                self.specific_agent_params_frame.pack(fill="x", pady=(5,0), after=self.agent_sel.master.master)
            elif selected_type == "stochastic":
                self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["stoch_left_rand_freq"], "Left Rand. Freq (prob/fr)", 0.0, 1.0, res=0.01)
                self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["stoch_right_rand_freq"], "Right Rand. Freq (prob/fr)", 0.0, 1.0, res=0.01)
                self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["inv_turn_left_thresh"], "Left Freq. Threshold", 1, 100, res=1)
                self._add_slider_entry(self.specific_agent_params_frame, self._tk_vars["inv_turn_right_thresh"], "Right Freq. Threshold", 1, 100, res=1)
                self.specific_agent_params_frame.pack(fill="x", pady=(5,0), after=self.agent_sel.master.master)
            else:
                self.specific_agent_params_frame.pack_forget()

    def _build_composite_config_tab(self, parent_tab):
        # This function and its helpers would need significant rework to support the new parameter model for each layer.
        # As per the request, the composite agent's adjustment is deferred.
        # For now, we'll leave a placeholder message.
        ttk.Label(parent_tab, text="Composite agent configuration has been deferred in this version.", padding=20).pack()

    def _build_headless_tab(self, parent_tab):
        # This function also depends on the agent parameter model.
        # It's simplified here as the full composite config is deferred.
        main = ttk.Frame(parent_tab, padding=10); main.pack(fill="both", expand=True)
        conf = ttk.LabelFrame(main, text="Headless Instance Configuration"); conf.pack(fill="x", pady=5, padx=5)
        f = ttk.Frame(conf); f.pack(fill="x", pady=3); ttk.Label(f, text="Agent Type:", width=20).pack(side="left")
        ttk.Combobox(f, values=list(main_agent_classes.keys()), state="readonly", width=20, textvariable=self.headless_agent_type_var).pack(side="left", fill="x", expand=True, padx=5)
        f = ttk.Frame(conf); f.pack(fill="x", pady=3); ttk.Label(f, text="Expire Timer (s):", width=20).pack(side="left")
        ttk.Spinbox(f, from_=5, to=3600, textvariable=self.headless_expiry_timer_var, width=10).pack(side="left", padx=5)
        f = ttk.Frame(conf); f.pack(fill="x", pady=3); ttk.Label(f, text="# Agents/Instance:", width=20).pack(side="left")
        ttk.Spinbox(f, from_=1, to=200, textvariable=self.headless_num_agents_var, width=10).pack(side="left", padx=5)
        f = ttk.Frame(conf); f.pack(fill="x", pady=3); ttk.Label(f, text="# Instances:", width=20).pack(side="left")
        ttk.Spinbox(f, from_=1, to=1000, textvariable=self.headless_num_instances_var, width=10).pack(side="left", padx=5)
        self.start_headless_button = ttk.Button(main, text="Start Headless Runs", command=self.start_headless_runs)
        self.start_headless_button.pack(fill="x", pady=10)
        res = ttk.LabelFrame(main, text="Results"); res.pack(fill="x", pady=5, padx=5)
        ttk.Label(res, textvariable=self.headless_status_var).pack(anchor="w", pady=2)
        ttk.Label(res, textvariable=self.headless_avg_eff_var).pack(anchor="w", pady=2)
        ttk.Label(res, textvariable=self.headless_median_eff_var).pack(anchor="w", pady=2)

    def _build_status_tab(self, parent_tab):
        stat = ttk.LabelFrame(parent_tab, text="Live Simulation Status", padding=10); stat.pack(fill="both", expand=True)
        self.sim_status_var = tk.StringVar(value="Simulation: Idle"); ttk.Label(stat, textvariable=self.sim_status_var, font=("Arial", 12)).pack(anchor="w", pady=5)
        self.total_food_var = tk.StringVar(value="Total Food: N/A"); ttk.Label(stat, textvariable=self.total_food_var).pack(anchor="w", pady=2)
        self.agents_active_var = tk.StringVar(value="Active Agents: N/A"); ttk.Label(stat, textvariable=self.agents_active_var).pack(anchor="w", pady=2)
        self.food_eaten_var = tk.StringVar(value="Food Eaten: N/A"); ttk.Label(stat, textvariable=self.food_eaten_var).pack(anchor="w", pady=2)
        self.efficiency_var = tk.StringVar(value="Efficiency (F/s): N/A"); ttk.Label(stat, textvariable=self.efficiency_var).pack(anchor="w", pady=2)
        log_f = ttk.LabelFrame(stat, text="Event Log"); log_f.pack(fill="both", expand=True, pady=10)
        self.log_text = tk.Text(log_f, height=8, state="disabled", wrap="word")
        scroll = ttk.Scrollbar(log_f, command=self.log_text.yview); self.log_text.config(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y"); self.log_text.pack(side="left", fill="both", expand=True)
        self._add_log_message("GUI Initialized.")

    def _periodic_status_update(self):
        if not self.root.winfo_exists(): return
        if self.running_simulation and not self.headless_run_active:
            stats = get_simulation_stats()
            self.total_food_var.set(f"Total Food: {stats.get('total_food', 'N/A')}")
            self.agents_active_var.set(f"Active Agents: {stats.get('active_agents', 'N/A')}")
            self.food_eaten_var.set(f"Food Eaten: {stats.get('food_eaten_run', 'N/A')}")
            if self.simulation_start_time_visual:
                elapsed = time.time() - self.simulation_start_time_visual
                eff = stats.get('food_eaten_run', 0) / elapsed if elapsed > 0.01 else 0.0
                self.efficiency_var.set(f"Efficiency (F/s): {eff:.2f}")
        self.root.after(1000, self._periodic_status_update)

    def _apply_settings(self, for_headless=False) -> bool:
        self._add_log_message("Applying settings...")
        try:
            set_frame_dt(1.0 / self._tk_vars["fps"].get())
            cfg.AGENT_CONSTANT_SPEED = self._tk_vars["agent_constant_speed"].get()
            
            agent_type = self.agent_type_var.get() if not for_headless else self.headless_agent_type_var.get()
            params = {}
            if agent_type == "inverse_turn":
                params['left_threshold'] = self._tk_vars["inv_turn_left_thresh"].get()
                params['right_threshold'] = self._tk_vars["inv_turn_right_thresh"].get()
            elif agent_type == "stochastic":
                params['left_rand_freq'] = self._tk_vars["stoch_left_rand_freq"].get()
                params['right_rand_freq'] = self._tk_vars["stoch_right_rand_freq"].get()
                params['left_threshold'] = self._tk_vars["inv_turn_left_thresh"].get()
                params['right_threshold'] = self._tk_vars["inv_turn_right_thresh"].get()

            main_configure(
                food_preset=self.food_sel.get(), agent_type=agent_type,
                n_agents=int(self._tk_vars["n_agents"].get()) if not for_headless else self.headless_num_agents_var.get(),
                spawn_point=(self._tk_vars["spawn_x"].get(), self._tk_vars["spawn_y"].get()),
                initial_orientation_deg=self._tk_vars["initial_orientation_deg"].get(),
                base_turn_deg_dt=self._tk_vars["base_turn_rate_dt"].get(),
                input_time_scale=self._tk_vars["input_time_scale"].get(),
                output_time_scale=self._tk_vars["output_time_scale"].get(),
                composite_signal_threshold=self._tk_vars["composite_signal_threshold"].get(),
                layer_configs_for_composite=None, # Deferred
                agent_constant_speed=cfg.AGENT_CONSTANT_SPEED,
                agent_params_for_main_agent=params
            )
            self._add_log_message(f"World configured for agent: {agent_type}")
            return True
        except Exception as e:
            self._add_log_message(f"Config Error: {e}"); messagebox.showerror("Config Error", f"Failed to apply settings:\n{e}")
            return False

    def start_simulation(self):
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run active."); return
        if self.running_simulation: self.stop_simulation()
        if not self._apply_settings(): return
        self.running_simulation = True; self.simulation_start_time_visual = time.time()
        self.start_button.config(state="disabled"); self.stop_button.config(state="normal")
        self.sim_status_var.set("Simulation: Starting...")
        self.sim_thread = threading.Thread(target=self._simulation_loop_thread, daemon=True)
        self.sim_thread.start()

    def stop_simulation(self):
        if not self.running_simulation: return
        self.running_simulation = False
        if self.sim_thread and self.sim_thread.is_alive(): self.sim_thread.join(timeout=1.0)
        self.sim_thread = None; self.simulation_start_time_visual = None
        self.start_button.config(state="normal"); self.stop_button.config(state="disabled")
        self.sim_status_var.set("Simulation: Idle")

    def _simulation_loop_thread(self):
        pygame.init()
        try:
            self.pygame_surface = pygame.display.set_mode((cfg.WINDOW_W, cfg.WINDOW_H))
            pygame.display.set_caption("Agent Sim")
            self.root.after(0, lambda: self.sim_status_var.set("Simulation: Running"))
        except pygame.error as e:
            self.root.after(0, lambda: messagebox.showerror("Pygame Error", str(e)))
            if pygame.get_init(): pygame.quit(); self.running_simulation = False
            return
        main_init_pygame_surface(self.pygame_surface)
        clock = pygame.time.Clock()
        while self.running_simulation:
            for event in pygame.event.get():
                if event.type == pygame.QUIT: self.running_simulation = False; break
            if not self.running_simulation: break
            main_run_frame(is_visual=True)
            pygame.display.flip()
            try: clock.tick(self._tk_vars["fps"].get())
            except tk.TclError: self.running_simulation = False
        if pygame.get_init(): pygame.quit()
        self.pygame_surface = None
        self.root.after(0, self.stop_simulation)

    def start_headless_runs(self):
        if self.running_simulation: messagebox.showwarning("Busy", "Visual sim active."); return
        if self.headless_run_active: messagebox.showwarning("Busy", "Headless run in progress."); return
        
        # Temporarily set main GUI values to headless values for configuration
        original_n_agents = self._tk_vars["n_agents"].get(); original_agent_type = self.agent_type_var.get()
        self._tk_vars["n_agents"].set(self.headless_num_agents_var.get()); self.agent_type_var.set(self.headless_agent_type_var.get())
        apply_ok = self._apply_settings(for_headless=True)
        self._tk_vars["n_agents"].set(original_n_agents); self.agent_type_var.set(original_agent_type)

        if not apply_ok: self.headless_status_var.set("Status: Config Error"); return
        self.headless_run_active = True; self.start_headless_button.config(state="disabled")
        self.headless_status_var.set("Status: Starting...")
        threading.Thread(target=self._headless_run_worker_thread, daemon=True).start()

    def _headless_run_worker_thread(self):
        try:
            num_instances = self.headless_num_instances_var.get()
            expiry_seconds = self.headless_expiry_timer_var.get()
            frames_per_instance = math.ceil(expiry_seconds / FRAME_DT) if FRAME_DT > 1e-9 else 0
            self.root.after(0, lambda: self._add_log_message(f"Headless: {num_instances} runs, {expiry_seconds}s each."))
            
            scores = []
            for i in range(num_instances):
                if not self.headless_run_active: self.root.after(0, lambda: self._add_log_message("Headless run aborted.")); break
                self.root.after(0, lambda i=i: self.headless_status_var.set(f"Status: Running instance {i+1}/{num_instances}"))
                self._apply_settings(for_headless=True) # Re-configure for each run
                main_init_pygame_surface(None)
                for _ in range(frames_per_instance): main_run_frame(is_visual=False)
                stats = get_simulation_stats()
                eff = stats.get("food_eaten_run", 0) / expiry_seconds if expiry_seconds > 0 else 0
                scores.append(eff)
                self.root.after(0, lambda f=stats.get("food_eaten_run",0), e=eff: self._add_log_message(f"Instance {i+1} done. Food: {f}, Eff: {e:.3f}"))
            
            if scores:
                avg, med = mean(scores), median(scores)
                self.root.after(0, lambda: self.headless_avg_eff_var.set(f"Avg. Efficiency: {avg:.3f} F/s"))
                self.root.after(0, lambda: self.headless_median_eff_var.set(f"Median Efficiency: {med:.3f} F/s"))
                self._generate_trace_file(scores)
        except Exception as e:
            self.root.after(0, lambda: self._add_log_message(f"Error in headless run: {e}"))
        finally:
            self.root.after(0, lambda: self.start_headless_button.config(state="normal"))
            self.root.after(0, lambda: self.headless_status_var.set("Status: Idle"))
            self.headless_run_active = False

    def _generate_trace_file(self, scores: List[float]):
        filepath = filedialog.asksaveasfilename(defaultextension=".txt", filetypes=[("Text files", "*.txt")], title="Save Trace File")
        if not filepath: self._add_log_message("Trace file saving cancelled."); return
        with open(filepath, "w") as f:
            f.write(f"Headless Simulation Trace - {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Agent Type: {self.headless_agent_type_var.get()}\n")
            # This part needs to be updated to fetch the correct parameters
            params = self._get_main_agent_specific_params(headless=True)
            f.write(f"Agent Params: {json.dumps(params)}\n")
            f.write(f"# Agents/Instance: {self.headless_num_agents_var.get()}\n")
            f.write(f"Duration/Instance (s): {self.headless_expiry_timer_var.get()}\n")
            f.write("---\nScores (Food/Second):\n" + "\n".join(f"Instance {i+1}: {s:.4f}" for i, s in enumerate(scores)) + "\n")
            f.write(f"---\nAverage: {mean(scores):.4f}\nMedian: {median(scores):.4f}\n")
        self._add_log_message(f"Trace file saved to {filepath}")
    
    def _get_main_agent_specific_params(self, headless=False) -> Dict[str, Any]:
         agent_type = self.agent_type_var.get() if not headless else self.headless_agent_type_var.get()
         params = {}
         if agent_type == "inverse_turn":
             params['left_threshold'] = self._tk_vars["inv_turn_left_thresh"].get()
             params['right_threshold'] = self._tk_vars["inv_turn_right_thresh"].get()
         elif agent_type == "stochastic":
             params['left_rand_freq'] = self._tk_vars["stoch_left_rand_freq"].get()
             params['right_rand_freq'] = self._tk_vars["stoch_right_rand_freq"].get()
             params['left_threshold'] = self._tk_vars["inv_turn_left_thresh"].get()
             params['right_threshold'] = self._tk_vars["inv_turn_right_thresh"].get()
         return params

if __name__ == "__main__":
    try: app = SimGUI(); app.root.mainloop()
    except Exception as e: print(f"An error occurred: {e}"); import traceback; traceback.print_exc(); sys.exit(1)