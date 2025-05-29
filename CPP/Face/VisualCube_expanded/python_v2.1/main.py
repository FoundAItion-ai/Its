
from __future__ import annotations
import pygame
import math
import random
from typing import List, Tuple, Dict, Type, Sequence, Any # Added Sequence, Any

# Assumes agent_module.py contains agent logic, base classes, and configurations
from agent_module import (
    stochastic_motion_random_movement_agent,
    inverse_motion_turning_agent,
    deceleration_motion_agent,
    composite_agent,
    _BaseAgent,
    AGENT_CONFIG as agent_module_AGENT_CONFIG, # Renamed for clarity
    agent_base_movement_params
)
from math_module import (
    AgentKinematics, AgentPhysicsProps, Vec2D, FRAME_DT,
    set_frame_dt, set_environment_density
)
import config as cfg

# --- MODIFIED AGENT_CLASSES ---
# Mapping of agent type names to their respective classes
AGENT_CLASSES: Dict[str, Type[_BaseAgent] | Type[composite_agent]] = {
    "stochastic": stochastic_motion_random_movement_agent,
    "inverse_turn": inverse_motion_turning_agent,
    "deceleration": deceleration_motion_agent,
    "composite": composite_agent, # Generic composite, configured by GUI
}
# --- END OF MODIFIED AGENT_CLASSES ---

class Food:
    __slots__ = ("x", "y", "r")

    def __init__(self, pos: Tuple[float, float]):
        self.x, self.y = pos
        self.r = cfg.FOOD_RADIUS

    def draw(self, surf: pygame.Surface):
        pygame.draw.circle(surf, (0, 255, 0), (int(self.x), int(self.y)), self.r)

class SimAgent:
    __slots__ = ("ctrl", "body", "color", "agent_type_name", "current_thrust_normalized", "physics_props")

    def __init__(self, cls_name: str, pos: Tuple[float, float],
                 input_time_scale: int,    # For atomic agents, or composite's own input_ts
                 output_time_scale: int,   # For atomic agents, or composite's own aggregation_ts
                 *,
                 initial_direction_deg: float | None = None,
                 initial_thrust_normalized: float = 0.0,
                 base_turn_rate_deg_dt: float = cfg.AGENT_DEFAULT_TURN_RATE_DEG_PER_DT,
                 **controller_kwargs: Any # To pass layer_configs etc. to _create_controller
                 ):

        self.agent_type_name = cls_name
        self.ctrl: _BaseAgent | composite_agent = self._create_controller(
            cls_name,
            input_time_scale=input_time_scale,
            output_time_scale=output_time_scale,
            **controller_kwargs # Pass through kwargs like layer_configs
        )

        self.ctrl.current_target_thrust_normalized = initial_thrust_normalized
        self.ctrl.movement[0] = initial_thrust_normalized
        self.ctrl.movement[1] = base_turn_rate_deg_dt

        self.current_thrust_normalized = initial_thrust_normalized

        self.physics_props = AgentPhysicsProps(
            mass=cfg.AGENT_DEFAULT_MASS,
            max_thrust_force=cfg.AGENT_DEFAULT_MAX_THRUST,
            drag_coefficient=cfg.AGENT_DEFAULT_DRAG_COEFF,
            frontal_area=cfg.AGENT_DEFAULT_FRONTAL_AREA
        )
        if initial_direction_deg is None:
            initial_direction_deg = random.uniform(0, 360)

        max_speed_agent_logic = getattr(self.ctrl, 'max_speed_cap', None)

        self.body = AgentKinematics(
            initial_position=pos,
            initial_direction_deg=initial_direction_deg,
            physics_props=self.physics_props,
            dt=FRAME_DT,
            max_speed_cap=max_speed_agent_logic,
            initial_turn_rate_rad_per_dt=base_turn_rate_deg_dt * AgentKinematics._DEG_TO_RAD
        )
        self.color = (random.randint(100, 255), random.randint(50, 150), random.randint(50, 150))

    def _create_controller(self, cls_name: str,
                           input_time_scale: int, 
                           output_time_scale: int,
                           **kwargs: Any # Receives layer_configs etc.
                           ) -> _BaseAgent | composite_agent:
        agent_class_constructor = AGENT_CLASSES[cls_name]

        # Base parameters for any agent's constructor.
        # These are the agent's (or composite's own) time scales.
        constructor_params = {
            "input_time_scale": input_time_scale,
            "output_time_scale": output_time_scale,
            **kwargs # Pass through other kwargs from SimAgent init (like layer_configs)
        }

        if cls_name == "composite":
            # layer_configs should be in kwargs, provided by GUI through main.configure
            if "layer_configs" not in kwargs or not kwargs["layer_configs"]:
                print(f"Warning: Composite agent '{cls_name}' created without layer_configs. Defaulting to one stochastic layer.")
                # Fallback: create a default layer config
                default_layer_agent_params = {
                    "accel_step": agent_module_AGENT_CONFIG["ACCEL_STEP"],
                    "decel_step": agent_module_AGENT_CONFIG["DECEL_STEP"]
                }
                kwargs["layer_configs"] = [{
                    "agent_type_name": "stochastic",
                    "num_instances": 1,
                    "input_time_scale": input_time_scale, # Use composite's scales for this default layer
                    "output_time_scale": output_time_scale,
                    "agent_params": default_layer_agent_params
                }]
            
            # Ensure layer_configs is passed correctly to composite_agent
            # constructor_params already includes **kwargs, so layer_configs is there.
            return composite_agent(**constructor_params)

        # For atomic agents:
        # kwargs might contain agent-specific parameters if we add GUI for them later.
        # For now, they mostly use defaults or global AGENT_CONFIG values via _BaseAgent._process_kwargs
        
        # Example of how specific params could be handled for atomic agents if needed:
        # if cls_name == "inverse_turn":
        #     constructor_params.update({"initial_turn_preference": -1}) # Hardcoded example

        try:
            return agent_class_constructor(**constructor_params)
        except Exception as e:
            print(f"Error creating controller for '{cls_name}' with params {constructor_params}: {e}")
            print("Falling back to stochastic_motion_random_movement_agent.")
            # Fallback with minimal safe parameters
            return stochastic_motion_random_movement_agent(
                input_time_scale=agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                output_time_scale=agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]
            )

    def step(self, food_signal: int):
        turn_command, _state, delta_thrust_turn_rate = self.ctrl.update(food_signal)

        self.ctrl.current_target_thrust_normalized += delta_thrust_turn_rate[0]
        self.ctrl.current_target_thrust_normalized = max(0.0, min(1.0, self.ctrl.current_target_thrust_normalized))
        
        self.current_thrust_normalized = self.ctrl.current_target_thrust_normalized
        self.ctrl.movement[0] = self.ctrl.current_target_thrust_normalized

        if len(delta_thrust_turn_rate) > 1 and delta_thrust_turn_rate[1] != 0.0:
            self.ctrl.movement[1] += delta_thrust_turn_rate[1]
            max_abs_base_turn_deg_dt = 45.0
            self.ctrl.movement[1] = max(-max_abs_base_turn_deg_dt, min(max_abs_base_turn_deg_dt, self.ctrl.movement[1]))
            self.body.current_turn_rate_rad_per_dt = self.ctrl.movement[1] * AgentKinematics._DEG_TO_RAD

        self.body.update(thrust_input_normalized=self.current_thrust_normalized, turn_command=float(turn_command))
        
        if hasattr(self.ctrl, 'max_speed_cap'): # Sync max_speed_cap if controller defines it
            self.body.max_speed_cap = getattr(self.ctrl, 'max_speed_cap')

    def draw(self, surf: pygame.Surface):
        pos_x, pos_y = int(self.body.position.x), int(self.body.position.y)
        pygame.draw.circle(surf, self.color, (pos_x, pos_y), cfg.AGENT_RADIUS)
        line_len = cfg.AGENT_RADIUS * 1.8
        end_x = pos_x + line_len * math.cos(self.body.direction_rad)
        end_y = pos_y + line_len * math.sin(self.body.direction_rad)
        pygame.draw.line(surf, (200, 200, 255), (pos_x, pos_y), (int(end_x), int(end_y)), 2)

_food_eaten_current_run = 0

class World:
    def __init__(self):
        self.food: List[Food] = []
        self.agents: List[SimAgent] = []
        self.surface: pygame.Surface | None = None

    def reset(self, *, food_preset: str = "three_lines_dense", agent_type: str = "stochastic",
                  n_agents: int = 1, spawn_point: Tuple[float, float] | None = None,
                  initial_thrust: float, base_turn_deg_dt: float,
                  input_time_scale: int, output_time_scale: int, # For agent/composite's own TS
                  layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None # New
                  ):
        global _food_eaten_current_run
        _food_eaten_current_run = 0
        self.food = [Food(p) for p in cfg.FOOD_PRESETS[food_preset]()]
        self.agents = []
        base_spawn_x, base_spawn_y = spawn_point if spawn_point else (cfg.WINDOW_W / 2, cfg.WINDOW_H / 2)
        
        agent_constructor_extra_kwargs = {}
        if agent_type == "composite" and layer_configs_for_composite:
            agent_constructor_extra_kwargs["layer_configs"] = layer_configs_for_composite
            # Pass composite agent's own parameters if they are part of its config (e.g., max_speed_cap)
            # For now, composite agent uses global config for these or relies on _BaseAgent defaults

        for i in range(n_agents):
            offset_x = (i - (n_agents - 1) / 2) * (cfg.AGENT_RADIUS * 5) if n_agents > 1 else 0
            current_spawn_x = max(cfg.AGENT_RADIUS, min(base_spawn_x + offset_x, cfg.WINDOW_W - cfg.AGENT_RADIUS))
            current_spawn_y = max(cfg.AGENT_RADIUS, min(base_spawn_y, cfg.WINDOW_H - cfg.AGENT_RADIUS))
            
            self.agents.append(SimAgent(
                cls_name=agent_type,
                pos=(current_spawn_x, current_spawn_y),
                input_time_scale=input_time_scale,     # Passed to SimAgent, then to _create_controller
                output_time_scale=output_time_scale,   # Passed to SimAgent, then to _create_controller
                initial_thrust_normalized=initial_thrust,
                base_turn_rate_deg_dt=base_turn_deg_dt,
                **agent_constructor_extra_kwargs # Passes 'layer_configs' if present
            ))

    def step(self):
        global _food_eaten_current_run
        for ag in self.agents:
            food_eaten_this_step = 0
            for f_idx in range(len(self.food) - 1, -1, -1):
                f = self.food[f_idx]
                dx = ag.body.position.x - f.x
                dy = ag.body.position.y - f.y
                if math.hypot(dx, dy) <= (cfg.FOOD_RADIUS + cfg.AGENT_RADIUS):
                    self.food.pop(f_idx)
                    food_eaten_this_step = 1
                    _food_eaten_current_run += 1
                    break
            ag.step(food_eaten_this_step)
            if ag.body.position.x < -cfg.AGENT_RADIUS: ag.body.position.x = cfg.WINDOW_W + cfg.AGENT_RADIUS
            elif ag.body.position.x > cfg.WINDOW_W + cfg.AGENT_RADIUS: ag.body.position.x = -cfg.AGENT_RADIUS
            if ag.body.position.y < -cfg.AGENT_RADIUS: ag.body.position.y = cfg.WINDOW_H + cfg.AGENT_RADIUS
            elif ag.body.position.y > cfg.WINDOW_H + cfg.AGENT_RADIUS: ag.body.position.y = -cfg.AGENT_RADIUS

    def draw(self, surf: pygame.Surface):
        surf.fill((30, 30, 30))
        for f in self.food: f.draw(surf)
        for ag in self.agents: ag.draw(surf)

WORLD = World()

def init_pygame_surface(surface: pygame.Surface | None):
    WORLD.surface = surface

def configure(*, food_preset: str, agent_type: str, n_agents: int,
              spawn_point: Tuple[float, float], initial_thrust: float, base_turn_deg_dt: float,
              input_time_scale: int, output_time_scale: int,
              layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None # New param
              ):
    WORLD.reset(
        food_preset=food_preset, agent_type=agent_type, n_agents=n_agents,
        spawn_point=spawn_point, initial_thrust=initial_thrust,
        base_turn_deg_dt=base_turn_deg_dt,
        input_time_scale=input_time_scale, # For agent/composite's own TS
        output_time_scale=output_time_scale, # For agent/composite's own TS
        layer_configs_for_composite=layer_configs_for_composite # Pass through
    )

def run_frame():
    if WORLD.surface is None: return
    WORLD.step()
    WORLD.draw(WORLD.surface)

def get_simulation_stats() -> dict:
    if WORLD:
        return {
            "total_food": len(WORLD.food),
            "active_agents": len(WORLD.agents),
            "food_eaten_run": _food_eaten_current_run
        }
    return {"total_food": "N/A", "active_agents": "N/A", "food_eaten_run": "N/A"}

if __name__ == "__main__":
    try:
        from GUI import SimGUI
        app = SimGUI()
        app.root.mainloop()
    except ImportError as e:
        print(f"Fatal Error: Could not import SimGUI. {e}\n"
              "Please ensure GUI.py is in the same directory or Python path.")
    except Exception as e:
        import traceback
        print(f"Application launch error: {e}")
        traceback.print_exc()