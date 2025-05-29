# --- agent_module.py (Modified) ---
from __future__ import annotations
import math
import random
from typing import List, Tuple, Any, Dict, Sequence, Type

# --- Global Agent Configuration ---
AGENT_CONFIG: Dict[str, Any] = {
    "CONSTANT_THRESHOLD": 0.5,
    "ACCEL_STEP": 0.05,
    "DECEL_STEP": 0.05,
    "INPUT_TIME_SCALE_FALLBACK": 30,
    "OUTPUT_TIME_SCALE_FALLBACK": 30,
}

# --- Global Agent Base Movement Parameters ---
agent_base_movement_params: List[float] = [0.5, 5.0] # [Thrust_norm, Turn_Rate_deg/dt]

def set_parameters(**kwargs):
    for key, value in kwargs.items():
        if key in AGENT_CONFIG:
            AGENT_CONFIG[key] = value

# --- Base Agent Class (Mostly Unchanged) ---
class _BaseAgent:
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        self.input_time_scale = input_time_scale
        self.output_time_scale = output_time_scale
        self.current_target_thrust_normalized: float = agent_base_movement_params[0]
        self.movement: List[float] = [
            self.current_target_thrust_normalized,
            float(agent_base_movement_params[1])
        ]
        self._process_kwargs(kwargs)

    def _process_kwargs(self, kwargs):
        self.constant_threshold = kwargs.get("constant_threshold", AGENT_CONFIG["CONSTANT_THRESHOLD"])
        if 'max_speed_cap' in kwargs:
            self.max_speed_cap = kwargs['max_speed_cap']
        self.accel_step = kwargs.get("accel_step", AGENT_CONFIG["ACCEL_STEP"])
        self.decel_step = kwargs.get("decel_step", AGENT_CONFIG["DECEL_STEP"])


    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        """
        Returns: (turn_command, current_state, (delta_thrust_normalized, delta_base_turn_rate_deg_dt))
        """
        raise NotImplementedError

# --- For Composite Agent Internal Use ---
_ATOMIC_AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {}

def _register_atomic_agent(name: str, cls: Type[_BaseAgent]):
    _ATOMIC_AGENT_CLASSES[name] = cls

# --- Atomic Agent Implementations ---

class stochastic_motion_random_movement_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        super().__init__(input_time_scale, output_time_scale, **kwargs)
        self.frames_since_last_decision = 0
        self._cached_turn_command = 0.0
        self._cached_delta_thrust = 0.0
        self._cached_delta_turn_rate = 0.0

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        self.frames_since_last_decision += 1
        if self.frames_since_last_decision >= self.output_time_scale:
            self.frames_since_last_decision = 0
            self._cached_turn_command = random.uniform(-1, 1)
            if food_signal > 0:
                self._cached_delta_thrust = self.accel_step
            else:
                self._cached_delta_thrust = -self.decel_step if random.random() < 0.2 else random.uniform(-0.02, 0.02)
            self._cached_delta_turn_rate = random.uniform(-0.5, 0.5) if random.random() < 0.05 else 0.0
        return self._cached_turn_command, "stochastic", (self._cached_delta_thrust, self._cached_delta_turn_rate)
_register_atomic_agent("stochastic", stochastic_motion_random_movement_agent)

class inverse_motion_turning_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        super().__init__(input_time_scale, output_time_scale, **kwargs)
        self.preferred_turn_direction = kwargs.get("initial_turn_preference", 1 if random.random() < 0.5 else -1)
        self.frames_since_last_decision = 0
        self._cached_turn_command = 0.0
        self._cached_delta_thrust = 0.0
        self._cached_delta_turn_rate = kwargs.get("fixed_delta_turn_rate", 0.0)


    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        self.frames_since_last_decision += 1
        if self.frames_since_last_decision >= self.output_time_scale:
            self.frames_since_last_decision = 0
            if food_signal > 0 and food_signal >= self.constant_threshold:
                self._cached_turn_command = float(self.preferred_turn_direction)
                self._cached_delta_thrust = self.accel_step
                self.preferred_turn_direction *= -1
            else:
                self._cached_delta_thrust = -self.decel_step * 0.3
                self._cached_turn_command = random.uniform(-0.2, 0.2)
        return self._cached_turn_command, "inverse_turn", (self._cached_delta_thrust, self._cached_delta_turn_rate)
_register_atomic_agent("inverse_turn", inverse_motion_turning_agent)

class deceleration_motion_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, **kwargs):
        super().__init__(input_time_scale, output_time_scale, **kwargs)
        self.always_decelerate = kwargs.get("always_decelerate", True)
        self.turn_influence = kwargs.get("turn_influence_on_decel", 0.0)

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        delta_thrust = 0.0
        turn_command = 0.0
        delta_turn_rate = 0.0

        if self.always_decelerate:
            if self.current_target_thrust_normalized > 0:
                delta_thrust = -self.decel_step
            else:
                delta_thrust = 0
        elif food_signal == 0 :
             if self.current_target_thrust_normalized > 0:
                delta_thrust = -self.decel_step
             else:
                delta_thrust = 0

        if self.turn_influence > 0:
            turn_command = random.uniform(-self.turn_influence, self.turn_influence)

        return turn_command, "deceleration", (delta_thrust, delta_turn_rate)
_register_atomic_agent("deceleration", deceleration_motion_agent)

# --- Composite Agent Implementation (Refactored) ---
class composite_agent(_BaseAgent):
    def __init__(self,
                 layer_configs: Sequence[Dict[str, Any]],
                 input_time_scale: int,    # Composite's own input time scale (from _BaseAgent)
                 output_time_scale: int,   # Composite's own aggregation time scale
                 **kwargs):
        # Initialize _BaseAgent with the composite's own time scales and parameters
        super().__init__(input_time_scale, output_time_scale, **kwargs)

        self._internal_agents: List[_BaseAgent] = []
        
        if not layer_configs:
            print("Warning: Composite_agent initialized with no layer_configs. It will be idle.")
        
        for i, layer_conf in enumerate(layer_configs):
            agent_type_name = layer_conf.get("agent_type_name")
            num_instances = layer_conf.get("num_instances", 0)
            layer_input_ts = layer_conf.get("input_time_scale")
            layer_output_ts = layer_conf.get("output_time_scale")
            # agent_params contains layer-specific overrides e.g. {"constant_threshold": 0.x}
            agent_params = layer_conf.get("agent_params", {})

            if not agent_type_name or agent_type_name not in _ATOMIC_AGENT_CLASSES:
                print(f"Warning: Unknown or missing agent_type_name '{agent_type_name}' in layer {i}. Skipping layer.")
                continue
            if num_instances <= 0:
                print(f"Warning: Zero or negative num_instances for '{agent_type_name}' in layer {i}. Skipping layer.")
                continue
            if layer_input_ts is None or layer_output_ts is None: # Should be validated by GUI
                print(f"Warning: Missing input/output time scale for layer {i} ('{agent_type_name}'). Skipping layer.")
                continue

            agent_class_to_instantiate = _ATOMIC_AGENT_CLASSES[agent_type_name]
            for _ in range(num_instances):
                # Combine kwargs passed to composite_agent itself (which might include global defaults)
                # with specific agent_params from the layer. Layer params take precedence.
                instance_specific_kwargs = {**kwargs, **agent_params}
                
                agent = agent_class_to_instantiate(
                    input_time_scale=layer_input_ts,
                    output_time_scale=layer_output_ts,
                    **instance_specific_kwargs # This passes constant_threshold, accel_step etc.
                )
                self._internal_agents.append(agent)
        
        self.total_internal_agents = len(self._internal_agents)

        # frames_since_last_decision from _BaseAgent is not used by composite_agent for its main update logic.
        # Instead, we use its output_time_scale directly.
        self.frames_since_last_aggregation = 0 # Uses self.output_time_scale (composite's aggregation frequency)
        
        # Cached values for the composite agent's aggregated decision
        self._cached_turn_command = 0.0
        self._cached_delta_thrust = 0.0
        self._cached_delta_turn_rate = 0.0
        self._cached_state_str = "composite_idle"
        if not self._internal_agents:
             self._cached_state_str = "composite_empty_or_misconfigured"
        elif self.output_time_scale <= 0: # Prevent division by zero or infinite loop
            print(f"Warning: Composite agent output_time_scale is {self.output_time_scale}. Clamping to 1.")
            self.output_time_scale = 1


    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        if not self._internal_agents:
            return 0.0, self._cached_state_str, (0.0, 0.0)

        self.frames_since_last_aggregation += 1
        
        # The composite agent makes its aggregated decision based on its *own* output_time_scale.
        if self.frames_since_last_aggregation >= self.output_time_scale:
            self.frames_since_last_aggregation = 0
            
            total_turn_command = 0.0
            total_delta_thrust = 0.0
            total_delta_turn_rate = 0.0
            combined_states = []

            for i, agent_instance in enumerate(self._internal_agents):
                # Sync internal agent's state with composite's current *effective* state
                # This is important: layers should react to the composite's overall state.
                agent_instance.current_target_thrust_normalized = self.current_target_thrust_normalized
                agent_instance.movement = list(self.movement) # Pass a copy

                turn_cmd, agent_state, (d_thrust, d_turn_rate) = agent_instance.update(food_signal)
                
                total_turn_command += turn_cmd
                total_delta_thrust += d_thrust
                total_delta_turn_rate += d_turn_rate
                
                try:
                    agent_type_name_short = agent_instance.__class__.__name__.replace("_agent","").replace("_motion","").replace("_random_movement","")[:3].upper()
                except: # Should not happen
                    agent_type_name_short = "UNK"
                combined_states.append(f"L{i//(self.total_internal_agents//len(self.layer_configs) if len(self.layer_configs)>0 else 1)}-{agent_type_name_short}:{agent_state}")


            if self.total_internal_agents > 0:
                avg_turn_command = total_turn_command / self.total_internal_agents
                avg_delta_thrust = total_delta_thrust / self.total_internal_agents
                avg_delta_turn_rate = total_delta_turn_rate / self.total_internal_agents
            else: # Should be caught by the check at the start of update
                avg_turn_command = 0.0
                avg_delta_thrust = 0.0
                avg_delta_turn_rate = 0.0

            self._cached_turn_command = max(-1.0, min(1.0, avg_turn_command))
            self._cached_delta_thrust = avg_delta_thrust
            self._cached_delta_turn_rate = avg_delta_turn_rate
            self._cached_state_str = "|".join(combined_states) if combined_states else "composite_aggregated"
        
        return self._cached_turn_command, self._cached_state_str, (self._cached_delta_thrust, self._cached_delta_turn_rate)