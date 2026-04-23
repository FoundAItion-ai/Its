# NNN (Natural Neural Network) Physical Model Summary

## Core Principle: The Frequency Inverter

The fundamental building block of the NNN model is the **Inverter** (INV). The name comes from its core function: it **inverts frequency**.

- **Input frequency > threshold** → **No output** (inhibited/suppressed)
- **Input frequency < threshold** → **Emit signals at the threshold frequency**

This is the physical principle behind the name "inverter": high input produces low (zero) output, low input produces high output. The threshold frequency defines both the switching point AND the output frequency when active.

## Elementary Units

### GEN (Generator / Oscillator)
A free-running oscillator that fires at a fixed base frequency.

- Has a self-excitatory feedback loop: output feeds back to input
- The feedback delay determines the oscillation period
- If delay ~ refractory period → **GEN mode** (continuous oscillation)
- Can be gated (started/stopped) by inhibitory input
- Think of it as a biological pacemaker neuron

*Theoretical concept from the NNN paper. Not implemented as a separate class in the codebase.*

### INV (Inverter)
Same physical cell as GEN, but with a different feedback configuration.

- Has BOTH external input AND self-feedback
- If feedback delay >> refractory period → **INV mode**
- Output frequency is the INVERSE of input frequency
- Detects the **ABSENCE** of signals within a certain period
- Key insight: "timing is the key -- not reacting to signals, but to ABSENCE of signals within certain period"

*Implemented as the `Inverter` class in `inverter.py`.*

### GEN vs INV: Same Cell, Different Wiring
The GEN and INV are the **same cell type** with different feedback delay:
- Short delay (~ refractory) → self-sustaining oscillation (GEN)
- Long delay (>> refractory) → frequency inversion (INV)
- STDP (spike-timing-dependent plasticity / Hebbian learning) automatically strengthens the dominant input pathway, allowing the cell to adapt between modes

## The GEN+INV Pair Architecture

A single NNN functional unit consists of a GEN+INV pair sharing the same input:

```
              +---- GEN ---- Output A (tracks input)
Input --------+
              +---- INV ---- Output B (inverts input)
```

- **GEN path**: Fires when input is present → Output A tracks input frequency
- **INV path**: Fires when input is absent → Output B is active during silence

This creates two outputs with **naturally different frequency responses** to the same input signal. When wired to L/R motors, this differential drives turning behavior.

## CMNT (Camertone / Tuning Fork) -- The Silence Detector

The CMNT is a composite structure built from multiple GEN+INV pairs. Its purpose is to detect **prolonged absence** of input (silence detection over long periods).

*Theoretical concept from the NNN paper. Not implemented in the codebase; the composite circuit achieves a similar effect through staggered C1 windows.*

### Architecture
Multiple generators with **pairwise prime frequencies** collectively inhibit a shared output:

```
Input --+-- GEN (period 3) --+
        +-- GEN (period 5) --+-- collective inhibition -- INV -- Output
        +-- GEN (period 7) --+
```

### How It Works
1. Each GEN fires at its own prime frequency (e.g., 3, 5, 7 Hz)
2. Their spikes collectively keep the output INV suppressed
3. Only when ALL generators stop firing (no input for long enough) does the INV activate
4. The silence detection window = **product of prime frequencies** (e.g., 3 x 5 x 7 = 105 seconds)
5. Any single input spike resets the timer by reactivating the generators

### Why Pairwise Prime?
Using coprime frequencies ensures the generators' silence windows don't align except at the product interval. This creates a reliable long-period timer from cheap, fast oscillators. If frequencies shared common factors, silence would be detected too early.

### Biological Role
The CMNT detects that the organism hasn't found food for an extended period, triggering a mode switch from "local search" to "expanding spiral search" -- a survival behavior.

## How Spiral Search Emerges

Spiral search is **emergent behavior** from staggered activation of inverters with different decision windows. It does NOT come from internal oscillator beat patterns.

### The Mechanism: Staggered Activation
1. **Food found** → ALL inverters **reset immediately** (stop firing, restart counting windows)
2. **Food stops** → each inverter silently counts over its own C1 window:
   - **t = 0.15s**: f1 (normal wiring, C1=0.15) finishes counting, detects silence → **starts firing** → drives tight circle
   - **t = 3.0s**: f2 (crossed wiring, C1=3.0) finishes counting → starts firing in **counter-phase** → partially opposes f1 → **wider turn**
   - **t = 6.0s**: f3 (crossed wiring, C1=6.0) finishes counting → fires in counter-phase → further opposes → **even wider turn**
   - **t = 10.0s**: f4 (crossed wiring, C1=10.0) finishes counting → fires in counter-phase → **widest arc**
3. **Food found again** → instant reset → back to step 1

### Why This Creates a Spiral
- Each new crossed inverter that activates **changes the L/R balance**
- The turn radius **progressively widens** as more counter-phase inverters join
- Progressive widening of turn radius + forward motion = **expanding spiral**
- The temporal lag between activations is determined by C1 differences, not oscillator beats

### Key Insight
The spiral is a natural consequence of:
- **Different C1 periods** = different reactivation delays after food reset
- **Crossed wiring** on slower inverters = counter-phase that opposes the base turn
- **Immediate reset on food** = the spiral restarts from tight circle every time food is found

## Current Implementation

### `Inverter` class (`inverter.py`)
- **1x2 unit**: 1 input (food eaten count), 2 outputs (L and R spikes)
- **Decision window**: Counts input signals over a full C1 period before deciding
- **Staggered activation**: Silent until first window completes; different C1 = different activation delay
- **Frequency inversion**: Input detected → suppress output (C2/C4 periods); No input → fire (C1/C3 periods)
- **Immediate reset**: `reset()` restarts counting window, stops all output
- **Spike jitter**: Gaussian inter-spike interval variability (CV = 0.1 default, clamped to [0.5x, 1.5x])

### Power model (`agent_module.py`)
- **P = rate (Hz)**: `get_rates()` returns deterministic base rates (1/period), no gain or baseline
- **Linear summation**: Composite agents sum rates across all inverters with crossed wiring swap
- **Speed**: `(P_r + P_l) * speed_scaling_factor` (default 5.0)
- **Turn angle**: `90 * (1 - exp(-angular_const * |P_r - P_l|)) * sign` (saturating, default angular_const 0.08)
- **Rotational diffusion**: Per-step Gaussian heading noise, `sigma = sqrt(2 * D_ROT * dt)` (D_ROT = 0.1 rad^2/s)

### Composite auto-recalculation (`inverter.py`)
- `recalculate_composite()` derives crossed inverter C2/C3/C4 from f1's turn differential
- **Opposition fractions**: 27%, 15%, 5% (diminishing by 0.5x for extra inverters beyond 3)
- **C2/C1 ratio**: 1.33, **C4/C2 ratio**: 2.0

## Parameter Mapping

HIGH = active/firing (void, no input). LOW = suppressed (food detected).

| Parameter | Physical Meaning                      | Current Role                                    |
|-----------|---------------------------------------|-------------------------------------------------|
| C1        | Decision window + HIGH mode L period  | Mode switch threshold + LEFT period in HIGH mode |
| C2        | LOW mode L period                     | LEFT period in LOW mode (suppressed)            |
| C3        | HIGH mode R period                    | RIGHT period in HIGH mode                       |
| C4        | LOW mode R period                     | RIGHT period in LOW mode (suppressed)           |
| crossed   | Wiring swap (L<->R)                   | Counter-phase mixing in composites              |

### Frequency Constraint

Two valid orderings (period space):

- **Normal**: C3 < C1 < C2 < C4 (R faster in HIGH, L faster in LOW)
- **Mirror**: C1 < C3 < C4 < C2 (L faster in HIGH, R faster in LOW)

The L/R asymmetry must flip between HIGH and LOW modes. Mixing (e.g., same side faster in both modes) breaks inversion behavior.

### Default Presets

**Single inverter** (C3 < C1 < C2 < C4):
- C1=0.15, C2=0.45, C3=0.135, C4=1.35
- HIGH: L=6.67Hz, R=7.41Hz → fast, gentle R>L curve
- LOW: L=2.22Hz, R=0.74Hz → slow, sharp L>R deflection

**Composite** (4 inverters):
- f1: C1=0.15 (normal) — tight initial circle
- f2: C1=3.0 (crossed, 27% opposition) — first widening
- f3: C1=6.0 (crossed, 15% opposition) — second widening
- f4: C1=10.0 (crossed, 5% opposition) — widest arc
