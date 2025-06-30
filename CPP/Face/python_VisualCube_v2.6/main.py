from __future__ import annotations
import pygame
import sys
import math
import random
from typing import List, Tuple, Dict, Type, Sequence, Any, Optional, TextIO
from collections import deque

from agent_module import (
    stochastic_motion_random_movement_agent,
    inverse_motion_turning_agent,
    composite_agent,
    _BaseAgent,
    AGENT_CONFIG as agent_module_AGENT_CONFIG,
)
from math_module import ( RowboatKinematics, FRAME_DT, set_frame_dt )
import config as cfg # Static configs like WINDOW_W, WINDOW_H, radii

# Dictionary mapping agent class names to their constructors
AGENT_CLASSES: Dict[str, Type[_BaseAgent] | Type[composite_agent]] = {
    "stochastic": stochastic_motion_random_movement_agent,
    "inverse_turn": inverse_motion_turning_agent,
    "composite": composite_agent,
}

# --- Simulation Data Structures ---

class Food:
    """Represents a single food item."""
    __slots__ = ("x", "y", "r")
    def __init__(self, pos: Tuple[float, float]): self.x, self.y = pos; self.r = cfg.FOOD_RADIUS
    def draw(self, surf: pygame.Surface):
        pygame.draw.circle(surf, (0, 255, 0), (int(self.x), int(self.y)), self.r)

class SimAgent:
    """Represents a single agent in the simulation."""
    __slots__ = ("ctrl", "body", "color", "agent_type_name", "trace")

    def __init__(self, cls_name: str, pos: Tuple[float, float],
                 input_time_scale: int, output_time_scale: int,
                 composite_signal_threshold: float,
                 *,
                 initial_direction_deg: float = 0.0,
                 agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
                 layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None,
                 agent_params: Optional[Dict[str, Any]] = None
                ):

        self.agent_type_name = cls_name
        self.trace = deque(maxlen=500) # For drawing agent's recent path
        
        controller_params = agent_params if agent_params is not None else {}
        controller_params.update({
            "input_time_scale": input_time_scale,
            "output_time_scale": output_time_scale,
        })

        if cls_name == "composite":
            controller_params["composite_signal_threshold"] = composite_signal_threshold
            controller_params["layer_configs"] = layer_configs_for_composite

        self.ctrl: _BaseAgent | composite_agent = self._create_controller(
            cls_name=cls_name,
            params=controller_params
        )

        self.body = RowboatKinematics(
            pos=pos,
            theta_deg=initial_direction_deg,
            lin_coeff=agent_constant_speed,
            beam_w=cfg.AGENT_RADIUS * 2.0
        )

        self.color = (random.randint(100, 255), random.randint(50, 150), random.randint(50, 150))

    def _create_controller(self, cls_name: str, params: Dict[str, Any]) -> _BaseAgent | composite_agent:
        """Helper to instantiate the correct controller class."""
        agent_class_constructor = AGENT_CLASSES.get(cls_name)

        if agent_class_constructor is None:
             print(f"Warning: Unknown agent type '{cls_name}'. Falling back to stochastic.")
             agent_class_constructor = stochastic_motion_random_movement_agent
             cls_name = "stochastic"

        if cls_name == "composite" and (not params.get("layer_configs")):
             print("Info: Composite agent configured with no layers. Adding default stochastic layer.")
             params["layer_configs"] = [{
                 "agent_type_name": "stochastic", "num_instances": 1, "weight": 1.0,
                 "output_time_scale": agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
                 "agent_params": {} 
             }]
        
        try:
            return agent_class_constructor(**params)
        except Exception as e:
            print(f"Error creating controller for '{cls_name}' with params {params}: {e}")
            import traceback; traceback.print_exc()
            print("Falling back to default stochastic agent.")
            return stochastic_motion_random_movement_agent(
                input_time_scale=agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                output_time_scale=agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]
            )

    def step(self, food_signal: int):
        f_left, f_right = self.ctrl.update(
            food_signal, 
            _total_frames_simulated_current_run, 
            FRAME_DT
        )
        self.body.update(f_left_hz=f_left, f_right_hz=f_right)
        self._wrap_position()
        self.trace.append((int(self.body.position.x), int(self.body.position.y)))


    def _wrap_position(self):
        if self.body.position.x < -cfg.AGENT_RADIUS: self.body.position.x = cfg.WINDOW_W + cfg.AGENT_RADIUS
        elif self.body.position.x > cfg.WINDOW_W + cfg.AGENT_RADIUS: self.body.position.x = -cfg.AGENT_RADIUS
        if self.body.position.y < -cfg.AGENT_RADIUS: self.body.position.y = cfg.WINDOW_H + cfg.AGENT_RADIUS
        elif self.body.position.y > cfg.WINDOW_H + cfg.AGENT_RADIUS: self.body.position.y = -cfg.AGENT_RADIUS

    def draw(self, surf: pygame.Surface):
        pos_x, pos_y = int(self.body.position.x), int(self.body.position.y)
        pygame.draw.circle(surf, self.color, (pos_x, pos_y), cfg.AGENT_RADIUS)
        line_len = cfg.AGENT_RADIUS * 1.8
        end_x = pos_x + line_len * math.cos(self.body.direction_rad)
        end_y = pos_y + line_len * math.sin(self.body.direction_rad)
        pygame.draw.line(surf, (200, 200, 255), (pos_x, pos_y), (int(end_x), int(end_y)), 2)

# --- World and Simulation State ---
WORLD = None
_food_eaten_current_run = 0
_total_frames_simulated_current_run = 0
_CURRENT_LOG_FILE: Optional[TextIO] = None

class World:
    def __init__(self):
        self.food: List[Food] = []
        self.agents: List[SimAgent] = []
        self.surface: pygame.Surface | None = None
        self.draw_trace_enabled: bool = False

    def reset(self, *, food_preset: str = "three_lines_dense",
                  agent_type: str = "stochastic", n_agents: int = 1,
                  spawn_point: Tuple[float, float] | None = None,
                  initial_direction_deg: float,
                  input_time_scale: int, output_time_scale: int,
                  composite_signal_threshold: float,
                  layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None,
                  agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
                  agent_params: Optional[Dict[str, Any]] = None,
                  draw_trace: bool = False
                  ):
        global _food_eaten_current_run, _total_frames_simulated_current_run
        _food_eaten_current_run = 0
        _total_frames_simulated_current_run = 0
        self.draw_trace_enabled = draw_trace

        food_coords_generator = cfg.FOOD_PRESETS.get(food_preset)
        self.food = [Food(p) for p in (food_coords_generator() if food_coords_generator else cfg.FOOD_PRESETS["void"]())]

        self.agents = []
        base_spawn_x, base_spawn_y = spawn_point if spawn_point else (cfg.WINDOW_W / 2, cfg.WINDOW_H / 2)

        for i in range(n_agents):
            offset_x = (i - (n_agents - 1) / 2) * (cfg.AGENT_RADIUS * 5) if n_agents > 1 else 0
            current_spawn_x = max(cfg.AGENT_RADIUS, min(base_spawn_x + offset_x, cfg.WINDOW_W - cfg.AGENT_RADIUS))
            current_spawn_y = max(cfg.AGENT_RADIUS, min(base_spawn_y, cfg.WINDOW_H - cfg.AGENT_RADIUS))

            agent = SimAgent(
                cls_name=agent_type,
                pos=(current_spawn_x, current_spawn_y),
                input_time_scale=input_time_scale,
                output_time_scale=output_time_scale,
                composite_signal_threshold=composite_signal_threshold,
                initial_direction_deg=initial_direction_deg,
                agent_constant_speed=agent_constant_speed,
                layer_configs_for_composite=layer_configs_for_composite,
                agent_params=agent_params
            )
            # Per requirement, give each agent a single food input at the start.
            agent.ctrl.update(1, 0, FRAME_DT) 
            self.agents.append(agent)

    def step(self):
        global _food_eaten_current_run, _total_frames_simulated_current_run
        _total_frames_simulated_current_run += 1

        for ag in self.agents:
            food_eaten_this_step = 0
            for f_idx in range(len(self.food) - 1, -1, -1):
                f = self.food[f_idx]
                dx = ag.body.position.x - f.x; dy = ag.body.position.y - f.y
                if dx**2 + dy**2 <= (cfg.FOOD_RADIUS + cfg.AGENT_RADIUS)**2:
                    self.food.pop(f_idx)
                    food_eaten_this_step = 1
                    _food_eaten_current_run += 1
                    break
            ag.step(food_signal=food_eaten_this_step)

    def draw(self, surf: pygame.Surface):
        surf.fill((30, 30, 30))
        for f in self.food: f.draw(surf)
        for ag in self.agents: ag.draw(surf)
        
        if self.draw_trace_enabled:
            for ag in self.agents:
                if len(ag.trace) > 1:
                    pygame.draw.aalines(surf, (255, 255, 0), False, list(ag.trace))


# --- Simulation Control Functions ---
WORLD = World()

def init_pygame_surface(surface: pygame.Surface | None):
    if WORLD: WORLD.surface = surface

def configure(*, food_preset: str, agent_type: str, n_agents: int,
              spawn_point: Tuple[float, float], initial_orientation_deg: float,
              input_time_scale: int, output_time_scale: int,
              composite_signal_threshold: float,
              layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None,
              agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
              agent_params_for_main_agent: Optional[Dict[str, Any]] = None,
              draw_trace: bool = False,
              log_file_handle: Optional[TextIO] = None
              ):
    global _CURRENT_LOG_FILE
    _CURRENT_LOG_FILE = log_file_handle
    if WORLD:
        WORLD.reset(
            food_preset=food_preset, agent_type=agent_type, n_agents=n_agents,
            spawn_point=spawn_point, initial_direction_deg=initial_orientation_deg,
            input_time_scale=input_time_scale, output_time_scale=output_time_scale,
            composite_signal_threshold=composite_signal_threshold,
            layer_configs_for_composite=layer_configs_for_composite,
            agent_constant_speed=agent_constant_speed, agent_params=agent_params_for_main_agent,
            draw_trace=draw_trace
        )

def run_frame(is_visual: bool = True):
    global _CURRENT_LOG_FILE  # <<< FIX: Moved to the top of the function
    if WORLD:
        WORLD.step()

        if _CURRENT_LOG_FILE and not _CURRENT_LOG_FILE.closed:
            frame_num = _total_frames_simulated_current_run
            for i, agent in enumerate(WORLD.agents):
                pos = agent.body.position
                log_line = f"{frame_num},{i},{pos.x:.2f},{pos.y:.2f},{agent.body.theta:.4f}\n"
                try:
                    _CURRENT_LOG_FILE.write(log_line)
                except Exception as e:
                    print(f"Error writing to log file: {e}")
                    # Now we can safely modify the global variable
                    _CURRENT_LOG_FILE = None # Stop trying to write

        if is_visual and WORLD.surface is not None: 
            WORLD.draw(WORLD.surface)

def get_simulation_stats() -> dict:
    stats = {}
    if WORLD:
        stats = {
            "total_food": len(WORLD.food), "active_agents": len(WORLD.agents),
            "food_eaten_run": _food_eaten_current_run,
            "total_frames_simulated_run": _total_frames_simulated_current_run,
            "current_frame_dt": FRAME_DT,
            "agent_type": WORLD.agents[0].agent_type_name if WORLD.agents else 'N/A'
        }
        # If there's an inverse agent, expose its calibration data
        if WORLD.agents and isinstance(WORLD.agents[0].ctrl, inverse_motion_turning_agent):
            inv_agent_ctrl = WORLD.agents[0].ctrl
            stats["inv_agent_calibrated"] = inv_agent_ctrl.is_calibrated
            stats["inv_agent_threshold_hz"] = inv_agent_ctrl.freq_threshold_hz
    else:
        stats = {k: "N/A" for k in ["total_food", "active_agents", "food_eaten_run", "total_frames_simulated_run", "current_frame_dt", "agent_type"]}
    
    return stats


if __name__ == "__main__":
    try:
        from GUI import SimGUI
        app = SimGUI(); app.root.mainloop()
    except ImportError:
        print("Error: Could not import SimGUI. Tkinter might be missing.")
        sys.exit(1)
    except Exception as e:
        import traceback
        print(f"An unexpected error occurred: {e}"); traceback.print_exc()
        sys.exit(1)