## `LLM.md`

```markdown
# Agent Simulation: Run and Tumble Model (Technical Overview for LLMs)

This document provides a technical breakdown of the "Agent Simulation: Run and Tumble Model" project, designed for understanding by a large language model. It outlines the project's purpose, architectural components, core concepts, and key interactions between modules.

## 1. Project Purpose

The primary goal of this project is to simulate the "run and tumble" behavior observed in microorganisms (e.g., bacteria chemotaxis) within a 2D environment. Agents navigate, consume food, and dynamically adjust their movement patterns based on environmental cues, specifically their recent food consumption frequency. The system supports visual observation, parameter tuning via a GUI, and extensive data collection through headless simulation runs and logging.

## 2. Core Concepts

*   **Agents:** Autonomous entities exhibiting "run and tumble" like behavior, meaning periods of relatively straight movement ("run") punctuated by reorientation events ("tumble" or "turn").
*   **Food:** Static resources in the environment that agents consume upon collision.
*   **Power System:** Agents generate "right-side" (`P_r`) and "left-side" (`P_l`) power inputs. These inputs are translated into a total forward speed and a differential rotation, mimicking differential drive kinematics.
*   **Environmental Cue (Food Frequency):** Agents track their recent food consumption rate (food items per second) within a sliding time window. This frequency is a critical input for many agent behavioral models.
*   **Simulation Loop:** The core simulation advances in discrete time steps (`FRAME_DELTA_TIME_SECONDS`), updating agent states, handling food consumption, and rendering (in visual mode).

## 3. Module Breakdown

The project is structured into several Python modules, each with distinct responsibilities:

### `config.py`
*   **Role:** Stores static configuration parameters and global constants for the simulation and GUI.
*   **Contents:**
    *   Window dimensions (`WINDOW_W`, `WINDOW_H`).
    *   Visual properties (`AGENT_RADIUS`, `FOOD_RADIUS`).
    *   Core physics constants (`ANGULAR_PROPORTIONALITY_CONSTANT`, `AGENT_SPEED_SCALING_FACTOR`).
    *   Agent behavioral defaults (`DEFAULT_TURN_DECISION_INTERVAL_SEC`).
    *   Global speed modifier (`GLOBAL_SPEED_MODIFIER`).
    *   Functions to dynamically set global configuration values (`set_global_speed_modifier`, `set_agent_speed_scaling_factor`, `set_angular_proportionality_constant`).
    *   Type alias `Coord`.
    *   Food layout preset functions (`_line_y`, `three_lines_food`, `void_food`, `filled_box_food`).
    *   `FOOD_PRESETS` dictionary mapping names to food generation functions.

### `math_module.py`
*   **Role:** Centralizes all mathematical calculations, vector operations, and agent body kinematics. Separates raw physics from behavioral logic.
*   **Contents:**
    *   `FRAME_DELTA_TIME_SECONDS`: The simulation's fixed time step, dynamically adjustable via `set_frame_dt`.
    *   `Vec2D` class: A utility class for 2D vector operations (addition, scalar multiplication, angle conversion).
    *   `calculate_sinusoidal_wave`: Generates a sine wave value given amplitude, period, and time.
    *   `apply_smoothing_filter`: Implements an exponential smoothing filter, used for noise reduction in agent power outputs.
    *   **Core Motion Dynamics:**
        *   `calculate_total_and_differential_power`: Converts raw left/right power inputs into total power and power difference.
        *   `calculate_speed_magnitude`: Determines forward speed from total power using `AGENT_SPEED_SCALING_FACTOR`.
        *   `calculate_saturated_rotation_angle`: Calculates the turn angle (in radians) from the power difference, applying a saturation function (exponential `1 - e^(-abs(angle))`) for realistic turning limits.
        *   `get_motion_outputs_from_power`: A convenience wrapper combining the core motion dynamics to return speed and angle.
    *   `AgentBody` class:
        *   **Role:** Represents an agent's physical state (position, heading).
        *   **Contents:** `position` (Vec2D), `heading_radians`.
        *   `apply_movement`: Updates position and heading based on distance and angular change.
        *   `_wrap_position`: Handles toroidal wrapping of agent position around screen edges.

### `agent_module.py`
*   **Role:** Defines the abstract base class for agents and concrete implementations of different behavioral models.
*   **Contents:**
    *   `SMOOTHING_FACTOR`, `FOOD_FREQ_WINDOW_SEC`: Constants for food frequency calculation.
    *   `_BaseAgent` (Abstract Base Class):
        *   **Role:** Provides common agent functionalities.
        *   **Contents:** `food_timestamps` (deque for tracking eaten food), `current_food_frequency`, `turn_decision_interval_sec`, `last_turn_decision_time`, `decision_count`.
        *   `_update_food_frequency`: Updates `current_food_frequency` based on recent consumption.
        *   `_calculate_power_outputs` (abstract method): Must be implemented by subclasses to determine `P_r` and `P_l`.
        *   `update`: The main agent logic method. Calculates distance and angle for a frame, but only applies a *new* turning decision if `turn_decision_interval_sec` has elapsed. It also supports `is_potential_move` for look-ahead planning.
    *   `StochasticMotionAgent`:
        *   **Role:** Implements a simple agent that randomizes its power outputs periodically.
        *   `_generate_new_power_choice`: Sets new random `P_r` and `P_l`.
        *   `_calculate_power_outputs`: Triggers `_generate_new_power_choice` if interval met.
    *   `InverseMotionAgent`:
        *   **Role:** Implements an agent whose power outputs are modulated by food frequency using sinusoidal waves.
        *   **Contents:** `r_smooth_prev`, `l_smooth_prev` (for smoothing), various thresholds and sinusoidal parameters (`threshold_r`, `r1_period_s`, `r1_amp`, etc.).
        *   `_calculate_power_outputs`: Selects sinusoidal parameters (amplitude, period) based on `current_food_frequency` relative to thresholds, then applies smoothing. Power outputs are squared (`R_smooth**2`, `L_smooth**2`) to ensure positive power for speed calculation.
    *   `CompositeMotionAgent`:
        *   **Role:** Implements a more complex agent with multiple "layers" of sinusoidal control.
        *   **Contents:** `layer_pairs`: A list of dictionaries, where each dictionary defines parameters for a pair of left/right sinusoidal contributions, including individual thresholds.
        *   `_calculate_power_outputs`: Sums the contributions from all active layers (where `current_food_frequency` meets or exceeds the layer's threshold), then applies smoothing. Power outputs are squared.
    *   `AGENT_CLASSES`: A dictionary mapping agent type names (strings) to their respective class types.

### `main.py`
*   **Role:** Orchestrates the entire simulation, manages world state (food, agents), and integrates with Pygame for rendering.
*   **Contents:**
    *   `Food` class: Simple data class for food items (`pos`, `r`).
    *   `SimAgent` class:
        *   **Role:** A wrapper around an `_BaseAgent` controller and an `AgentBody` for simulation management.
        *   **Contents:** `ctrl` (`_BaseAgent` instance), `body` (`AgentBody` instance), `color`, `agent_type_name`, `trace` (for visual trails).
        *   `plan_and_consume`: Executes agent's `update` in two phases: a "potential move" to check for food consumption without altering actual agent state, then a final `update` with the actual food count to get final movement deltas. This ensures food consumption influences the *current frame's* decision.
        *   `apply_planned_move`: Updates `AgentBody` state and trace based on calculated movement.
        *   `_wrap_position`: Calls `AgentBody`'s wrap method and manages trace segmentation.
        *   `draw`: Renders the agent and its heading vector.
    *   `World` class:
        *   **Role:** The central simulation manager.
        *   **Contents:** `food` (list of `Food` objects), `agents` (list of `SimAgent` objects), `surface` (Pygame surface), `total_sim_time`, `frame_count`, `log_file_handle`, `stats_history` (NEW: stores time series data for graphing).
        *   `reset`: Initializes or resets the world with food and agents based on parameters.
        *   `step`: Advances the simulation by one frame. Iterates through agents, calls `plan_and_consume`, updates food list, applies agent moves, and logs data. Records performance stats to `stats_history`.
        *   `draw`: Renders all food and agents (and their traces) to the Pygame surface.
    *   `WORLD`: A global instance of the `World` class.
    *   `_food_eaten_current_run`: Global counter for food eaten.
    *   `init_pygame_surface`: Sets the `WORLD.surface`.
    *   `configure`: A facade function to reset and configure the global `WORLD` instance.
    *   `run_frame`: Executes a single simulation step and optionally draws it.
    *   `get_simulation_stats`: Returns current simulation metrics, including the `history` for graphs.

### `GUI.py`
*   **Role:** Provides the Tkinter-based graphical user interface for interacting with the simulation.
*   **Contents:**
    *   Tkinter `StringVar`, `IntVar`, `DoubleVar` instances to bind UI elements to configuration values.
    *   `SimGUI` class:
        *   **Role:** Manages the entire GUI, user interactions, simulation threading, and visualization.
        *   **UI Elements:** Notebook tabs (Controls, Composite Config, Headless, Status), sliders, spinboxes, comboboxes, buttons, text log, Matplotlib graph area.
        *   `_build_*_tab`: Methods to construct each tab's layout and widgets.
        *   `_on_agent_type_change`: Dynamically updates specific agent parameter controls based on the selected agent type.
        *   `_get_main_agent_specific_params`: Gathers agent-specific parameters from GUI variables.
        *   `_rebuild_composite_gui`, `_add_composite_layer_pair`, `_remove_composite_layer_pair`: Manage the dynamic creation/removal of layers for Composite agents.
        *   `start_simulation`, `stop_simulation`: Control the visual simulation lifecycle, including thread management and Pygame initialization/quitting.
        *   `_simulation_loop_thread`: The thread that runs the Pygame visual simulation, calling `main_run_frame` repeatedly.
        *   `_periodic_status_update`: Updates GUI status labels and the Matplotlib graphs with data from `main.get_simulation_stats()`. Includes snapshotting history lists to avoid race conditions.
        *   `start_headless_runs`, `_headless_run_worker_thread`: Manages batch execution of non-visual simulations, collecting and analyzing results.
        *   `_write_log_header`: Writes initial configuration details to the log file.
        *   `_analyze_log_file`: Parses a selected log file to extract and display performance metrics.
        *   `_import_preset_config`, `_parse_and_apply_preset`: Load configuration settings from a log or preset file.
    *   Matplotlib integration: Uses `Figure`, `FigureCanvasTkAgg` to embed plots for cumulative metrics and consumption rate.

### `requirements.txt`
*   **Role:** Lists all Python package dependencies required for the project (e.g., `pygame`, `matplotlib`).

## 4. Key Interactions and Data Flow

1.  **GUI Initialization:** `GUI.py` creates `SimGUI`, which builds the Tkinter interface.
2.  **Configuration:** User interacts with `GUI.py` to set parameters. When "Start Visual Sim" or "Start Headless Runs" is pressed, `GUI._apply_settings` is called. This function reads all GUI variables and calls `main.configure` to set up the `WORLD` instance and its agents with the chosen parameters, which in turn updates `config.py` and `math_module.py` global variables.
3.  **Simulation Loop (Visual):**
    *   `GUI.start_simulation` launches `_simulation_loop_thread`.
    *   This thread initializes Pygame and continuously calls `main.run_frame(is_visual=True)`.
    *   `main.run_frame` calls `WORLD.step()`.
    *   `WORLD.step()` iterates through `SimAgent`s:
        *   For each `SimAgent`, `plan_and_consume` is called.
        *   `plan_and_consume` calls `SimAgent.ctrl.update` (which is an `_BaseAgent` method).
        *   `_BaseAgent.update` calculates food frequency, calls `_BaseAgent._calculate_power_outputs` (polymorphically invoking agent-specific logic), and then `math_module.get_motion_outputs_from_power` to get raw speed and angle.
        *   `plan_and_consume` then checks for food collisions using `point_segment_dist_sq` and updates `eaten_this_frame`.
        *   `SimAgent.ctrl.update` is called again with the actual `eaten_this_frame` count to finalize the decision for the frame.
        *   `SimAgent.apply_planned_move` updates `SimAgent.body` (`AgentBody` instance) using `AgentBody.apply_movement` and handles `_wrap_position` and trace drawing.
    *   `WORLD.step()` also updates `_food_eaten_current_run` and `WORLD.stats_history`.
    *   `main.run_frame` then calls `WORLD.draw` which renders agents and food to the Pygame surface.
4.  **Simulation Loop (Headless):**
    *   `GUI.start_headless_runs` launches `_headless_run_worker_thread`.
    *   This thread configures the `WORLD` (via `_apply_settings`) but sets `main_init_pygame_surface(None)`.
    *   It then runs `main.run_frame(is_visual=False)` for a specified number of frames. No Pygame drawing occurs.
    *   After each instance, `main.get_simulation_stats` is called to retrieve `food_eaten_run` for performance aggregation.
5.  **Logging:** If enabled, `main.WORLD.log_file_handle` (an open file object provided by `GUI.py`) is used by `WORLD.step()` to write per-frame, per-agent data.
6.  **Status and Graphs:** `GUI._periodic_status_update` is called regularly. It retrieves current stats and the `stats_history` from `main.get_simulation_stats()` and updates the Tkinter labels and Matplotlib plots. Critical handling is in place to snapshot the history lists to prevent race conditions during concurrent access from GUI and simulation threads.

## 5. Key Data Structures and Their Importance

*   `Vec2D`: Fundamental for all spatial calculations, simplifying vector arithmetic.
*   `AgentBody`: Encapsulates the core physical state of an agent, cleanly separating it from behavioral logic.
*   `_BaseAgent` subclasses: Represent the diverse behavioral models, allowing for easy extension and comparison of different agent strategies.
*   `deque` for `food_timestamps`: Efficiently maintains a sliding window of recent food consumption to calculate food frequency.
*   `WORLD.stats_history`: A dictionary of lists, crucial for capturing time-series data for post-run analysis and real-time graph visualization.
*   Agent-specific parameter dictionaries (e.g., `layer_pairs` for Composite): Provide highly flexible and dynamic configuration for complex agent behaviors.

## 6. Design Principles

*   **Separation of Concerns:** Distinct modules for configuration, math, agent logic, world management, and GUI.
*   **Modularity:** New agent types or food presets can be added with minimal changes to existing code.
*   **Configurability:** Extensive parameters exposed through the GUI and log file format allow for detailed experimentation.
*   **Performance:** Headless mode and threading enable efficient execution of multiple simulations for research/analysis.
*   **Real-time Feedback:** Live statistics and graphs provide immediate insights into simulation dynamics.