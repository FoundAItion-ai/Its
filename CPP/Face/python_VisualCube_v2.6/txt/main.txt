
from __future__ import annotations
import pygame
import sys
import math
import random
from typing import List, Tuple, Dict, Type, Sequence, Any, Optional # Added Optional

from agent_module import (
    stochastic_motion_random_movement_agent,
    inverse_motion_turning_agent,
    composite_agent,
    _BaseAgent,
    AGENT_CONFIG as agent_module_AGENT_CONFIG,
    agent_base_movement_params # Although params[1] is just a default now
)
from math_module import ( AgentKinematics, FRAME_DT, set_frame_dt )
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
        # Draw the food item as a green circle
        pygame.draw.circle(surf, (0, 255, 0), (int(self.x), int(self.y)), self.r)

class SimAgent:
    """Represents a single agent in the simulation."""
    __slots__ = ("ctrl", "body", "color", "agent_type_name")

    def __init__(self, cls_name: str, pos: Tuple[float, float],
                 input_time_scale: int, output_time_scale: int,
                 decay_timer_frames: int, # New: Decay timer max for this agent's controller
                 composite_signal_threshold: float, # New: Composite threshold if this is a composite agent
                 *, # Forces remaining args to be keyword-only
                 initial_direction_deg: float = 0.0,
                 base_turn_deg_dt: float = cfg.AGENT_DEFAULT_TURN_RATE_DEG_PER_DT, # Max turn rate for kinematics and controller
                 agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
                 layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None, # New: Layer configs for composite
                 agent_params: Optional[Dict[str, Any]] = None # New: specific params for non-composite agent
                ):

        self.agent_type_name = cls_name

        # Create the controller instance based on the agent type name
        # Pass common controller params (time scales, decay, turn rate cap)
        # and type-specific/composite params (agent_params, layer_configs, composite_threshold)
        self.ctrl: _BaseAgent | composite_agent = self._create_controller(
            cls_name=cls_name,
            input_time_scale=input_time_scale,
            output_time_scale=output_time_scale,
            decay_timer_frames=decay_timer_frames,
            composite_signal_threshold=composite_signal_threshold, # Passed to composite if applicable
            initial_base_turn_rate_deg_dt=base_turn_deg_dt, # Max turn rate passed to controller for internal use/scaling
            layer_configs=layer_configs_for_composite,     # Passed to composite if applicable
            **agent_params if agent_params is not None else {} # Pass type-specific params
        )

        # Create the kinematics body
        self.body = AgentKinematics(
            initial_position=pos,
            initial_direction_deg=initial_direction_deg,
            dt=FRAME_DT,
            constant_speed_pixels_sec=agent_constant_speed,
            max_turn_rate_deg_per_dt=base_turn_deg_dt # Max turn rate for kinematics
        )

        # Assign a random color for visual differentiation
        self.color = (random.randint(100, 255), random.randint(50, 150), random.randint(50, 150))

    def _create_controller(self, cls_name: str,
                           input_time_scale: int, output_time_scale: int,
                           decay_timer_frames: int, composite_signal_threshold: float,
                           **kwargs: Any) -> _BaseAgent | composite_agent:
        """Helper to instantiate the correct controller class."""

        agent_class_constructor = AGENT_CLASSES.get(cls_name)

        if agent_class_constructor is None:
             print(f"Warning: Unknown agent type '{cls_name}'. Falling back to stochastic.")
             agent_class_constructor = stochastic_motion_random_movement_agent
             cls_name = "stochastic" # Update name in case it matters later

        # Base parameters common to all controllers
        constructor_params = {
            "input_time_scale": input_time_scale,
            "output_time_scale": output_time_scale,
            "decay_timer_frames": decay_timer_frames, # Pass decay setting
            **kwargs # Includes initial_base_turn_rate_deg_dt and agent_params
        }

        # Add composite-specific parameters if creating a composite agent
        if cls_name == "composite":
             constructor_params["composite_signal_threshold"] = composite_signal_threshold
             # layer_configs is already in kwargs if provided

             # If layer_configs were not provided or were empty, use a default layer
             if "layer_configs" not in kwargs or not kwargs["layer_configs"]:
                 print("Info: Composite agent configured with no layers or empty layer config. Adding default stochastic layer.")
                 constructor_params["layer_configs"] = [{
                     "agent_type_name": "stochastic",
                     "num_instances": 1,
                     "input_time_scale": agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                     "output_time_scale": agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
                     "decay_timer_frames": agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"], # Default layer decay
                     "weight": 1.0, # Default layer weight
                     "agent_params": {}
                 }]

        try:
            # Instantiate the agent controller
            return agent_class_constructor(**constructor_params)
        except Exception as e:
            # Catch errors during instantiation (e.g., missing expected param)
            print(f"Error creating controller for '{cls_name}' with params {constructor_params}: {e}")
            import traceback; traceback.print_exc()
            print("Falling back to default stochastic agent.")
            # Fallback to a basic stochastic agent
            return stochastic_motion_random_movement_agent(
                input_time_scale=agent_module_AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"],
                output_time_scale=agent_module_AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"],
                decay_timer_frames=agent_module_AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"],
                initial_base_turn_rate_deg_dt=kwargs.get("initial_base_turn_rate_deg_dt", cfg.AGENT_DEFAULT_TURN_RATE_DEG_PER_DT)
            )

    def step(self, food_signal: int):
        """Runs one simulation step for the agent."""
        # Get the normalized turn command from the controller
        turn_command_normalized, _state, _decision_outputs = self.ctrl.update(food_signal)

        # Update the agent's kinematics (position and direction)
        # Thrust is currently always 1.0 (constant speed movement)
        self.body.update(thrust_input_normalized=1.0, turn_command_normalized=float(turn_command_normalized))

        # Wrap agent position around screen boundaries
        self._wrap_position()

    def _wrap_position(self):
        """Wraps the agent's position around the screen edges."""
        # Check and wrap X coordinate
        if self.body.position.x < -cfg.AGENT_RADIUS:
            self.body.position.x = cfg.WINDOW_W + cfg.AGENT_RADIUS
        elif self.body.position.x > cfg.WINDOW_W + cfg.AGENT_RADIUS:
            self.body.position.x = -cfg.AGENT_RADIUS

        # Check and wrap Y coordinate
        if self.body.position.y < -cfg.AGENT_RADIUS:
            self.body.position.y = cfg.WINDOW_H + cfg.AGENT_RADIUS
        elif self.body.position.y > cfg.WINDOW_H + cfg.AGENT_RADIUS:
            self.body.position.y = -cfg.AGENT_RADIUS

    def draw(self, surf: pygame.Surface):
        """Draws the agent on the given surface."""
        pos_x, pos_y = int(self.body.position.x), int(self.body.position.y)

        # Draw the agent body (circle)
        pygame.draw.circle(surf, self.color, (pos_x, pos_y), cfg.AGENT_RADIUS)

        # Draw a line indicating direction
        line_len = cfg.AGENT_RADIUS * 1.8
        end_x = pos_x + line_len * math.cos(self.body.direction_rad)
        end_y = pos_y + line_len * math.sin(self.body.direction_rad)
        pygame.draw.line(surf, (200, 200, 255), (pos_x, pos_y), (int(end_x), int(end_y)), 2)

# --- World and Simulation State ---

# Global instance of the simulation world
WORLD = None

# Global simulation statistics
_food_eaten_current_run = 0
_total_frames_simulated_current_run = 0

class World:
    """Manages the simulation environment (food, agents, drawing)."""
    def __init__(self):
        self.food: List[Food] = []
        self.agents: List[SimAgent] = []
        self.surface: pygame.Surface | None = None # Pygame surface for drawing

    def reset(self, *, food_preset: str = "three_lines_dense",
                  agent_type: str = "stochastic", n_agents: int = 1,
                  spawn_point: Tuple[float, float] | None = None,
                  initial_direction_deg: float, base_turn_deg_dt: float,
                  input_time_scale: int, output_time_scale: int,
                  decay_timer_frames: int, # New: Decay timer for top-level agent(s)
                  composite_signal_threshold: float, # New: Composite threshold
                  layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None, # New: Composite layer configs
                  agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
                  agent_params: Optional[Dict[str, Any]] = None # New: specific params for non-composite agent
                  ):
        """Resets the world state, populating food and creating agents."""
        global _food_eaten_current_run, _total_frames_simulated_current_run
        _food_eaten_current_run = 0
        _total_frames_simulated_current_run = 0

        # Populate food based on the selected preset
        food_coords_generator = cfg.FOOD_PRESETS.get(food_preset)
        if food_coords_generator:
            self.food = [Food(p) for p in food_coords_generator()]
        else:
            print(f"Warning: Unknown food preset '{food_preset}'. Using 'void_food'.")
            self.food = [Food(p) for p in cfg.FOOD_PRESETS["void"]()]

        # Create agents
        self.agents = []
        # Use provided spawn point or default to center
        base_spawn_x, base_spawn_y = spawn_point if spawn_point else (cfg.WINDOW_W / 2, cfg.WINDOW_H / 2)

        # Prepare controller specific kwargs based on the main agent type selected
        controller_kwargs_for_sim_agent: Dict[str, Any] = {}

        # Pass specific parameters if provided (for non-composite agents)
        if agent_params is not None:
            controller_kwargs_for_sim_agent.update(agent_params)

        # Pass composite specific configurations if agent type is composite
        if agent_type == "composite" and layer_configs_for_composite is not None:
            controller_kwargs_for_sim_agent["layer_configs"] = layer_configs_for_composite
            # composite_signal_threshold is passed directly to SimAgent which passes to controller

        # Create multiple agents if requested
        for i in range(n_agents):
            # Calculate spawn position with horizontal offset for multiple agents
            offset_x = (i - (n_agents - 1) / 2) * (cfg.AGENT_RADIUS * 5) if n_agents > 1 else 0
            current_spawn_x = max(cfg.AGENT_RADIUS, min(base_spawn_x + offset_x, cfg.WINDOW_W - cfg.AGENT_RADIUS))
            current_spawn_y = max(cfg.AGENT_RADIUS, min(base_spawn_y, cfg.WINDOW_H - cfg.AGENT_RADIUS))

            self.agents.append(SimAgent(
                cls_name=agent_type,
                pos=(current_spawn_x, current_spawn_y),
                input_time_scale=input_time_scale,
                output_time_scale=output_time_scale,
                decay_timer_frames=decay_timer_frames, # Pass decay setting
                composite_signal_threshold=composite_signal_threshold, # Pass composite threshold
                initial_direction_deg=initial_direction_deg,
                base_turn_deg_dt=base_turn_deg_dt, # Pass max turn rate
                agent_constant_speed=agent_constant_speed,
                layer_configs_for_composite=layer_configs_for_composite, # Pass composite layers
                agent_params=agent_params # Pass other specific agent params
            ))

    def step(self):
        """Advances the simulation by one frame."""
        global _food_eaten_current_run, _total_frames_simulated_current_run
        _total_frames_simulated_current_run += 1 # Increment total frame count

        for ag in self.agents:
            food_eaten_this_step = 0
            # Check for collisions with food, iterating backwards to allow safe removal
            for f_idx in range(len(self.food) - 1, -1, -1):
                f = self.food[f_idx]
                # Calculate distance between agent and food center
                dx = ag.body.position.x - f.x
                dy = ag.body.position.y - f.y
                distance_sq = dx**2 + dy**2 # Use squared distance for performance
                combined_radii = cfg.FOOD_RADIUS + cfg.AGENT_RADIUS

                # If distance is less than sum of radii, a collision occurs
                if distance_sq <= combined_radii**2:
                    self.food.pop(f_idx) # Remove the eaten food item
                    food_eaten_this_step = 1 # Signal that food was eaten this step
                    _food_eaten_current_run += 1 # Increment global food eaten count
                    # Assuming an agent can only eat one food item per step, break after finding one
                    break

            # Step the agent's controller and kinematics
            ag.step(food_signal=food_eaten_this_step)

            # Agent position wrapping is now handled inside ag.step() -> ag._wrap_position()

    def draw(self, surf: pygame.Surface):
        """Draws all simulation elements on the surface."""
        # Fill background
        surf.fill((30, 30, 30))
        # Draw all food items
        for f in self.food: f.draw(surf)
        # Draw all agents
        for ag in self.agents: ag.draw(surf)

# --- Simulation Control Functions ---

WORLD = World()

def init_pygame_surface(surface: pygame.Surface | None):
    """Sets the Pygame surface for drawing (or None for headless)."""
    if WORLD:
        WORLD.surface = surface

def configure(*, food_preset: str, agent_type: str, n_agents: int,
              spawn_point: Tuple[float, float], initial_orientation_deg: float,
              base_turn_deg_dt: float, input_time_scale: int, output_time_scale: int,
              decay_timer_frames: int, # New
              composite_signal_threshold: float, # New
              layer_configs_for_composite: Sequence[Dict[str, Any]] | None = None, # New
              agent_constant_speed: float = cfg.AGENT_CONSTANT_SPEED,
              agent_params_for_main_agent: Optional[Dict[str, Any]] = None # New
              ):
    """Configures the simulation world with specified parameters."""
    if WORLD:
        WORLD.reset(
            food_preset=food_preset,
            agent_type=agent_type,
            n_agents=n_agents,
            spawn_point=spawn_point,
            initial_direction_deg=initial_orientation_deg,
            base_turn_deg_dt=base_turn_deg_dt,
            input_time_scale=input_time_scale,
            output_time_scale=output_time_scale,
            decay_timer_frames=decay_timer_frames, # Pass decay
            composite_signal_threshold=composite_signal_threshold, # Pass threshold
            layer_configs_for_composite=layer_configs_for_composite, # Pass composite layers
            agent_constant_speed=agent_constant_speed,
            agent_params=agent_params_for_main_agent # Pass specific params
        )

def run_frame(is_visual: bool = True):
    """Runs one frame of the simulation."""
    if WORLD:
        WORLD.step() # Update simulation state
        if is_visual and WORLD.surface is not None:
            WORLD.draw(WORLD.surface) # Draw if visual mode and surface is available

def get_simulation_stats() -> dict:
    """Returns current simulation statistics."""
    if WORLD:
        return {
            "total_food": len(WORLD.food),
            "active_agents": len(WORLD.agents),
            "food_eaten_run": _food_eaten_current_run,
            "total_frames_simulated_run": _total_frames_simulated_current_run,
            "current_frame_dt": FRAME_DT # Get current FRAME_DT from math_module
        }
    # Return default 'N/A' stats if world is not initialized
    return {
        "total_food":"N/A",
        "active_agents":"N/A",
        "food_eaten_run":"N/A",
        "total_frames_simulated_run":"N/A",
        "current_frame_dt":"N/A"
    }

# If this script is run directly, start the GUI
if __name__ == "__main__":
    try:
        # Import SimGUI and start the application
        from GUI import SimGUI
        app = SimGUI()
        app.root.mainloop()
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