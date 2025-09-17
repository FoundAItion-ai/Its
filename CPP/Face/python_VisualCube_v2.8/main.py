# main.py

from __future__ import annotations
import pygame
import sys
import math
import random
from typing import List, Tuple, Dict, Type, Optional, TextIO, Any
from collections import deque

from agent_module import AGENT_CLASSES, _BaseAgent
# Import the new AgentBody and other components
from math_module import AgentBody, Vec2D, FRAME_DT, set_frame_dt
import config as cfg

# ... (point_segment_dist_sq and Food class remain the same) ...
def point_segment_dist_sq(p: Vec2D, a: Vec2D, b: Vec2D) -> float:
    l2 = (a.x - b.x)**2 + (a.y - b.y)**2
    if l2 == 0.0: return (p.x - a.x)**2 + (p.y - a.y)**2
    t = max(0, min(1, ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2))
    projection_x = a.x + t * (b.x - a.x)
    projection_y = a.y + t * (b.y - a.y)
    return (p.x - projection_x)**2 + (p.y - projection_y)**2

class Food:
    __slots__ = ("pos", "r")
    def __init__(self, pos: Tuple[float, float]): self.pos = Vec2D(*pos); self.r = cfg.FOOD_RADIUS
    def draw(self, surf: pygame.Surface):
        pygame.draw.circle(surf, (0, 255, 0), (int(self.pos.x), int(self.pos.y)), self.r)

class SimAgent:
    __slots__ = ("ctrl", "body", "color", "agent_type_name", "trace")
    def __init__(self, cls_name: str, pos: Tuple[float, float], *,
                 initial_direction_deg: float = 0.0,
                 agent_params: Optional[Dict[str, Any]] = None):
        self.agent_type_name = cls_name
        self.trace = deque(maxlen=500)
        self.ctrl: _BaseAgent = AGENT_CLASSES[cls_name](**(agent_params or {}))
        # Use the new AgentBody
        self.body = AgentBody(pos=pos, theta_deg=initial_direction_deg)
        self.color = (random.randint(100, 255), random.randint(50, 150), random.randint(50, 150))

    def plan_and_consume(self, food_list: List[Food], current_sim_time: float) -> Tuple[float, float]:
        # Agent controller now returns final (distance, delta_theta) for the frame
        
        potential_dist, potential_delta_theta = self.ctrl.update(0, current_sim_time, FRAME_DT)
        
        start_pos = self.body.position
        temp_body = AgentBody(pos=(start_pos.x, start_pos.y), theta_deg=math.degrees(self.body.theta))
        # Use the new apply_movement method for collision check
        temp_body.apply_movement(potential_dist, potential_delta_theta)
        end_pos = temp_body.position
        
        eaten_this_frame = 0
        collision_radius_sq = (cfg.AGENT_RADIUS + cfg.FOOD_RADIUS) ** 2
        for food in food_list:
            if food.r > 0 and point_segment_dist_sq(food.pos, start_pos, end_pos) < collision_radius_sq:
                food.r = -1
                eaten_this_frame += 1
        
        final_dist, final_delta_theta = self.ctrl.update(eaten_this_frame, current_sim_time, FRAME_DT)
        return final_dist, final_delta_theta

    def apply_planned_move(self, distance: float, delta_theta: float, draw_trace: bool):
        # Pass the final values to the body's new, correct application method
        self.body.apply_movement(distance, delta_theta)
        self._wrap_position()
        if draw_trace:
            self.trace.append((int(self.body.position.x), int(self.body.position.y)))
    
    # ... (the rest of the file remains the same as the previous correct version) ...
    def _wrap_position(self):
        w, h = cfg.WINDOW_W, cfg.WINDOW_H
        self.body.pos.x %= w
        self.body.pos.y %= h

    def draw(self, surf: pygame.Surface):
        pos_x, pos_y = int(self.body.position.x), int(self.body.position.y)
        pygame.draw.circle(surf, self.color, (pos_x, pos_y), cfg.AGENT_RADIUS)
        end_x = pos_x + cfg.AGENT_RADIUS * 1.8 * math.cos(self.body.direction_rad)
        end_y = pos_y + cfg.AGENT_RADIUS * 1.8 * math.sin(self.body.direction_rad)
        pygame.draw.line(surf, (200, 200, 255), (pos_x, pos_y), (int(end_x), int(end_y)), 2)

class World:
    def __init__(self):
        self.food: List[Food] = []
        self.agents: List[SimAgent] = []
        self.surface: Optional[pygame.Surface] = None
        self.draw_trace_enabled: bool = False
        self.total_sim_time: float = 0.0
        self.frame_count: int = 0
        self.log_file_handle: Optional[TextIO] = None

    def reset(self, *, food_preset: str, agent_type: str, n_agents: int,
                  spawn_point: Tuple[float, float], initial_direction_deg: float,
                  agent_params: Optional[Dict[str, Any]], draw_trace: bool,
                  log_file_handle: Optional[TextIO]):
        global _food_eaten_current_run
        _food_eaten_current_run = 0
        self.total_sim_time = 0.0
        self.frame_count = 0
        self.draw_trace_enabled = draw_trace
        self.log_file_handle = log_file_handle

        food_gen = cfg.FOOD_PRESETS.get(food_preset, cfg.FOOD_PRESETS["void"])
        self.food = [Food(p) for p in food_gen()]
        self.agents = []
        base_x, base_y = spawn_point if spawn_point else (cfg.WINDOW_W / 2, cfg.WINDOW_H / 2)
        for i in range(n_agents):
            offset = (i - (n_agents - 1) / 2) * (cfg.AGENT_RADIUS * 5) if n_agents > 1 else 0
            spawn_x = max(cfg.AGENT_RADIUS, min(base_x + offset, cfg.WINDOW_W - cfg.AGENT_RADIUS))
            self.agents.append(SimAgent(
                cls_name=agent_type, pos=(spawn_x, base_y),
                initial_direction_deg=initial_direction_deg, agent_params=agent_params
            ))

    def step(self):
        global _food_eaten_current_run
        self.total_sim_time += FRAME_DT
        self.frame_count += 1
        
        planned_moves = []
        for ag in self.agents:
            distance, delta_theta = ag.plan_and_consume(self.food, self.total_sim_time)
            planned_moves.append((distance, delta_theta))

        initial_food_count = len(self.food)
        self.food = [f for f in self.food if f.r > 0]
        _food_eaten_current_run += initial_food_count - len(self.food)
        
        for i, ag in enumerate(self.agents):
            distance, delta_theta = planned_moves[i]
            ag.apply_planned_move(distance, delta_theta, self.draw_trace_enabled)
            
            if self.log_file_handle and not self.log_file_handle.closed:
                pos = ag.body.position
                log_line = (f"{self.frame_count},{i},{pos.x:.2f},{pos.y:.2f},{ag.body.theta:.4f},"
                            f"{distance:.4f},{delta_theta:.6f}\n")
                try:
                    self.log_file_handle.write(log_line)
                except Exception as e:
                    print(f"Error writing to log file: {e}")
                    self.log_file_handle = None

    def draw(self, surf: pygame.Surface):
        surf.fill((30, 30, 30))
        for f in self.food: f.draw(surf)
        for ag in self.agents:
            ag.draw(surf)
            if self.draw_trace_enabled and len(ag.trace) > 1:
                pygame.draw.aalines(surf, (255, 255, 0), False, list(ag.trace))

WORLD = World()
_food_eaten_current_run = 0

def init_pygame_surface(surface: Optional[pygame.Surface]):
    if WORLD: WORLD.surface = surface

def configure(*, food_preset: str, agent_type: str, n_agents: int,
              spawn_point: Tuple[float, float], initial_direction_deg: float,
              agent_params_for_main_agent: Optional[Dict[str, Any]],
              draw_trace: bool, log_file_handle: Optional[TextIO]):
    if WORLD:
        WORLD.reset(
            food_preset=food_preset, agent_type=agent_type, n_agents=n_agents,
            spawn_point=spawn_point, initial_direction_deg=initial_direction_deg,
            agent_params=agent_params_for_main_agent, draw_trace=draw_trace,
            log_file_handle=log_file_handle
        )

def run_frame(is_visual: bool = True):
    if WORLD:
        WORLD.step()
        if is_visual and WORLD.surface:
            WORLD.draw(WORLD.surface)

def get_simulation_stats() -> dict:
    if not WORLD or not WORLD.agents: return {}
    stats = {
        "total_food": len(WORLD.food),
        "active_agents": len(WORLD.agents),
        "food_eaten_run": _food_eaten_current_run,
        "agent_type": WORLD.agents[0].agent_type_name
    }
    ctrl = WORLD.agents[0].ctrl
    if hasattr(ctrl, 'current_food_frequency'):
        stats['input_food_frequency'] = ctrl.current_food_frequency
    return stats

if __name__ == "__main__":
    from GUI import SimGUI
    app = SimGUI(); app.root.mainloop()