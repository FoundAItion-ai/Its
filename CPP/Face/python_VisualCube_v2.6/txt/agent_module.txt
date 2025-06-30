# --- agent_module.py (Modified) ---
from __future__ import annotations
import math
import random
from typing import List, Tuple, Any, Dict, Sequence, Type, Optional

# --- Global Agent Configuration ---
AGENT_CONFIG: Dict[str, Any] = {
    "INPUT_TIME_SCALE_FALLBACK": 30,
    "OUTPUT_TIME_SCALE_FALLBACK": 30,
    "DEFAULT_FREQUENCY_THRESHOLD": 10,
    "DEFAULT_DECAY_TIMER_FRAMES": 60, # New: Default frames for decay timer max
    "DEFAULT_COMPOSITE_SIGNAL_THRESHOLD": 0.5, # New: Default threshold for composite signal sum magnitude
}

agent_base_movement_params: List[float] = [1.0, 5.0] # [Speed (unused directly now), Base Turn Rate]

class _BaseAgent:
    def __init__(self,
                 input_time_scale: int, # How often agent considers input
                 output_time_scale: int, # How often agent makes a primary "decision" or output command
                 decay_timer_frames: int, # How many frames until signal fully decays (approx)
                 **kwargs):

        self.input_time_scale = input_time_scale if input_time_scale > 0 else 1
        self.output_time_scale = output_time_scale if output_time_scale > 0 else 1
        self.base_turn_rate_deg_dt: float = float(kwargs.get("initial_base_turn_rate_deg_dt", agent_base_movement_params[1]))

        # --- New Signal/Decay State ---
        self._decay_max_frames = max(1, decay_timer_frames) # Avoid division by zero, ensure positive
        self._decay_timer_frames: int = 0 # Counts frames since last signal fired
        # _current_turn_direction: -1.0 for left bias, 1.0 for right bias
        self._current_turn_direction: float = random.choice([-1.0, 1.0])
        self._cached_normalized_turn_output: float = 0.0

        self._process_kwargs(kwargs)

        # Initialize output based on initial state
        self._update_decay_and_output()

    def _process_kwargs(self, kwargs):
        # Specific agents can override to process their own kwargs
        pass

    def _flip_direction_and_reset_decay(self):
        """Flips the intended turn direction and resets the decay timer."""
        self._current_turn_direction *= -1.0
        self._decay_timer_frames = 0
        # Recalculate output immediately based on new state
        self._update_decay_and_output()

    def _update_decay_and_output(self):
        """Updates the decay timer and calculates the current normalized output command."""
        # Decay factor decreases from 1.0 towards 0 as decay_timer_frames increases
        # Function: max_frames / (timer + max_frames)
        # At timer=0, factor = max/max = 1
        # At timer=max, factor = max/(max+max) = 0.5
        # As timer -> inf, factor -> 0
        decay_factor = self._decay_max_frames / (self._decay_timer_frames + self._decay_max_frames)

        # The magnitude of the turn command is based on the decay factor
        current_magnitude = decay_factor

        # The final normalized command is the magnitude modulated by the direction
        self._cached_normalized_turn_output = self._current_turn_direction * current_magnitude

        # Clamp to [-1.0, 1.0] just in case (shouldn't exceed with this decay formula)
        self._cached_normalized_turn_output = max(-1.0, min(1.0, self._cached_normalized_turn_output))

    def _check_and_fire_signal(self, food_signal: int):
        """Specific agent logic to decide IF a signal fires this frame."""
        # This method should be overridden by subclasses.
        # If a signal fires, it should call self._flip_direction_and_reset_decay()
        pass

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        """
        Updates the agent's state based on food signal and time.
        Returns: (normalized_turn_command, state_string, decision_output_vector)
        decision_output_vector is (thrust_normalized, ??) - keeping for structure, thrust is always 1.0
        """
        # Process input and check if a signal should fire
        self._check_and_fire_signal(food_signal)

        # Increment decay timer every frame
        self._decay_timer_frames += 1

        # Recalculate the output based on the updated decay timer
        self._update_decay_and_output()

        # Return the cached output and state info
        # State string can be customized by subclasses if needed
        return self._cached_normalized_turn_output, "base_agent", (1.0, 0.0)

_ATOMIC_AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {}
def _register_atomic_agent(name: str, cls: Type[_BaseAgent]): _ATOMIC_AGENT_CLASSES[name] = cls

class stochastic_motion_random_movement_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, decay_timer_frames: int, **kwargs):
        # Use output_time_scale for how often to consider random decision
        super().__init__(input_time_scale, output_time_scale, decay_timer_frames, **kwargs)
        self.frames_since_last_decision = 0

    def _check_and_fire_signal(self, food_signal: int):
        self.frames_since_last_decision += 1
        if self.frames_since_last_decision >= self.output_time_scale:
            self.frames_since_last_decision = 0
            # 50% chance to flip direction and reset decay
            if random.random() < 0.5:
                self._flip_direction_and_reset_decay()

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        # Call base update which handles signal check, decay increment, and output calculation
        normalized_turn_cmd, _, decision_outputs = super().update(food_signal)

        # Custom state string (optional)
        state_str = f"stoch(dir:{self._current_turn_direction:.1f}, dec:{self._decay_timer_frames}/{self._decay_max_frames})"

        return normalized_turn_cmd, state_str, decision_outputs

_register_atomic_agent("stochastic", stochastic_motion_random_movement_agent)

class inverse_motion_turning_agent(_BaseAgent):
    def __init__(self, input_time_scale: int, output_time_scale: int, decay_timer_frames: int, **kwargs):
        super().__init__(input_time_scale, output_time_scale, decay_timer_frames, **kwargs)
        # Frequency threshold specifically for *this* agent type
        self.frequency_threshold = int(kwargs.get("frequency_threshold", AGENT_CONFIG["DEFAULT_FREQUENCY_THRESHOLD"]))
        if self.frequency_threshold < 1:
            self.frequency_threshold = 1

        self.positive_signal_count: int = 0 # Counts food signals received since last flip

    def _check_and_fire_signal(self, food_signal: int):
        # Process input signal every frame
        if food_signal > 0:
            self.positive_signal_count += 1

        # Check if the accumulated signal reaches the threshold
        if self.positive_signal_count >= self.frequency_threshold:
            self._flip_direction_and_reset_decay() # Flip direction and reset decay
            self.positive_signal_count = 0 # Reset food signal counter

    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        # Call base update which handles signal check, decay increment, and output calculation
        normalized_turn_cmd, _, decision_outputs = super().update(food_signal)

        # Custom state string
        state_str = f"inv_turn(dir:{self._current_turn_direction:.1f}, sig:{self.positive_signal_count}/{self.frequency_threshold}, dec:{self._decay_timer_frames}/{self._decay_max_frames})"

        return normalized_turn_cmd, state_str, decision_outputs

_register_atomic_agent("inverse_turn", inverse_motion_turning_agent)

class composite_agent(_BaseAgent):
    def __init__(self,
                 layer_configs: Sequence[Dict[str, Any]],
                 input_time_scale: int, # Not directly used for aggregation logic, but required by _BaseAgent
                 output_time_scale: int, # This is the composite's aggregation frequency (how often to check total signal)
                 decay_timer_frames: int, # Composite's own decay timer max
                 composite_signal_threshold: float, # New: Threshold for summed signal magnitude
                 **kwargs):

        # The `initial_base_turn_rate_deg_dt` passed via kwargs is the composite agent's own max turn rate.
        super().__init__(input_time_scale, output_time_scale, decay_timer_frames, **kwargs)

        self._internal_agents: List[_BaseAgent] = []
        self._layer_weights: List[float] = [] # New: Weights for each internal agent's contribution
        self._composite_signal_threshold: float = max(0.0, composite_signal_threshold) # Ensure non-negative threshold

        if not layer_configs: print("Warning: Composite_agent initialized with no layer_configs.")

        # Initialize internal agents and weights based on layer_configs
        for i, layer_conf in enumerate(layer_configs):
            agent_type_name = layer_conf.get("agent_type_name")
            num_instances = layer_conf.get("num_instances", 0)

            # Internal agents have their OWN time scales and decay timers
            # This allows layers to operate at different frequencies and have different decay profiles
            layer_input_ts = layer_conf.get("input_time_scale", AGENT_CONFIG["INPUT_TIME_SCALE_FALLBACK"])
            layer_output_ts = layer_conf.get("output_time_scale", AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"])
            layer_decay_frames = layer_conf.get("decay_timer_frames", AGENT_CONFIG["DEFAULT_DECAY_TIMER_FRAMES"]) # Layers have their own decay setting
            layer_weight = layer_conf.get("weight", 1.0) # New: Weight for this layer type

            agent_params = layer_conf.get("agent_params", {})

            # Each instance in the layer gets the same config and weight
            if agent_type_name and agent_type_name in _ATOMIC_AGENT_CLASSES and num_instances > 0:
                agent_class_to_instantiate = _ATOMIC_AGENT_CLASSES[agent_type_name]
                for _ in range(num_instances):
                    instance_specific_kwargs = {
                        **agent_params,
                        # Pass the composite's max turn rate? Or layer specific?
                        # Let's let layers use their *own* base_turn_rate if specified, default to the composite's.
                        # This allows fine-tuning layer influence beyond just weight.
                        # Or simplify and let weights handle influence? Let's simplify: Weights handle influence.
                        # The layer's turn rate is irrelevant to the composite aggregation, only its normalized output matters.
                        # So, don't pass 'initial_base_turn_rate_deg_dt' from composite down.
                        # It was already removed from layer_config in GUI logic (see GUI thought block).
                    }
                    agent = agent_class_to_instantiate(
                        input_time_scale=layer_input_ts,
                        output_time_scale=layer_output_ts,
                        decay_timer_frames=layer_decay_frames, # Each layer instance has its own decay
                        **instance_specific_kwargs
                    )
                    self._internal_agents.append(agent)
                    self._layer_weights.append(max(0.0, layer_weight)) # Store weight per instance

        self.total_internal_agents = len(self._internal_agents)
        # The latest *normalized decayed turn command [-1,1]* from each internal agent.
        self._latest_normalized_turn_from_internal: List[float] = [0.0] * self.total_internal_agents

        # --- Composite's Own Aggregation State ---
        self.frames_since_last_aggregation_check = 0 # Uses composite's own output_time_scale
        # Composite's own decay timer and direction are managed by _BaseAgent

        # Initialize composite's state
        if not self._internal_agents: self._cached_state_str = "composite_empty"
        else: self._cached_state_str = "composite_idle"

    def _check_and_fire_signal(self, food_signal: int):
        """Composite logic: Check if the sum of weighted internal signals exceeds the threshold."""
        if not self._internal_agents: return

        # Composite checks its aggregation criteria based on its own output_time_scale
        self.frames_since_last_aggregation_check += 1
        if self.frames_since_last_aggregation_check >= self.output_time_scale:
            self.frames_since_last_aggregation_check = 0

            sum_of_weighted_decayed_commands = 0.0
            num_active_outputs = 0 # Count agents that actually outputted a command (all do now)

            # Sum the latest *decayed* normalized commands from internal agents, applying weights
            for agent_idx, latest_norm_cmd in enumerate(self._latest_normalized_turn_from_internal):
                 if agent_idx < len(self._layer_weights): # Ensure weight exists (should always if lists match)
                     sum_of_weighted_decayed_commands += latest_norm_cmd * self._layer_weights[agent_idx]
                     num_active_outputs += 1 # All contribute their latest decayed state

            # If the magnitude of the summed signal exceeds the composite's threshold
            if abs(sum_of_weighted_decayed_commands) >= self._composite_signal_threshold:
                 # The composite agent flips its OWN direction and resets its OWN decay timer
                 self._flip_direction_and_reset_decay()
                 self._cached_state_str = f"comp_flip({num_active_outputs}/{self.total_internal_agents}, sum:{sum_of_weighted_decayed_commands:.2f})"
            else:
                 self._cached_state_str = f"comp_agg({num_active_outputs}/{self.total_internal_agents}, sum:{sum_of_weighted_decayed_commands:.2f})"


    def update(self, food_signal: int) -> Tuple[float, Any, Tuple[float, float]]:
        """
        Updates composite agent. Internal agents update every frame.
        Composite aggregates based on its own output_time_scale and applies its own decay.
        """
        if not self._internal_agents:
            # Composite's own decay timer still increments even if empty? No, let's halt if empty.
            return 0.0, self._cached_state_str, (1.0, 0.0)

        # 1. Update all internal agents EVERY FRAME and store their latest normalized decayed command.
        for agent_idx, agent_instance in enumerate(self._internal_agents):
            normalized_turn_cmd, _, _ = agent_instance.update(food_signal) # Internal agents handle their OWN decay
            if agent_idx < len(self._latest_normalized_turn_from_internal):
                self._latest_normalized_turn_from_internal[agent_idx] = normalized_turn_cmd
            else: # Should not happen if initialized correctly, but safety check
                 self._latest_normalized_turn_from_internal.append(normalized_turn_cmd)
                 self._layer_weights.append(1.0) # Add default weight if mismatched

        # 2. Check composite aggregation criteria and potentially fire composite signal
        self._check_and_fire_signal(food_signal) # This handles composite flipping and decay reset

        # 3. Increment composite's OWN decay timer and calculate composite's final output
        # This part is handled by the base class update method after _check_and_fire_signal completes
        normalized_turn_cmd, _, decision_outputs = super().update(food_signal)

        # The state string was set in _check_and_fire_signal
        return normalized_turn_cmd, self._cached_state_str, decision_outputs

_register_atomic_agent("composite", composite_agent)

# Update agent_base_movement_params description for clarity with new model
# agent_base_movement_params[1] is now the DEFAULT base turn rate used if not specified
# The SimAgent takes base_turn_deg_dt, which comes from GUI setting (defaulting to this param)
# This base_turn_deg_dt is set as agent.body.max_turn_rate_rad_per_dt (converted)
# and as agent.ctrl.base_turn_rate_deg_dt
# The controller outputs [-1, 1], which gets multiplied by agent.body.max_turn_rate_rad_per_dt
