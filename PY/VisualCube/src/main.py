"""
Simulation world: food, agents, collision, logging, and frame loop.

This is free and unencumbered software released into the public domain.
For more information, see LICENSE.txt or https://unlicense.org
"""

from __future__ import annotations
import pygame
import sys
import math
import random
from typing import List, Tuple, Dict, Type, Optional, TextIO, Any
from collections import deque

from agent_module import AGENT_CLASSES, _BaseAgent
from math_module import AgentBody, Vec2D, FRAME_DELTA_TIME_SECONDS
import config as cfg

def point_segment_dist_sq(p: Vec2D, a: Vec2D, b: Vec2D) -> float:
    l2 = (a.x - b.x)**2 + (a.y - b.y)**2
    if l2 == 0.0: return (p.x - a.x)**2 + (p.y - a.y)**2
    t = max(0, min(1, ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2))
    projection_x = a.x + t * (b.x - a.x)
    projection_y = a.y + t * (b.y - a.y)
    return (p.x - projection_x)**2 + (p.y - projection_y)**2

class Food:
    __slots__ = ("pos", "r")
    def __init__(self, pos: Tuple[float, float], radius: int = None): self.pos = Vec2D(*pos); self.r = radius if radius is not None else cfg.FOOD_RADIUS

class SimAgent:
    __slots__ = ("ctrl", "body", "color", "agent_type_name", "trace", "trace_maxlen", "signal_display", "last_eaten_count")

    # Signal display decay: how many frames a signal stays visible
    SIGNAL_DECAY_FRAMES = 15

    def __init__(self, cls_name: str, pos: Tuple[float, float], *,
                 initial_direction_deg: float = 0.0,
                 agent_params: Optional[Dict[str, Any]] = None,
                 permanent_trace: bool = False):
        self.agent_type_name = cls_name

        if permanent_trace:
            self.trace_maxlen = None
        else:
            self.trace_maxlen = 500

        self.trace: List[deque] = [deque(maxlen=self.trace_maxlen)]

        self.ctrl: _BaseAgent = AGENT_CLASSES[cls_name](**(agent_params or {}))
        self.body = AgentBody(pos=pos, theta_deg=initial_direction_deg)
        self.color = (random.randint(100, 255), random.randint(50, 150), random.randint(50, 150))

        # Signal display persistence: {(inv_idx, side): frames_remaining}
        self.signal_display: Dict[Tuple[int, str], int] = {}
        self.last_eaten_count: int = 0

    def plan_and_consume(self, world, current_sim_time: float) -> Tuple[float, float]:
        potential_dist, potential_heading_change = self.ctrl.update(
            0, current_sim_time, FRAME_DELTA_TIME_SECONDS, is_potential_move=True
        )

        start_pos = self.body.position
        temp_body = AgentBody(pos=(start_pos.x, start_pos.y), theta_deg=math.degrees(self.body.heading_radians))
        temp_body.apply_movement(potential_dist, potential_heading_change)
        end_pos = temp_body.position

        eaten_this_frame = 0
        collision_r = cfg.AGENT_RADIUS + cfg.FOOD_RADIUS
        collision_radius_sq = collision_r ** 2
        # Use spatial grid: only check food near the agent's path
        mid_x = (start_pos.x + end_pos.x) * 0.5
        mid_y = (start_pos.y + end_pos.y) * 0.5
        search_margin = max(abs(end_pos.x - start_pos.x), abs(end_pos.y - start_pos.y)) * 0.5 + collision_r
        nearby = world._get_nearby_food(mid_x, mid_y, search_margin)
        for food in nearby:
            if food.r > 0 and point_segment_dist_sq(food.pos, start_pos, end_pos) < collision_radius_sq:
                food.r = -1
                eaten_this_frame += 1
        self.last_eaten_count = eaten_this_frame

        final_dist, final_heading_change = self.ctrl.update(
            eaten_this_frame, current_sim_time, FRAME_DELTA_TIME_SECONDS
        )
        return final_dist, final_heading_change

    def apply_planned_move(self, distance: float, change_in_heading_radians: float, draw_trace: bool):
        self.body.apply_movement(distance, change_in_heading_radians)
        was_wrapped = self._wrap_position()
        
        if draw_trace:
            if was_wrapped and self.trace[-1]:
                self.trace.append(deque(maxlen=self.trace_maxlen))
            
            if self.trace:
                self.trace[-1].append((int(self.body.position.x), int(self.body.position.y)))
    
    def _wrap_position(self) -> bool:
        """Wraps the agent's position to the screen dimensions and reports if a wrap occurred."""
        w, h = cfg.WINDOW_W, cfg.WINDOW_H
        pos_x, pos_y = self.body.position.x, self.body.position.y
        
        is_off_screen = (pos_x >= w or pos_x < 0 or pos_y >= h or pos_y < 0)
        
        if is_off_screen:
            self.body.position.x %= w
            self.body.position.y %= h
            return True
        
        return False

    # Colors for inverter signals: (L_color, R_color) - L is darker, R is brighter
    # Made brighter for visibility on dark background
    INVERTER_COLORS = [
        ((50, 50, 255), (150, 150, 255)),    # Inverter 0: Blue L / Light blue R
        ((255, 50, 50), (255, 150, 150)),    # Inverter 1: Red L / Light red R
        ((255, 150, 0), (255, 220, 100)),    # Inverter 2: Orange L / Light orange R
        ((50, 255, 50), (150, 255, 150)),    # Inverter 3: Green L / Light green R
        ((200, 50, 200), (255, 150, 255)),   # Inverter 4: Purple L / Light purple R
    ]

    # Signal display settings
    SIGNAL_BASE_RADIUS = 6  # Larger base size for visibility

    def draw(self, surf: pygame.Surface):
        pos_x, pos_y = int(self.body.position.x), int(self.body.position.y)

        # Update signal_display with new signals from controller
        if hasattr(self.ctrl, 'last_signals'):
            for inv_idx, side, value in self.ctrl.last_signals:
                self.signal_display[(inv_idx, side)] = self.SIGNAL_DECAY_FRAMES

        # Draw signal indicators behind the agent (with persistence)
        if self.signal_display:
            behind_dist = cfg.AGENT_RADIUS * 3.0
            behind_x = pos_x - behind_dist * math.cos(self.body.direction_rad)
            behind_y = pos_y - behind_dist * math.sin(self.body.direction_rad)
            perp_angle = self.body.direction_rad + math.pi / 2

            # Draw all active signals and decay them
            keys_to_remove = []
            for (inv_idx, side), frames_left in list(self.signal_display.items()):
                if inv_idx < len(self.INVERTER_COLORS):
                    l_color, r_color = self.INVERTER_COLORS[inv_idx]
                    base_color = l_color if side == 'L' else r_color

                    # Fade color based on remaining frames (min 30% brightness)
                    fade = 0.3 + 0.7 * (frames_left / self.SIGNAL_DECAY_FRAMES)
                    color = tuple(int(c * fade) for c in base_color)

                    # Offset L signals to left, R signals to right (wider spacing)
                    offset = 8 + inv_idx * 6
                    if side == 'L':
                        offset = -offset

                    dot_x = int(behind_x + offset * math.cos(perp_angle))
                    dot_y = int(behind_y + offset * math.sin(perp_angle))

                    # Size scales with fade (larger base size)
                    radius = max(3, int(self.SIGNAL_BASE_RADIUS * fade))
                    pygame.draw.circle(surf, color, (dot_x, dot_y), radius)

                # Decay signal
                self.signal_display[(inv_idx, side)] = frames_left - 1
                if frames_left <= 1:
                    keys_to_remove.append((inv_idx, side))

            for key in keys_to_remove:
                del self.signal_display[key]

        # Draw agent body
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
        self.permanent_trace_enabled: bool = False
        self.total_sim_time: float = 0.0
        self.frame_count: int = 0
        self.log_file_handle: Optional[TextIO] = None
        # History storage for graphs (capped to prevent unbounded growth)
        _ml = cfg.STATS_HISTORY_MAX_LEN
        self.stats_history: Dict[str, deque] = {
            "time": deque(maxlen=_ml),
            "food_eaten": deque(maxlen=_ml),
            "food_available": deque(maxlen=_ml),
            "decisions": deque(maxlen=_ml),
        }

    def reset(self, *, food_preset: str, agent_type: str, n_agents: int,
                  spawn_point: Tuple[float, float], initial_direction_deg: float,
                  agent_params: Optional[Dict[str, Any]], draw_trace: bool,
                  permanent_trace: bool, log_file_handle: Optional[TextIO],
                  log_stride: int = 1):
        global _food_eaten_current_run
        _food_eaten_current_run = 0
        self.total_sim_time = 0.0
        self.frame_count = 0
        self.draw_trace_enabled = draw_trace
        self.permanent_trace_enabled = permanent_trace
        self.log_file_handle = log_file_handle
        self.log_stride = log_stride
        
        # Reset history (capped deques)
        _ml = cfg.STATS_HISTORY_MAX_LEN
        self.stats_history = {
            "time": deque(maxlen=_ml),
            "food_eaten": deque(maxlen=_ml),
            "food_available": deque(maxlen=_ml),
            "decisions": deque(maxlen=_ml),
            "excursion_dist": deque(maxlen=_ml),
        }

        food_gen = cfg.FOOD_PRESETS.get(food_preset, cfg.FOOD_PRESETS["void"])
        self.food = [Food(p[:2], p[2] if len(p) > 2 else None) for p in food_gen()]
        # Compute food band Y extent for excursion tracking
        if self.food:
            ys = [f.pos.y for f in self.food]
            self._band_y_min = min(ys)
            self._band_y_max = max(ys)
        else:
            self._band_y_min = cfg.WINDOW_H * 0.4
            self._band_y_max = cfg.WINDOW_H * 0.6
        self._band_margin = cfg.AGENT_RADIUS  # margin beyond band edge before counting as "outside"
        # Spatial grid for fast collision detection
        self._grid_cell = max(cfg.AGENT_RADIUS + cfg.FOOD_RADIUS, 20)  # cell size in px
        self._food_grid: Dict[Tuple[int, int], List[Food]] = {}
        self._rebuild_food_grid()
        # Pre-render food dot sprites keyed by radius
        self._food_sprites: Dict[int, pygame.Surface] = {}
        for f in self.food:
            r = f.r
            if r > 0 and r not in self._food_sprites:
                s = pygame.Surface((r * 2, r * 2), pygame.SRCALPHA)
                pygame.draw.circle(s, (0, 255, 0), (r, r), r)
                self._food_sprites[r] = s
        self.agents = []
        base_x, base_y = spawn_point if spawn_point else (cfg.WINDOW_W / 2, cfg.WINDOW_H / 2)
        for i in range(n_agents):
            offset = (i - (n_agents - 1) / 2) * (cfg.AGENT_RADIUS * 5) if n_agents > 1 else 0
            spawn_x = max(cfg.AGENT_RADIUS, min(base_x + offset, cfg.WINDOW_W - cfg.AGENT_RADIUS))
            agent_direction = random.uniform(0, 360) if initial_direction_deg < 0 else initial_direction_deg
            self.agents.append(SimAgent(
                cls_name=agent_type, pos=(spawn_x, base_y),
                initial_direction_deg=agent_direction,
                agent_params=agent_params,
                permanent_trace=self.permanent_trace_enabled
            ))
        # Excursion tracking per agent
        self._in_band: List[bool] = [True] * n_agents  # whether agent is inside band
        self._excursion_max_dist: List[float] = [0.0] * n_agents  # max dist from band edge in current excursion
        self._excursion_history: List[List[float]] = [[] for _ in range(n_agents)]  # completed excursion distances

    def _rebuild_food_grid(self):
        self._food_grid.clear()
        cell = self._grid_cell
        for f in self.food:
            if f.r > 0:
                key = (int(f.pos.x // cell), int(f.pos.y // cell))
                self._food_grid.setdefault(key, []).append(f)

    def _get_nearby_food(self, x: float, y: float, margin: float) -> List[Food]:
        cell = self._grid_cell
        cx0 = int((x - margin) // cell)
        cx1 = int((x + margin) // cell)
        cy0 = int((y - margin) // cell)
        cy1 = int((y + margin) // cell)
        result = []
        for cx in range(cx0, cx1 + 1):
            for cy in range(cy0, cy1 + 1):
                bucket = self._food_grid.get((cx, cy))
                if bucket:
                    result.extend(bucket)
        return result

    def step(self):
        global _food_eaten_current_run
        self.total_sim_time += FRAME_DELTA_TIME_SECONDS
        self.frame_count += 1

        planned_moves = []
        for ag in self.agents:
            distance, heading_change = ag.plan_and_consume(self, self.total_sim_time)
            planned_moves.append((distance, heading_change))

        eaten_count = sum(ag.last_eaten_count for ag in self.agents)
        _food_eaten_current_run += eaten_count
        if eaten_count > 0:
            self.food = [f for f in self.food if f.r > 0]
            self._rebuild_food_grid()
        
        should_log = (self.log_file_handle and not self.log_file_handle.closed
                      and self.frame_count % self.log_stride == 0)
        log_write = self.log_file_handle.write if should_log else None
        for i, ag in enumerate(self.agents):
            distance, heading_change = planned_moves[i]
            ag.apply_planned_move(distance, heading_change, self.draw_trace_enabled)

            if log_write:
                pos = ag.body.position
                # Enhanced logging: include food_frequency, mode, speed, signals
                food_freq = ag.ctrl.current_food_frequency if hasattr(ag.ctrl, 'current_food_frequency') else 0
                # Determine mode from inverter state
                if hasattr(ag.ctrl, 'inverters'):
                    # Composite: show mode of first inverter
                    inv0 = ag.ctrl.inverters[0] if ag.ctrl.inverters else None
                    mode = "LOW" if (inv0 and inv0.is_suppressed) else "HIGH"
                elif hasattr(ag.ctrl, 'inverter'):
                    mode = "LOW" if ag.ctrl.inverter.is_suppressed else "HIGH"
                else:
                    mode = "N/A"
                speed = distance / FRAME_DELTA_TIME_SECONDS if FRAME_DELTA_TIME_SECONDS > 0 else 0

                # Output signals as 1/0 per L/R channel per inverter
                n_inv = getattr(ag.ctrl, 'num_inverters', 0)
                inv_signals = [[0, 0] for _ in range(n_inv)]  # [L, R] per inverter
                if hasattr(ag.ctrl, 'last_signals'):
                    for idx, side, _ in ag.ctrl.last_signals:
                        if idx < n_inv:
                            inv_signals[idx][0 if side == 'L' else 1] = 1

                signals_csv = ",".join(f"{s[0]},{s[1]}" for s in inv_signals)

                log_line = (f"{self.frame_count},{i},{pos.x:.2f},{pos.y:.2f},{ag.body.heading_radians:.4f},"
                            f"{distance:.4f},{heading_change:.6f},{food_freq:.2f},{mode},{speed:.2f},{signals_csv}\n")
                try:
                    log_write(log_line)
                except Exception as e:
                    print(f"Error writing to log file: {e}")
                    self.log_file_handle = None
                    log_write = None

        # Track excursions from food band per agent
        EXCURSION_WINDOW = 20  # keep last N excursions for rolling average
        band_lo = self._band_y_min - self._band_margin
        band_hi = self._band_y_max + self._band_margin
        for i, ag in enumerate(self.agents):
            y = ag.body.position.y
            inside = band_lo <= y <= band_hi
            if self._in_band[i] and not inside:
                # Agent just left the band — start tracking excursion
                self._in_band[i] = False
                self._excursion_max_dist[i] = 0.0
            if not self._in_band[i]:
                # Agent is outside — update max distance from nearest band edge
                dist_from_band = min(abs(y - band_lo), abs(y - band_hi))
                if dist_from_band > self._excursion_max_dist[i]:
                    self._excursion_max_dist[i] = dist_from_band
                if inside:
                    # Agent just returned — record completed excursion
                    self._in_band[i] = True
                    if self._excursion_max_dist[i] > 0:
                        self._excursion_history[i].append(self._excursion_max_dist[i])
                        if len(self._excursion_history[i]) > EXCURSION_WINDOW:
                            self._excursion_history[i] = self._excursion_history[i][-EXCURSION_WINDOW:]
                    self._excursion_max_dist[i] = 0.0

        # Compute average excursion distance for agent 0
        if self._excursion_history and self._excursion_history[0]:
            avg_excursion = sum(self._excursion_history[0]) / len(self._excursion_history[0])
        else:
            avg_excursion = 0.0

        # Record stats for this frame
        total_decisions = sum(a.ctrl.decision_count for a in self.agents)
        self.stats_history["time"].append(self.total_sim_time)
        self.stats_history["food_eaten"].append(_food_eaten_current_run)
        self.stats_history["food_available"].append(len(self.food))
        self.stats_history["decisions"].append(total_decisions)
        self.stats_history["excursion_dist"].append(avg_excursion)

    def draw(self, surf: pygame.Surface):
        surf.fill((30, 30, 30))
        # Batch food drawing using pre-rendered sprites
        blit = surf.blit
        sprites = self._food_sprites
        for f in self.food:
            sprite = sprites.get(f.r)
            if sprite:
                blit(sprite, (int(f.pos.x) - f.r, int(f.pos.y) - f.r))
        for ag in self.agents:
            ag.draw(surf)
            if self.draw_trace_enabled:
                for trace_segment in ag.trace:
                    if len(trace_segment) > 1:
                        pygame.draw.aalines(surf, (255, 255, 0), False, list(trace_segment))

WORLD = World()
_food_eaten_current_run = 0

def init_pygame_surface(surface: Optional[pygame.Surface]):
    if WORLD: WORLD.surface = surface

def configure(*, food_preset: str, agent_type: str, n_agents: int,
              spawn_point: Tuple[float, float], initial_direction_deg: float,
              agent_params_for_main_agent: Optional[Dict[str, Any]],
              draw_trace: bool, permanent_trace: bool, log_file_handle: Optional[TextIO],
              log_stride: int = 1):
    if WORLD:
        WORLD.reset(
            food_preset=food_preset, agent_type=agent_type, n_agents=n_agents,
            spawn_point=spawn_point, initial_direction_deg=initial_direction_deg,
            agent_params=agent_params_for_main_agent, draw_trace=draw_trace,
            permanent_trace=permanent_trace, log_file_handle=log_file_handle,
            log_stride=log_stride,
        )

def run_frame(is_visual: bool = True):
    if WORLD:
        WORLD.step()
        if is_visual and WORLD.surface:
            WORLD.draw(WORLD.surface)

def teleport_agent(agent_index: int, new_x: float, new_y: float):
    """Reposition an agent, reset its inverters and food frequency, break trace."""
    if not WORLD or agent_index >= len(WORLD.agents):
        return
    ag = WORLD.agents[agent_index]
    ag.body.position.x = new_x
    ag.body.position.y = new_y
    # Reset inverters
    ctrl = ag.ctrl
    if hasattr(ctrl, 'inverter'):
        ctrl.inverter.reset()
    if hasattr(ctrl, 'inverters'):
        for inv in ctrl.inverters:
            inv.reset()
    # Clear food frequency window
    if hasattr(ctrl, 'food_timestamps'):
        ctrl.food_timestamps.clear()
    if hasattr(ctrl, 'current_food_frequency'):
        ctrl.current_food_frequency = 0.0
    # Reset excursion tracking for this agent
    if agent_index < len(WORLD._in_band):
        WORLD._in_band[agent_index] = True
        WORLD._excursion_max_dist[agent_index] = 0.0
    # Break trace continuity
    ag.trace.append(deque(maxlen=ag.trace_maxlen))

def get_simulation_stats() -> dict:
    if not WORLD or not WORLD.agents: return {}
    stats = {
        "total_food": len(WORLD.food),
        "active_agents": len(WORLD.agents),
        "food_eaten_run": _food_eaten_current_run,
        "agent_type": WORLD.agents[0].agent_type_name,
        # NEW: Expose history to GUI
        "history": WORLD.stats_history
    }
    ctrl = WORLD.agents[0].ctrl
    if hasattr(ctrl, 'current_food_frequency'):
        stats['input_food_frequency'] = ctrl.current_food_frequency
    return stats

if __name__ == "__main__":
    from GUI import SimGUI
    app = SimGUI(); app.root.mainloop()