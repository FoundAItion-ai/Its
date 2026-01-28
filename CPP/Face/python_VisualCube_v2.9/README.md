README.md

# Agent Simulation: Run and Tumble Model

This project implements an agent-based simulation inspired by the "run and tumble" behavior, where agents navigate an environment to find and consume food sources. The simulation is highly configurable, offering various agent behaviors, environmental setups, and both visual and headless execution modes for analysis.

## Table of Contents
1.  [Features](#features)
2.  [Installation](#installation)
3.  [Usage](#usage)
4.  [Agent Types](#agent-types)
5.  [Configuration](#configuration)
6.  [Logging and Analysis](#logging-and-analysis)
7.  [Screenshots](#screenshots) (Placeholder)
8.  [License](#license) (Placeholder)

## 1. Features

*   **Diverse Agent Behaviors:** Implementations for Stochastic, Inverse Turn, and Composite agents, each with unique decision-making logic.
*   **Configurable Environment:** Choose from preset food layouts (e.g., three lines, filled box, void) or define custom setups.
*   **Customizable Physics:** Adjust global speed modifiers, agent speed scaling, and angular proportionality constants to fine-tune agent movement.
*   **Visual Simulation:** A Pygame-based graphical interface to observe agent behavior in real-time, including optional movement traces.
*   **Headless Execution:** Run multiple simulation instances without a visual display for efficient data collection and performance comparison.
*   **Real-time Statistics & Graphs:** Live updates on food eaten, active agents, efficiency, and interactive performance history plots using Matplotlib.
*   **Detailed Logging:** Comprehensive logging of agent movements and simulation parameters to timestamped files for post-simulation analysis.
*   **Configuration Presets:** Load and save simulation settings from log files or dedicated preset files.
*   **Log File Analysis:** Built-in tool to analyze historical log data and extract key performance metrics.

## 2. Installation

1.  **Prerequisites:** Ensure you have Python 3.x installed on your system.
2.  **Clone the Repository:**
    ```bash
    git clone <repository_url> # Replace <repository_url> with the actual URL
    cd agent-simulation
    ```
3.  **Install Dependencies:** All required Python packages are listed in `requirements.txt`.
    ```bash
    pip install -r requirements.txt
    ```

## 3. Usage

To launch the simulation GUI, run the `main.py` file:

```bash
python main.py
```

The GUI is organized into four main tabs:
Controls & Sim Params: Configure global simulation settings, core physics, world setup, and agent-specific parameters. This is your primary control panel.
Composite Config: (Enabled when "Composite" agent type is selected) Define the layered behavior of Composite agents.
Headless Runs: Set up and execute multiple non-visual simulation instances for batch testing and data analysis.
Status & Graphs: Monitor live simulation statistics, view an event log, and visualize performance history graphs (requires Matplotlib).
For detailed instructions on each setting and feature, please refer to the user_manual.txt file.

4. Agent Types
The simulation supports the following agent behavioral models:
StochasticMotionAgent: Agents make randomized power output decisions at regular intervals, leading to unpredictable movements.
InverseMotionAgent: Agents adjust their left and right motor power outputs based on whether their current food consumption frequency is above or below a defined threshold, using sinusoidal patterns.
CompositeMotionAgent: An advanced agent that combines multiple "layers" of sinusoidal power generation. Each layer contributes its output only when the agent's food frequency meets or exceeds a specific threshold for that layer.

5. Configuration
Core simulation parameters are managed through the config.py file, which defines window dimensions, agent/food visual properties, and physics constants. Many of these, along with agent-specific parameters, can be adjusted dynamically via the GUI.

6. Logging and Analysis
When "Enable Logging" is selected, the simulation generates detailed log files (.log) containing per-frame, per-agent data (position, heading, movement deltas) and a header with all configuration settings. These logs are invaluable for debugging and detailed post-simulation analysis using the "Analyze Log File..." button on the "Status & Graphs" tab.
