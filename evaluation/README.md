# Evaluation: Reproducing Paper Results

This directory contains all scripts, datasets, and tools required to reproduce the experimental results presented in **Section 6** of the paper.

## Overview

The evaluation is organized into individual test directories, each corresponding to a specific experiment or figure in the paper. The tests can be run individually or all at once using the master script.

## Directory Structure

```
evaluation/
├── 1-run-all-tests.sh      # Master script to run all tests
├── README.md               # This file
├── graphs/                 # Figure generation scripts
│   ├── 1-draw-graphs.sh    # Generate all paper figures
│   ├── patterns/           # Pre-computed pattern data
│   └── scripts/            # Python plotting scripts
├── t1-mstp-metrics/        # M-Step performance metrics (Table 5)
├── t2-covert-udiv/         # Division timing channel (Fig. 6a)
├── t3-covert-inst/         # Instruction timing channel (Fig. 6b)
├── t4-covert-cache/        # Cache timing channel (Fig. 6c)
├── t5-covert-cont/         # Bus contention channel (Fig. 6d)
├── t6_pocs/                # End-to-end PoC attacks (Section 6.3)
└── t7-printf-gtkwave/      # Trace visualization tests
```

## Quick Start

### Run All Experiments

```bash
# Enter the Nix development environment first
cd /path/to/m-step
nix develop

# Run all tests
cd evaluation
./1-run-all-tests.sh
```

### Run with Timing Information

```bash
./1-run-all-tests.sh -ti
```

### Clean All Test Outputs

```bash
./1-run-all-tests.sh -c
```

## Paper Results Mapping

| Paper Result | Test Directory | Script | Output |
|--------------|----------------|--------|--------|
| **Table 5** (M-Step Metrics)  | `t1-mstp-metrics/`| `1-run-test.sh`   | Results on the terminal. |
| **Figure 6a** (Division)      | `t2-covert-udiv/` | `1-run-test.sh`   | `logs/06-div.png` |
| **Figure 6b** (Instructions)  | `t3-covert-inst/` | `1-run-test.sh`   | `logs/06-inst_diff.png` |
| **Figure 6c** (Cache)         | `t4-covert-cache/`| `1-run-test.sh`   | `logs/06-cache.png` |
| **Figure 6d** (Contention)    | `t5-covert-cont/` | `1-run-test.sh`   | `logs/06-contention.png` |
| **Figure 7a & 10** (Template) | `graphs/`         | `1-draw-graphs.sh`| `patterns/06-shift_template.png` |
| **Figure 7b** (Template)      | `graphs/`         | `1-draw-graphs.sh`| `patterns/06-sub_template.png` |
| **Section 6.3** (PoC Attacks) | `t6_pocs/`        | `1-run-pocs.sh`   | Results on the terminal. |
| **Appendix A Fig 8**          | `t7-printf-gtkwave/`| `1-run-test.sh` | Opens GTKWave with the trace of the printf. |

## Individual Test Descriptions

### T1: M-Step Performance Metrics (Table 5)

Measures the overhead and accuracy of the M-Step single-stepping mechanism.

```bash
cd t1-mstp-metrics
./1-run-test.sh
```

**Outputs:**
- Single-step success rate, timing overhead, zero-step detection accuracy will be shown on the terminal.

### T2: Division Timing Channel (Figure 6a)

Evaluates the covert channel based on the variable-time UDIV instruction.

```bash
cd t2-covert-udiv
./1-run-test.sh
```

**Outputs:**
- `logs/0_raw_trace.txt`: Raw trace from the board
- `logs/1_results.txt`: Processed timing differences
- `logs/06-div.txt`: Matrix data for figure generation
- `logs/06-div.png`: Generated heatmap figure

### T3: Instruction Timing Channel (Figure 6b)

Evaluates timing differences between different instruction types.

```bash
cd t3-covert-inst
./1-run-test.sh
```

**Outputs:**
- `logs/0_raw_trace.txt`: Raw trace from the board
- `logs/06-inst_diff.txt`: Matrix data
- `logs/06-inst_diff.png`: Generated figure

### T4: Cache Timing Channel (Figure 6c)

Evaluates the cache-based timing side channel (Mstp-Cache).

```bash
cd t4-covert-cache
./1-run-test.sh
```

**Outputs:**
- `logs/0_raw_trace.txt`: Raw trace from the board
- `logs/06-cache.txt`: Matrix data
- `logs/06-cache.png`: Generated figure

### T5: Bus Contention Channel (Figure 6d)

Evaluates the bus contention side channel (Mstp-BUSted).

```bash
cd t5-covert-cont
./1-run-test.sh
```

**Outputs:**
- `logs/0_raw_trace.txt`: Raw trace from the board
- `logs/06-contention.txt`: Matrix data
- `logs/06-contention.png`: Generated figure

### T6: Proof-of-Concept Attacks (Section 6.3)

Runs the end-to-end RSA key extraction attacks against Mbed TLS.

```bash
cd t6_pocs
./1-run-pocs.sh
```

**What it does:**
1. Fetches and patches TF-M and Mbed TLS
2. Builds and deploys PoC #1 (RSA key generation attack)
3. Captures the trace and extracts the private key
4. Builds and deploys PoC #2 (RSA decryption attack)
5. Captures the trace and extracts the private key
6. Compares extracted keys with ground truth

**Outputs:**
- `logs/trace_poc1.txt`: Trace from PoC #1
- `logs/trace_poc2.txt`: Trace from PoC #2
- You will see the extracted RSA prime factors (P, Q) on the terminal.


### T7: GTKWave Visualization Tests

Tests the trace visualization pipeline.

```bash
cd t7-printf-gtkwave
./1-run-test.sh
```

**Outputs:**
- `logs/`: Trace files
- VCD files for GTKWave visualization

## Generating Paper Figures

To regenerate all figures from the paper using pre-computed data:

```bash
cd graphs
./1-draw-graphs.sh
```

Or with custom output directory:

```bash
./1-draw-graphs.sh -o /path/to/output
```

### Figure Generation Scripts

| Script | Figures | Description |
|--------|---------|-------------|
| `gen_matrix.py` | Fig. 6a-d | Generates heatmap matrices |
| `gen_template_matrix.py` | Fig. 7, 10 | Generates template attack matrices |

## Adding New Experiments

To add a new experiment:

1. Create a new directory `tN-<experiment-name>/`
2. Add a `1-run-test.sh` script following the existing pattern
3. Create a `logs/` directory for outputs
4. Update `1-run-all-tests.sh` to include your test
5. Document the expected outputs
