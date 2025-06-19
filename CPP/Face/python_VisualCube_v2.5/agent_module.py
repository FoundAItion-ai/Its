from __future__ import annotations
import math
import random
from typing import List, Tuple, Any, Dict, Sequence, Type, Optional

# --- Global Agent Configuration ---
AGENT_CONFIG: Dict[str, Any] = {
    "INPUT_TIME_SCALE_FALLBACK": 30,
    "OUTPUT_TIME_SCALE_FALLBACK": 30,
    "DEFAULT_STOCHASTIC_FREQ": 1, # Note: This was 10 in your provided snippet, corrected to 0.1 for probability
    "DEFAULT_COMPOSITE_SIGNAL_THRESHOLD": 0.5,
    "STOCHASTIC_BASE_FREQ_HZ": 5.0, # Base frequency for stochastic agent
}

################################################################################
#                               BASE AGENT CLASS                               #
################################################################################
class _BaseAgent:
    """Base class for all agent controllers. Defines the new API."""
    def update(self, food_signal: int, current_frame: int, dt: float) -> Tuple[float, float]:
        """
        Processes signals and returns the desired oar frequencies for the rowboat model.
        
        Args:
            food_signal: 1 if food was eaten this frame, 0 otherwise.
            current_frame: The total number of frames simulated in the current run.
            dt: The time delta for the current frame (e.g., 1/60s).

        Returns:
            A tuple of (left_oar_frequency_hz, right_oar_frequency_hz).
        """
        raise NotImplementedError

################################################################################
#                   REGISTRY FOR ATOMIC (NON‑COMPOSITE) AGENTS                 #
################################################################################
_ATOMIC_AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {}

def _register_atomic_agent(name: str, cls: Type[_BaseAgent]):
    _ATOMIC_AGENT_CLASSES[name] = cls

################################################################################
#                         STOCHASTIC – RANDOM MOVEMENT                        #
################################################################################
class stochastic_motion_random_movement_agent(_BaseAgent):
    """Ignores food and generates randomized oar frequencies."""

    def __init__(self, output_time_scale: int, **kwargs):
        self.output_time_scale = max(1, output_time_scale)
        self.frames_since_decision: int = 0
        
        # Use provided random frequencies as probabilities to generate a stroke
        self.left_rand_prob = float(kwargs.get("left_rand_freq", AGENT_CONFIG["DEFAULT_STOCHASTIC_FREQ"]))
        self.right_rand_prob = float(kwargs.get("right_rand_freq", AGENT_CONFIG["DEFAULT_STOCHASTIC_FREQ"]))

        self.f_left_hz = 0.0
        self.f_right_hz = 0.0

    def update(self, food_signal: int, current_frame: int, dt: float) -> Tuple[float, float]:
        self.frames_since_decision += 1
        if self.frames_since_decision >= self.output_time_scale:
            self.frames_since_decision = 0
            
            # Decide a new randomized frequency for each oar
            base_hz = AGENT_CONFIG["STOCHASTIC_BASE_FREQ_HZ"]
            self.f_left_hz = random.uniform(0, base_hz) * self.left_rand_prob
            self.f_right_hz = random.uniform(0, base_hz) * self.right_rand_prob
            
        return self.f_left_hz, self.f_right_hz

_register_atomic_agent("stochastic", stochastic_motion_random_movement_agent)


################################################################################
#                           INVERSE – CIRCLING AGENT                          #
################################################################################
class inverse_motion_turning_agent(_BaseAgent):
    """
    A frequency-detecting agent that circles and flips direction.
    It self-calibrates a frequency threshold and has two operational layers,
    each with a left and right side frequency.
    """
    def __init__(self, **kwargs):
        # --- State Variables ---
        self._active_side: str = random.choice(["left", "right"])
        self.frame_of_last_food: int = -1
        self.is_calibrated: bool = False
        self.freq_threshold_hz: float = 5.0  # A default, will be overwritten
        self.is_layer2_active: bool = False

        # --- Configuration: Four distinct layer frequencies ---
        self.l1_left_hz = float(kwargs.get("l1_left_hz", 5.0))
        self.l1_right_hz = float(kwargs.get("l1_right_hz", 5.0))
        self.l2_left_hz = float(kwargs.get("l2_left_hz", 10.0))
        self.l2_right_hz = float(kwargs.get("l2_right_hz", 10.0))

    def update(self, food_signal: int, current_frame: int, dt: float) -> Tuple[float, float]:
        # --- Step 1: Update internal state based on food signal ---
        if food_signal > 0:
            if self.frame_of_last_food >= 0:  # We have a previous ping to compare to
                delta_frames = current_frame - self.frame_of_last_food
                if delta_frames > 0 and dt > 0:
                    input_freq_hz = 1.0 / (delta_frames * dt)
                    
                    if not self.is_calibrated:
                        self.freq_threshold_hz = input_freq_hz
                        self.is_calibrated = True
                    
                    # Update layer state based on the calibrated threshold
                    self.is_layer2_active = (input_freq_hz > self.freq_threshold_hz)
            
            # Flip active side for the next food event
            self._active_side = "right" if self._active_side == "left" else "left"
            self.frame_of_last_food = current_frame

        # --- Step 2: Determine output frequencies for THIS frame based on current state ---
        # This part runs every frame, ensuring the agent never stops.
        f_left = 0.0
        f_right = 0.0

        if self._active_side == 'left':
            if self.is_layer2_active:
                f_left = self.l2_left_hz
            else:
                f_left = self.l1_left_hz
        else: # active side is right
            if self.is_layer2_active:
                f_right = self.l2_right_hz
            else:
                f_right = self.l1_right_hz

        return f_left, f_right

_register_atomic_agent("inverse_turn", inverse_motion_turning_agent)


################################################################################
#                           COMPOSITE (AGGREGATOR) - UNCHANGED                #
################################################################################
class composite_agent(_BaseAgent):
    """Aggregates multiple child agents. LOGIC DEFERRED/UNFINISHED."""

    def __init__(self, **kwargs):
        # This agent is unfinished as per the request.
        # It needs a full rewrite to work with the new rowboat model.
        print("Warning: Composite agent is not updated for the new physics model and will not function correctly.")

    def update(self, food_signal: int, current_frame: int, dt: float) -> Tuple[float, float]:
        # Return zero movement as it's not implemented.
        return 0.0, 0.0