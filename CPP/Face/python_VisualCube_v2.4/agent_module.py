from __future__ import annotations
import math
import random
from typing import List, Tuple, Any, Dict, Sequence, Type, Optional

# --- Global Agent Configuration ---
AGENT_CONFIG: Dict[str, Any] = {
    "INPUT_TIME_SCALE_FALLBACK": 30,   # Back‑compat placeholder
    "OUTPUT_TIME_SCALE_FALLBACK": 30,
    "DEFAULT_LEFT_THRESHOLD": 10,
    "DEFAULT_RIGHT_THRESHOLD": 10,
    "DEFAULT_STOCHASTIC_FREQ": 0.1,
    "DEFAULT_COMPOSITE_SIGNAL_THRESHOLD": 0.5,
}

# [Speed (unused), Base Turn Rate]
agent_base_movement_params: List[float] = [1.0, 5.0]

################################################################################
#                               BASE AGENT CLASS                               #
################################################################################
class _BaseAgent:
    """Common functionality for all atomic (non‑composite) agents."""

    def __init__(
        self,
        input_time_scale: int,   # kept for API compatibility (unused)
        output_time_scale: int,  # decision period in frames
        **kwargs,
    ) -> None:
        # —————————————————————— timing ——————————————————————
        self.output_time_scale = max(1, output_time_scale)
        self.frames_since_decision: int = 0

        # ————————————————— movement —————————————————
        self.base_turn_rate_deg_dt: float = float(
            kwargs.get("initial_base_turn_rate_deg_dt", agent_base_movement_params[1])
        )

        # ————————————————— dual‑signal state —————————————————
        self.left_threshold: int = int(
            kwargs.get("left_threshold", AGENT_CONFIG["DEFAULT_LEFT_THRESHOLD"])
        )
        self.right_threshold: int = int(
            kwargs.get("right_threshold", AGENT_CONFIG["DEFAULT_RIGHT_THRESHOLD"])
        )
        self.left_input_counter: int = 0
        self.right_input_counter: int = 0
        self.left_output_freq: int = 0
        self.right_output_freq: int = 0

        # ————————————————— default turn logic (NEW) —————————————————
        # ‑1 = left, +1 = right, 0 = straight. Each agent instance can supply its own
        # default via **kwargs.  We *do not* overwrite this value on ties, so the agent
        # preserves its last direction unless one side wins.
        self._turn_command: float = float(kwargs.get("default_turn_command", -1.0))

        # Let subclasses process additional kwargs
        self._process_kwargs(kwargs)

    # ---------------------------------------------------------------------
    #                         Overridable hooks
    # ---------------------------------------------------------------------
    def _process_kwargs(self, kwargs):
        """Sub‑classes parse their own keyword arguments here."""
        pass

    def _process_input_signals(self, food_signal: int):
        """Populate the *input* counters for this frame."""
        pass

    def _on_output_fire(self, side: str):
        """Hook called when *output* counter on the given side fires."""
        pass

    # ------------------------------------------------------------------
    #                       Internal mechanics
    # ------------------------------------------------------------------
    def _check_thresholds_and_fire(self):
        """If input counters reached their thresholds, emit an output pulse."""
        if self.left_threshold > 0 and self.left_input_counter >= self.left_threshold:
            self.left_output_freq += 1
            self.left_input_counter = 0
            self._on_output_fire("left")

        if self.right_threshold > 0 and self.right_input_counter >= self.right_threshold:
            self.right_output_freq += 1
            self.right_input_counter = 0
            self._on_output_fire("right")

    def _make_decision(self):
        """Determine the turn direction for the next window."""
        if self.left_output_freq > self.right_output_freq:
            self._turn_command = -1.0  # turn left
        elif self.right_output_freq > self.left_output_freq:
            self._turn_command = 1.0   # turn right
        else:
            # Tie – keep the previous command (persistent default‑turn logic)
            pass

        # Reset for next accumulation window
        self.left_output_freq = 0
        self.right_output_freq = 0

    # ------------------------------------------------------------------
    #                             PUBLIC API
    # ------------------------------------------------------------------
    def update(self, food_signal: int) -> Tuple[float, str, Tuple[float, float]]:
        """Advance one frame and return (turn_cmd, state_str, debug_vec)."""
        # 1) Let subclass gather inputs
        self._process_input_signals(food_signal)

        # 2) Check thresholds → output pulses
        self._check_thresholds_and_fire()

        # 3) Periodically select a steering command
        self.frames_since_decision += 1
        if self.frames_since_decision >= self.output_time_scale:
            self.frames_since_decision = 0
            self._make_decision()

        # 4) Report
        return self._turn_command, "base_agent", (1.0, 0.0)


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
    """Ignores food and generates random internal signals."""

    def _process_kwargs(self, kwargs):
        self.left_rand_freq = float(
            kwargs.get("left_rand_freq", AGENT_CONFIG["DEFAULT_STOCHASTIC_FREQ"])
        )
        self.right_rand_freq = float(
            kwargs.get("right_rand_freq", AGENT_CONFIG["DEFAULT_STOCHASTIC_FREQ"])
        )
        # Honour chosen default turn (falls back to left)
        self._turn_command = float(kwargs.get("default_turn_command", -1.0))

    def _process_input_signals(self, food_signal: int):
        # purely random detections each frame
        if random.random() < self.left_rand_freq:
            self.left_input_counter += 1
        if random.random() < self.right_rand_freq:
            self.right_input_counter += 1

    def update(self, food_signal: int):
        cmd, _, vec = super().update(food_signal)
        state_str = (
            f"stoch(L_in:{self.left_input_counter}/{self.left_threshold} "
            f"R_in:{self.right_input_counter}/{self.right_threshold} | "
            f"L_out:{self.left_output_freq} R_out:{self.right_output_freq})"
        )
        return cmd, state_str, vec

_register_atomic_agent("stochastic", stochastic_motion_random_movement_agent)

################################################################################
#                           INVERSE – CIRCLING AGENT                          #
################################################################################
class inverse_motion_turning_agent(_BaseAgent):
    """Circles until food on the active side flips its direction."""

    def _process_kwargs(self, kwargs):
        # Pick which side is initially *active* (listens for food)
        self._active_side = random.choice(["left", "right"])
        # Set default turn to match that active side (unless caller overrides)
        self._turn_command = float(
            kwargs.get(
                "default_turn_command", -1.0 if self._active_side == "left" else 1.0
            )
        )
        # If caller forced default_turn_command to the *other* side, adjust _active_side
        if (self._turn_command < 0 and self._active_side != "left") or (
            self._turn_command > 0 and self._active_side != "right"
        ):
            self._active_side = "left" if self._turn_command < 0 else "right"

    def _process_input_signals(self, food_signal: int):
        if food_signal > 0:
            if self._active_side == "left":
                self.left_input_counter += 1
            else:
                self.right_input_counter += 1

    def _on_output_fire(self, side: str):
        # Flip focus after a pulse fires
        self._active_side = "right" if side == "left" else "left"

    def update(self, food_signal: int):
        cmd, _, vec = super().update(food_signal)
        active_flag = "L" if self._active_side == "left" else "R"
        state_str = (
            f"inv(act:{active_flag} | L_in:{self.left_input_counter}/{self.left_threshold} "
            f"R_in:{self.right_input_counter}/{self.right_threshold} | "
            f"L_out:{self.left_output_freq} R_out:{self.right_output_freq})"
        )
        return cmd, state_str, vec

_register_atomic_agent("inverse_turn", inverse_motion_turning_agent)

################################################################################
#                           COMPOSITE (AGGREGATOR)                            #
################################################################################
class composite_agent(_BaseAgent):
    """Aggregates multiple child agents and turns based on their consensus."""

    def __init__(
        self,
        layer_configs: Sequence[Dict[str, Any]],
        input_time_scale: int,
        output_time_scale: int,
        composite_signal_threshold: float,
        **kwargs,
    ) -> None:
        # Initialise *our* counters / thresholds first
        super().__init__(input_time_scale, output_time_scale, **kwargs)

        self.composite_signal_threshold: float = max(0.0, composite_signal_threshold)
        self._internal_agents: List[_BaseAgent] = []
        self._layer_weights: List[float] = []
        self._cached_state_str = "composite_idle"

        if not layer_configs:
            print("Warning: composite_agent initialised with no layer_configs.")

        # —— Build internal layers ——
        for conf in layer_configs:
            agent_name = conf.get("agent_type_name")
            num_instances = int(conf.get("num_instances", 0))
            agent_params = conf.get("agent_params", {})
            layer_weight = float(conf.get("weight", 1.0))
            layer_out_ts = int(
                conf.get("output_time_scale", AGENT_CONFIG["OUTPUT_TIME_SCALE_FALLBACK"])
            )

            if agent_name in _ATOMIC_AGENT_CLASSES and num_instances > 0:
                cls = _ATOMIC_AGENT_CLASSES[agent_name]
                for _ in range(num_instances):
                    params = {"output_time_scale": layer_out_ts, **agent_params}
                    self._internal_agents.append(cls(input_time_scale=0, **params))
                    self._layer_weights.append(max(0.0, layer_weight))

    # ------------------------------------------------------------------
    #                 Composite: aggregate child decisions
    # ------------------------------------------------------------------
    def _process_input_signals(self, food_signal: int):
        if not self._internal_agents:
            return

        # 1) Update children → collect their steering commands (−1, 0, 1)
        child_cmds = [ag.update(food_signal)[0] for ag in self._internal_agents]

        # 2) Weighted sum
        weighted = sum(c * w for c, w in zip(child_cmds, self._layer_weights))

        # 3) Feed into our own dual‑signal counters
        if weighted > self.composite_signal_threshold:
            self.right_input_counter += 1
        elif weighted < -self.composite_signal_threshold:
            self.left_input_counter += 1

        self._cached_state_str = (
            f"comp(sum:{weighted:.2f} | L_in:{self.left_input_counter} "
            f"R_in:{self.right_input_counter} | L_out:{self.left_output_freq} "
            f"R_out:{self.right_output_freq})"
        )

    def update(self, food_signal: int):
        if not self._internal_agents:
            return 0.0, "composite_empty", (1.0, 0.0)

        cmd, _, vec = super().update(food_signal)
        return cmd, self._cached_state_str, vec

# NOTE: composite_agent is *not* registered in _ATOMIC_AGENT_CLASSES on purpose
