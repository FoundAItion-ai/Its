from __future__ import annotations
import pygame
import math
import random
from typing import List, Tuple, Dict, Type, Sequence, Any, Optional # Added Optional

from agent_module import (
    stochastic_motion_random_movement_agent,
    inverse_motion_turning_agent,
    composite_agent,
    _BaseAgent,
    AGENT_CONFIG as agent_module_AGENT_CONFIG,
    agent_base_movement_params
)
from math_module import ( AgentKinematics, FRAME_DT, set_frame_dt ) 
import config as cfg

AGENT_CLASSES: Dict[str, Type[_BaseAgent] | Type[composite_agent]] = {
    "stochastic": stochastic_motion_random_movement_agent,
    "inverse_turn": inverse_motion_turning_agent,
    "composite": composite_agent,
}

class Food: 
    __slots__ = ("x", "y", "r")
    def __init__(self, pos: Tuple[float, float]): self.x, self.y = pos; self.r = cfg.FOOD_RADIUS
    def draw(self, surf: pygame.Surface): pygame.draw.circle(surf, (0, 255, 0), (int(self.x), int(self.y)), self.r)

class SimAgent: 
    __slots__ = ("ctrl", "body", "color", "agent_type_name")
    def __init__(self, cls_name: str, pos: Tuple[float, float],
                 input_time_scale: int, output_time_scale: int, *,
                 initial_direction_deg: float = 0.0,
                 base_turn_rate_deg_dt: float = cfg.AGENT_DEFAULT_TURN_RATE_DEG_PER_DT,
                 agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
                 **controller_kwargs: Any ):
        self.agent_type_name = cls_name
        controller_kwargs_with_turn_rate = {**controller_kwargs, "initial_base_turn_rate_deg_dt": base_turn_rate_deg_dt}
        self.ctrl: _BaseAgent | composite_agent = self._create_controller(cls_name, input_time_scale, output_time_scale, **controller_kwargs_with_turn_rate)
        self.ctrl.base_turn_rate_deg_dt = base_turn_rate_deg_dt
        self.body = AgentKinematics(initial_position=pos, initial_direction_deg=initial_direction_deg, dt=FRAME_DT,
            constant_speed_pixels_sec=agent_constant_speed, initial_turn_rate_rad_per_dt=base_turn_rate_deg_dt * AgentKinematics._DEG_TO_RAD)
        self.color = (random.randint(100, 255), random.randint(50, 150), random.randint(50, 150))
    def _create_controller(self, cls_name: str, input_time_scale: int, output_time_scale: int, **kwargs: Any) -> _BaseAgent | composite_agent: 
        agent_class_constructor = AGENT_CLASSES[cls_name]
        constructor_params = {"input_time_scale": input_time_scale, "output_time_scale": output_time_scale, **kwargs}
        if cls_name == "composite":
            if "layer_configs" not in kwargs or not kwargs["layer_configs"]:
                kwargs["layer_configs"] = [{"agent_type_name": "stochastic", "num_instances": 1, "input_time_scale": input_time_scale, 
                                            "output_time_scale": output_time_scale, "agent_params": {}, 
                                            "initial_base_turn_rate_deg_dt": kwargs.get("initial_base_turn_rate_deg_dt", cfg.AGENT_DEFAULT_TURN_RATE_DEG_PER_DT)}]
            return composite_agent(**constructor_params)
        try: return agent_class_constructor(**constructor_params)
        except Exception as e:
            print(f"Error creating controller for '{cls_name}' with params {constructor_params}: {e}\nFalling back to stochastic.")
            return stochastic_motion_random_movement_agent(input_time_scale=agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                output_time_scale=agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
                initial_base_turn_rate_deg_dt=kwargs.get("initial_base_turn_rate_deg_dt", cfg.AGENT_DEFAULT_TURN_RATE_DEG_PER_DT))
    def step(self, food_signal: int): 
        turn_command, _state, _decision_outputs = self.ctrl.update(food_signal)
        self.body.update(thrust_input_normalized=1.0, turn_command=float(turn_command))
    def draw(self, surf: pygame.Surface): 
        pos_x,pos_y=int(self.body.position.x),int(self.body.position.y); pygame.draw.circle(surf,self.color,(pos_x,pos_y),cfg.AGENT_RADIUS)
        line_len=cfg.AGENT_RADIUS*1.8; end_x=pos_x+line_len*math.cos(self.body.direction_rad); end_y=pos_y+line_len*math.sin(self.body.direction_rad)
        pygame.draw.line(surf,(200,200,255),(pos_x,pos_y),(int(end_x),int(end_y)),2)

_food_eaten_current_run = 0; _total_frames_simulated_current_run = 0

class World:
    def __init__(self):
        self.food: List[Food] = []; self.agents: List[SimAgent] = []
        self.surface: pygame.Surface | None = None

    def reset(self, *, food_preset: str = "three_lines_dense", agent_type: str = "stochastic",
                  n_agents: int = 1, spawn_point: Tuple[float, float] | None = None,
                  initial_direction_deg: float, base_turn_deg_dt: float,
                  input_time_scale: int, output_time_scale: int,
                  layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None,
                  agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
                  agent_params: Optional[Dict[str, Any]] = None # New: specific params for direct agent
                  ):
        global _food_eaten_current_run, _total_frames_simulated_current_run
        _food_eaten_current_run = 0; _total_frames_simulated_current_run = 0
        self.food = [Food(p) for p in cfg.FOOD_PRESETS[food_preset]()]
        self.agents = []
        base_spawn_x, base_spawn_y = spawn_point if spawn_point else (cfg.WINDOW_W / 2, cfg.WINDOW_H / 2)
        
        controller_kwargs_for_sim_agent = agent_params if agent_params is not None else {}
        if agent_type == "composite" and layer_configs_for_composite:
            controller_kwargs_for_sim_agent["layer_configs"] = layer_configs_for_composite
            
        for i in range(n_agents):
            offset_x = (i - (n_agents - 1) / 2) * (cfg.AGENT_RADIUS * 5) if n_agents > 1 else 0
            current_spawn_x = max(cfg.AGENT_RADIUS, min(base_spawn_x + offset_x, cfg.WINDOW_W - cfg.AGENT_RADIUS))
            current_spawn_y = max(cfg.AGENT_RADIUS, min(base_spawn_y, cfg.WINDOW_H - cfg.AGENT_RADIUS))
            
            self.agents.append(SimAgent(
                cls_name=agent_type, pos=(current_spawn_x, current_spawn_y),
                input_time_scale=input_time_scale, output_time_scale=output_time_scale,
                initial_direction_deg=initial_direction_deg,
                base_turn_rate_deg_dt=base_turn_deg_dt,
                agent_constant_speed=agent_constant_speed,
                **controller_kwargs_for_sim_agent # Pass through specific agent params
            ))

    def step(self): 
        global _food_eaten_current_run, _total_frames_simulated_current_run; _total_frames_simulated_current_run +=1
        for ag in self.agents:
            food_eaten_this_step=0
            for f_idx in range(len(self.food)-1,-1,-1):
                f=self.food[f_idx]; dx=ag.body.position.x-f.x; dy=ag.body.position.y-f.y
                if math.hypot(dx,dy)<=(cfg.FOOD_RADIUS+cfg.AGENT_RADIUS): self.food.pop(f_idx); food_eaten_this_step=1; _food_eaten_current_run+=1; break
            ag.step(food_eaten_this_step)
            if ag.body.position.x<-cfg.AGENT_RADIUS:ag.body.position.x=cfg.WINDOW_W+cfg.AGENT_RADIUS
            elif ag.body.position.x>cfg.WINDOW_W+cfg.AGENT_RADIUS:ag.body.position.x=-cfg.AGENT_RADIUS
            if ag.body.position.y<-cfg.AGENT_RADIUS:ag.body.position.y=cfg.WINDOW_H+cfg.AGENT_RADIUS
            elif ag.body.position.y>cfg.WINDOW_H+cfg.AGENT_RADIUS:ag.body.position.y=-cfg.AGENT_RADIUS
    def draw(self, surf: pygame.Surface): 
        surf.fill((30,30,30)); [f.draw(surf) for f in self.food]; [ag.draw(surf) for ag in self.agents]

WORLD = World()
def init_pygame_surface(surface: pygame.Surface | None): WORLD.surface = surface

def configure(*, food_preset: str, agent_type: str, n_agents: int,
              spawn_point: Tuple[float, float], initial_orientation_deg: float,
              base_turn_deg_dt: float, input_time_scale: int, output_time_scale: int,
              layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None,
              agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
              agent_params_for_main_agent: Optional[Dict[str, Any]] = None # New
              ):
    WORLD.reset( food_preset=food_preset, agent_type=agent_type, n_agents=n_agents,
                 spawn_point=spawn_point, initial_direction_deg=initial_orientation_deg,
                 base_turn_deg_dt=base_turn_deg_dt, input_time_scale=input_time_scale,
                 output_time_scale=output_time_scale,
                 layer_configs_for_composite=layer_configs_for_composite,
                 agent_constant_speed=agent_constant_speed,
                 agent_params=agent_params_for_main_agent # Pass to World.reset
                )

def run_frame(is_visual: bool = True): 
    WORLD.step(); (WORLD.draw(WORLD.surface) if is_visual and WORLD.surface is not None else None)
def get_simulation_stats() -> dict: 
    if WORLD: return {"total_food":len(WORLD.food),"active_agents":len(WORLD.agents),"food_eaten_run":_food_eaten_current_run,"total_frames_simulated_run":_total_frames_simulated_current_run,"current_frame_dt":FRAME_DT}
    return {"total_food":"N/A","active_agents":"N/A","food_eaten_run":"N/A","total_frames_simulated_run":"N/A","current_frame_dt":"N/A"}

if __name__ == "__main__": 
    try: from GUI import SimGUI; app=SimGUI(); app.root.mainloop()
    except ImportError as e: print(f"Fatal Error: Could not import SimGUI. {e}")
    except Exception as e: import traceback; print(f"App error: {e}"); traceback.print_exc()