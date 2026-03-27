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
- If delay ≈ refractory period → **GEN mode** (continuous oscillation)
- Can be gated (started/stopped) by inhibitory input
- Think of it as a biological pacemaker neuron

### INV (Inverter)
Same physical cell as GEN, but with a different feedback configuration.

- Has BOTH external input AND self-feedback
- If feedback delay >> refractory period → **INV mode**
- Output frequency is the INVERSE of input frequency
- Detects the **ABSENCE** of signals within a certain period
- Key insight: "timing is the key — not reacting to signals, but to ABSENCE of signals within certain period"

### GEN vs INV: Same Cell, Different Wiring
The GEN and INV are the **same cell type** with different feedback delay:
- Short delay (≈ refractory) → self-sustaining oscillation (GEN)
- Long delay (>> refractory) → frequency inversion (INV)
- STDP (spike-timing-dependent plasticity / Hebbian learning) automatically strengthens the dominant input pathway, allowing the cell to adapt between modes

## The GEN+INV Pair Architecture

A single NNN functional unit consists of a GEN+INV pair sharing the same input:

```
              ┌──── GEN ──── Output A (tracks input)
Input ────────┤
              └──── INV ──── Output B (inverts input)
```

- **GEN path**: Fires when input is present → Output A tracks input frequency
- **INV path**: Fires when input is absent → Output B is active during silence

This creates two outputs with **naturally different frequency responses** to the same input signal. When wired to L/R motors, this differential drives turning behavior.

## CMNT (Camertone / Tuning Fork) — The Silence Detector

The CMNT is a composite structure built from multiple GEN+INV pairs. Its purpose is to detect **prolonged absence** of input (silence detection over long periods).

### Architecture
Multiple generators with **pairwise prime frequencies** collectively inhibit a shared output:

```
Input ──┬── GEN (period 3) ──┐
        ├── GEN (period 5) ──┼── collective inhibition ── INV ── Output
        └── GEN (period 7) ──┘
```

### How It Works
1. Each GEN fires at its own prime frequency (e.g., 3, 5, 7 Hz)
2. Their spikes collectively keep the output INV suppressed
3. Only when ALL generators stop firing (no input for long enough) does the INV activate
4. The silence detection window = **product of prime frequencies** (e.g., 3 × 5 × 7 = 105 seconds)
5. Any single input spike resets the timer by reactivating the generators

### Why Pairwise Prime?
Using coprime frequencies ensures the generators' silence windows don't align except at the product interval. This creates a reliable long-period timer from cheap, fast oscillators. If frequencies shared common factors, silence would be detected too early.

### Biological Role
The CMNT detects that the organism hasn't found food for an extended period, triggering a mode switch from "local search" to "expanding spiral search" — a survival behavior.

## How Spiral Search Emerges

Spiral search is **emergent behavior** from staggered activation of inverters with different decision windows. It does NOT come from internal oscillator beat patterns.

### The Mechanism: Staggered Activation
1. **Food found** → ALL inverters **reset immediately** (stop firing, restart counting windows)
2. **Food stops** → each inverter silently counts over its own C1 window:
   - **t = C1_fast** (e.g., 0.5s): Fastest inverter (normal wiring) finishes counting, detects silence → **starts firing** → drives tight circle
   - **t = C1_medium** (e.g., 0.8s): Medium inverter (crossed wiring) finishes counting → starts firing in **counter-phase** → partially opposes the first → **wider turn**
   - **t = C1_slow** (e.g., 1.3s): Slowest inverter (crossed wiring) finishes counting → fires in counter-phase → further opposes → **even wider turn**
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

### `Inverter` class (temporal integration model)
- **Decision window**: Counts input signals over a full C1 period before deciding
- **Staggered activation**: Silent until first window completes; different C1 = different activation delay
- **Frequency inversion**: High input → suppress output; Low input → fire at own periods (C1/C3)
- **Immediate reset**: `reset()` restarts counting window, stops all output
- Discrete unit impulse spikes (0 or 1)
- Dendritic integration in agent (sqrt sublinear + tanh saturation)

### Potential Future Extensions
1. **CMNT (Camertone)**: Composite silence detector using multiple generators with pairwise prime frequencies for very long timescale detection
2. **GEN+INV pair architecture**: Explicit generator/inverter pair per functional unit
3. **STDP / Hebbian learning**: Adaptive pathway strengthening

## Parameter Mapping

| Parameter | Physical Meaning | Current Role |
|-----------|-----------------|--------------|
| C1 | Threshold period / LOW mode L period | Mode switch threshold + LEFT period in LOW mode |
| C2 | HIGH mode L period | LEFT period in HIGH mode |
| C3 | LOW mode R period | RIGHT period in LOW mode |
| C4 | HIGH mode R period | RIGHT period in HIGH mode |
| crossed | Wiring swap (L↔R) | Counter-phase mixing in composites |

## Next Steps for True NNN Implementation

1. **Redesign Inverter** to implement continuous frequency inversion (not binary mode switch)
2. **Implement GEN** as a self-sustaining oscillator with inhibitory gating
3. **Build GEN+INV pair** as the fundamental functional unit
4. **Implement CMNT** from multiple GENs with prime frequencies for slow-timescale silence detection
5. **Redesign presets** with proper frequency relationships that enable spiral emergence through organism-environment interaction
