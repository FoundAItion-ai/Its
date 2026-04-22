# Its

Research repository by FoundAItion Inc., hosting three related projects spanning the evolution from
conventional neural networks to natural spiking oscillator models.

## 1. CNN Engine (`CPP/`, `Solutions/`, `Tests/`, `Tools/`)

A convolutional neural network prototype powered by Nvidia's CUDA SDK. The model utilizes reflections
for self-tuning via backpropagation. Designed as a three-dimensional tensor with built-in runtime
support for collecting statistics and recording graphical snapshots, it supports persistent storage
and accommodates various types of training data.

## 2. NNN Model -- Paper 1 (`CPP/Face/VisualCube/`)

The original Natural Neural Network (NNN) spiking inverter model, implemented in C++, introduced in:

> A. Fedosov, M. Yakimenko, S. Stepanov, "Natural neural network as evolutionary trained foundation
> of intelligence," *Journal of Artificial Intelligence and Robotics*, vol. 4, no. 3, 2024.

This model demonstrated that a simple frequency-inverting spiking unit, wired to left/right motor
outputs, produces run-and-tumble foraging behavior without training.

## 3. NNN Model -- Paper 2 (`PY/VisualCube/`)

The current version of the codebase, extending the original model to composite circuits:

> A. Fedosov, M. Yakimenko, S. Stepanov, "Exploration-exploitation switching as an emergent property
> of composite frequency-tuned oscillators," 2026.

A composite circuit of three or more frequency-tuned spiking inverters, connected in counter-phase,
autonomously switches between spiral search (exploration) and food tracking (exploitation) without
training or explicit programming. Validated across 63,000 independent trials (10 hypotheses,
11 negative controls, 115/115 checks passed).

See [PY/VisualCube/README.md](PY/VisualCube/README.md) for setup, usage, and hypothesis details.

## License

See [LICENSE.txt](LICENSE.txt).
