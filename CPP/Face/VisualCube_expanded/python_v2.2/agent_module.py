# --- agent_module.py (Modified) ---
from __future__ import annotations
import math 
import random
from typing import List, Tuple, Any, Dict, Sequence, Type, Optional

# --- Global Agent Configuration ---
AGENT_CONFIG: Dict[str, Any] = {
    "INPUT_TIME_SCALE_FALLBACK": 30, # Fallback for non-composite agents, not used by composite's core logic
    "OUTPUT_TIME_SCALE_FALLBACK": 30, # Fallback for all agents if not specified
    "DEFAULT_FREQUENCY_THRESHOLD": 10, 
}

agent_base_movement_params: List[float] = [1.0, 5.0]

class _BaseAgent:
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        # input_time_scale is for how often an agent *processes* its input or makes internal decisions.
        # For composite, this is not directly used for its aggregation logic.
        self.input_time_scale = input_time_scale 
        # output_time_scale is for how often an agent *produces an output/command*.
        # For composite, this is its aggregation frequency.
        self.output_time_scale = output_time_scale if output_time_scale > 0 else 1

        self.current_target_thrust_normalized: float = 1.0 
        self.base_turn_rate_deg_dt: float = float(kwargs.get("initial_base_turn_rate_deg_dt", agent_base_movement_params[1]))
        self.movement: List[float] = [ self.current_target_thrust_normalized, self.base_turn_rate_deg_dt ]
        self._process_kwargs(kwargs)

    def _process_kwargs(self, kwargs): pass

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        raise NotImplementedError

_ATOMIC_AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {}
def _register_atomic_agent(name: str, cls: Type[_BaseAgent]): _ATOMIC_AGENT_CLASSES[name] = cls

class stochastic_motion_random_movement_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        super().__init__(input_time_scale, output_time_scale, **kwargs)
        self.frames_since_last_decision = 0 # Uses its own output_time_scale
        self._cached_turn_command = 0.0 

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        self.frames_since_last_decision += 1
        if self.frames_since_last_decision >= self.output_time_scale: # Use own output_time_scale
            self.frames_since_last_decision = 0
            self._cached_turn_command = random.uniform(-1, 1)
        return self._cached_turn_command, "stochastic", (1.0, 0.0)
_register_atomic_agent("stochastic", stochastic_motion_random_movement_agent)

class inverse_motion_turning_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        super().__init__(input_time_scale, output_time_scale, **kwargs)
        self.frequency_threshold = int(kwargs.get("frequency_threshold", AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]))
        if self.frequency_threshold < 1: 
            self.frequency_threshold = 1 
        
        self.preferred_turn_direction: float = random.choice([-1.0, 1.0]) 
        self.positive_signal_count: int = 0
        # This agent's output_time_scale determines how often it re-evaluates and outputs its turn.
        # The turn itself is continuous based on internal state (preferred_turn_direction).
        self.frames_since_last_output = 0 # Tracks its own output frequency
        self._cached_turn_command = self.preferred_turn_direction # Initial output

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        # Input processing (food signal) happens every frame.
        if food_signal > 0:
            self.positive_signal_count += 1
        
        if self.positive_signal_count >= self.frequency_threshold:
            self.preferred_turn_direction *= -1.0  
            self.positive_signal_count = 0         

        self.frames_since_last_output += 1
        if self.frames_since_last_output >= self.output_time_scale:
            self.frames_since_last_output = 0
            self._cached_turn_command = self.preferred_turn_direction
        
        state_str = f"inv_turn(dir:{self.preferred_turn_direction:.1f}, sig:{self.positive_signal_count}/{self.frequency_threshold})"
        return self._cached_turn_command, state_str, (1.0, 0.0)
_register_atomic_agent("inverse_turn", inverse_motion_turning_agent)

class composite_agent(_BaseAgent):
    def __init__(self,
                 layer_configs: Sequence[Dict[str, Any]],
                 input_time_scale: int, # Not used by composite's core aggregation logic
                 output_time_scale: int, # This is the composite's aggregation frequency
                 **kwargs):
        # The `initial_base_turn_rate_deg_dt` passed via kwargs is the composite agent's own max turn rate.
        super().__init__(input_time_scale, output_time_scale, **kwargs)
        self._internal_agents: List[_BaseAgent] = []
        
        if not layer_configs: print("Warning: Composite_agent initialized with no layer_configs.")
        
        for i, layer_conf in enumerate(layer_configs):
            agent_type_name = layer_conf.get("agent_type_name")
            num_instances = layer_conf.get("num_instances", 0)
            layer_input_ts = layer_conf.get("input_time_scale", AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"]) # Fallback for layer
            layer_output_ts = layer_conf.get("output_time_scale", AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"]) # Fallback for layer
            agent_params = layer_conf.get("agent_params", {})
            layer_base_turn_rate = layer_conf.get("base_turn_rate_deg_dt", agent_base_movement_params[1]) # Default for layer

            if not agent_type_name or agent_type_name not in _ATOMIC_AGENT_CLASSES: continue
            if num_instances <= 0: continue

            agent_class_to_instantiate = _ATOMIC_AGENT_CLASSES[agent_type_name]
            for _ in range(num_instances):
                instance_specific_kwargs = {
                    **agent_params, 
                    "initial_base_turn_rate_deg_dt": layer_base_turn_rate 
                }
                agent = agent_class_to_instantiate(
                    input_time_scale=layer_input_ts, # Layer specific
                    output_time_scale=layer_output_ts, # Layer specific
                    **instance_specific_kwargs
                )
                self._internal_agents.append(agent)
        
        self.total_internal_agents = len(self._internal_agents)
        # Stores the latest *normalized turn command [-1,1]* from each internal agent.
        self._latest_normalized_turn_from_internal: List[float] = [0.0] * self.total_internal_agents 
        
        self.frames_since_last_aggregation = 0 # Uses composite's own output_time_scale
        self._cached_final_normalized_turn_command = 0.0 
        self._cached_state_str = "composite_idle"
        if not self._internal_agents: self._cached_state_str = "composite_empty"


    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        if not self._internal_agents:
            return 0.0, self._cached_state_str, (1.0, 0.0)

        # 1. Update all internal agents and store their latest normalized turn command.
        #    This happens every frame the composite_agent.update() is called.
        #    Internal agents decide to output based on *their own* output_time_scale.
        for agent_idx, agent_instance in enumerate(self._internal_agents):
            normalized_turn_cmd, _, _ = agent_instance.update(food_signal)
            self._latest_normalized_turn_from_internal[agent_idx] = normalized_turn_cmd
            
        # 2. Aggregate these latest commands based on the composite_agent's own output_time_scale.
        self.frames_since_last_aggregation += 1
        if self.frames_since_last_aggregation >= self.output_time_scale: # Composite's aggregation frequency
            self.frames_since_last_aggregation = 0
            
            sum_of_desired_turn_degrees = 0.0
            num_active_internal_outputs = 0 # Count agents that provided a non-stale command (though all are updated now)

            for agent_idx, agent_instance in enumerate(self._internal_agents):
                # Get the latest normalized command stored from this agent
                latest_norm_cmd = self._latest_normalized_turn_from_internal[agent_idx]
                
                # Convert this normalized command to a desired turn in degrees/dt for *this specific internal agent*
                desired_turn_deg_for_agent = latest_norm_cmd * agent_instance.base_turn_rate_deg_dt
                sum_of_desired_turn_degrees += desired_turn_deg_for_agent
                num_active_internal_outputs +=1 # All are considered active as we take their latest

            if num_active_internal_outputs > 0:
                avg_desired_turn_degrees = sum_of_desired_turn_degrees / num_active_internal_outputs
                
                # Now, normalize this average desired turn in degrees based on the
                # composite_agent's *own* base_turn_rate_deg_dt (its maximum turning capability).
                if self.base_turn_rate_deg_dt != 0:
                    self._cached_final_normalized_turn_command = avg_desired_turn_degrees / self.base_turn_rate_deg_dt
                else: 
                    self._cached_final_normalized_turn_command = 0.0
                
                self._cached_final_normalized_turn_command = max(-1.0, min(1.0, self._cached_final_normalized_turn_command))
            else: 
                self._cached_final_normalized_turn_command = 0.0 

            self._cached_state_str = f"comp_agg({num_active_internal_outputs}/{self.total_internal_agents})"
        
        # The composite agent returns its _cached_final_normalized_turn_command.
        # This command persists until the next aggregation.
        return self._cached_final_normalized_turn_command, self._cached_state_str, (1.0, 0.0)