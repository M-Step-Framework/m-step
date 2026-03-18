# M-Step: A Single-Stepping Framework for Side-Channel Analysis on TrustZone-M

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-STM32L5-green.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32l5-series.html)
[![Paper](https://img.shields.io/badge/Paper-USENIX%20Sec'26-red.svg)](TBD!)

This repository contains the artifacts for the USENIX Security'26 paper: **"M-Step: A Single-Stepping Framework for Side-Channel Analysis on TrustZone-M"**.

M-Step is a software-based single-stepping framework that enables instruction-level side-channel analysis on ARM TrustZone-M enabled microcontrollers. It allows an unprivileged attacker in the Non-Secure world to precisely interrupt and observe the execution of Secure world code at instruction granularity.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Repository Structure](#repository-structure)
- [Hardware Requirements](#hardware-requirements)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Building and Deployment](#building-and-deployment)
- [Running the Evaluation](#running-the-evaluation)
- [Tools and Utilities](#tools-and-utilities)
- [Extending M-Step](#extending-m-step)
- [Troubleshooting](#troubleshooting)
- [Citation](#citation)
- [License](#license)

---

## Overview

M-Step exploits the interrupt mechanism in ARM Cortex-M processors to achieve single-stepping of Secure world code from the Non-Secure world. The framework includes:

- **Core M-Step Algorithm**: Precise timer-based interrupt injection for single-stepping
- **Side-Channel Primitives**: Multiple observation channels including:
  - `Mstp-Nemesis`: Reveals interrupt-latencies, Nemesis.
  - `Mstp-Cache`: Reveals cache activity, Prime+Probe.
  - `Mstp-BUSted`: Reveals memory accesses, BUSted.
  - `Mstp-Zoom`: Amplifies interrupt-latency leakage.
- **Analysis Tools**: Trace visualization, VCD generation for GTKWave, and automated key extraction
- **Proof-of-Concept Attacks**: End-to-end RSA key extraction from Mbed TLS

---

## Repository Structure

```
m-step/
├── copilot/            # Build automation scripts and TF-M configurations
├── evaluation/         # Evaluation scripts and datasets
├── m-step/             # M-Step framework source code
├── ns-world-bare/      # Baremetal Non-Secure world runtime
├── s-world/            # Secure world runtime (TF-M, Mbed TLS, MCUboot)
├── flake.nix           # Nix development environment
```

---

## Hardware Requirements

- **Target Board**: [NUCLEO-L552ZE-Q](https://www.st.com/en/evaluation-tools/nucleo-l552ze-q.html) (STM32L5 series with TrustZone-M)

---

## Prerequisites

### 1. Install Nix Package Manager

The development environment uses Nix for reproducible builds. Install Nix and add your user to the `nix-users` group:

```bash
sh <(curl --proto '=https' --tlsv1.2 -L https://nixos.org/nix/install) --no-daemon
```

Enable experimental features by adding to `~/.config/nix/nix.conf` or `/etc/nix/nix.conf`:

```
experimental-features = nix-command flakes
```

### 2. Install STM32 Programmer

Download and install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) (requires ST account).

Verify installation:
```bash
STM32_Programmer_CLI --version
```

Ensure `STM32_Programmer_CLI` is in your `PATH`.

### 3. Configure USB Permissions (udev Rules)

Create `/etc/udev/rules.d/99-stlink.rules` with the following content:

```
# ST-LINK V3 (STM32L5 Discovery / Nucleo)
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374e", MODE="0666", GROUP="plugdev"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666", GROUP="plugdev"
```

Reload udev rules:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

> **Note**: Without these rules, `sudo` is required for every deployment.

---

## Quick Start

### 1. Clone the Repository

```bash
git clone --recurse-submodules https://github.com/M-Step-Framework/m-step.git
cd m-step
```

### 2. Enter the Development Environment

```bash
nix develop
```

This provides a reproducible shell with all required dependencies (ARM toolchain, CMake, Python packages, GTKWave, etc.).

> **Note**: Run `nix develop` in every new terminal session.

### 3. Build and Deploy

```bash
# Navigate to the baremetal NS world
cd ns-world-bare

# Configure the build system
./0-config.sh

# Compile the Secure and Non-Secure images
./1-compile.sh

# Deploy to the target board
./2-deploy.sh
```

### 4. Monitor Output

```bash
# Start the serial monitor
./3-monitor.sh -o trace.txt
```

---

## Building and Deployment

### Using the Copilot Script

The `copilot.sh` script provides a unified interface for building and deploying:

```bash
cd copilot

# Configure and build the Secure world
./copilot.sh -c s -p mstp
./copilot.sh -b s -p mstp

# Configure and build the Non-Secure world
./copilot.sh -c ns -p mstp
./copilot.sh -b ns -p mstp

# Deploy to target
./copilot.sh -d -p mstp

# Monitor serial output
./copilot.sh -m output.txt
```

### Build Profiles

| Profile | Description |
|---------|-------------|
| `bare`  | Minimal TF-M configuration |
| `crypto`| TF-M with crypto services |
| `mstp`  | Full M-Step evaluation setup |

### First-Time Setup

On first deployment, you may need to configure the board's option bytes.
Uncomment the regression.sh line in ([`copilot.sh`](copilot/copilot.sh)) or run manually:

```bash
${BUILD_S}/api_ns/regression.sh
```

---

## Running the Evaluation

### Run All Tests

```bash
cd evaluation
./1-run-all-tests.sh
```

### Run Individual Tests

| Test | Command | Paper Reference |
|------|---------|-----------------|
| M-Step Metrics | `./evaluation/t1-mstp-metrics/1-run-test.sh` | Table 5 |
| DIV Instruction Covert Channel | `./evaluation/t2-covert-udiv/1-run-test.sh` | Figure 6a |
| Instruction Timing Covert Channel | `./evaluation/t3-covert-inst/1-run-test.sh` | Figure 6b |
| ICache-based Covert Channel | `./evaluation/t4-covert-cache/1-run-test.sh` | Figure 6c |
| Bus Contention Covert Channel | `./evaluation/t5-covert-cont/1-run-test.sh` | Figure 6d |
| End-to-End PoC Attacks | `./evaluation/t6_pocs/1-run-pocs.sh` | Section 6.3 |
| Trace visualization tests | `./evaluation/t7-printf-gtkwave/1-run-test.sh`| Figure 8 |

### Generate Paper Figures

```bash
cd evaluation/graphs
./1-draw-graphs.sh
```

### Clean Up

```bash
./1-run-all-tests.sh -c
```

---

## Tools and Utilities

### Serial Monitor (`mstp-monitor`)

Captures UART output from the target board:

```bash
python3 m-step/mstp-monitor/monitor.py -o trace.txt
```

### Trace Visualizer (`mstp-visualizer`)

Converts M-Step traces to VCD format for visualization in GTKWave:

```bash
cd m-step/mstp-visualizer

# Generate VCD file
./1-gen-vcd.sh -t /path/to/trace.txt -elf /path/to/tfm_s.elf -o ./outputs

# Open in GTKWave
./2-gtkwave.sh -i ./outputs/trace.vcd
```

### Debugging Tools (`mstp-debug`)

- **OpenOCD**: Hardware debugging configurations
- **GDB-Py**: Python-enhanced GDB scripts
- **Renode**: MCU emulation for trace analysis

### Board Connection

```bash
# List connected boards
STM32_Programmer_CLI -l

# Connect via minicom
minicom -D /dev/ttyACM0 -b 115200

# Or via screen
screen /dev/ttyACM0 115200
```

---

## Extending M-Step

### Adding New Tests

See [`m-step/mstp-eval/README.md`](m-step/mstp-eval/README.md) for detailed instructions on:

1. Creating new test files
2. Defining test configurations
3. Customizing M-Step parameters

### M-Step Configuration

Key parameters in `m-step/mstp/inc/mstp.h`:

| Parameter | Description |
|-----------|-------------|
| `BASE_CLK` | Timer value for interrupt injection |
| `STREAK_THRESHOLD` | Zero-step detection threshold |
| `BASE_ISR_TIME` | Interrupt handler overhead |

### M-Step Plugins

M-Step supports multiple features which can be enabled on demand via plugins:

#### M-Step Side-Channel Plugins

| Plugin | Location | Description |
| :--- | :--- | :--- |
| **Mstp-Nemesis**  | `m-step/mstp`         | Reveals interrupt-latencies, Nemesis [57]. |
| **Mstp-Cache**    | `m-step/mstp-cache`   | Reveals cache activity, Prime+Probe [40, 42]. |
| **Mstp-BUSted**   | `m-step/mstp-busted`  | Reveals memory accesses, BUSted [46]. |

#### M-Step Architectural Plugins

| Plugin | Location | Description |
| :--- | :--- | :--- |
| **Mstp-Zoom** | `m-step/mstp` | Amplifies interrupt-latency leakage §5.2. |

#### M-Step Framework-related Plugins

| Plugin | Location | Description |
| :--- | :--- | :--- |
| **Mstp-Production**   | `m-step/mstp`         | Single-step for production code. |
| **Mstp-Debug**        | `m-step/mstp-debug`   | Single-step with debug information. |
| **Mstp-Metrics**      | `m-step/mstp-metrics` | Single-step performance metrics. |
| **Mstp-Emulator**     | `m-step/mstp-debug`   | MCU emulator with side-channel information. |
| **Mstp-Visualizer**   | `m-step/mstp-visualizer` | Interactive interface to visualize M-Step traces. |
| **Mstp-opDecoder**    | `m-step/mstp-opdecoder` | Runtime library to decode Armv8-M opcodes. |
| **Mstp-Test**         | `m-step/mstp-test` | Evaluation and regression testing framework. |

---

## Troubleshooting

### Common Issues

**Build Fails with missing tool/dependence:**

Probably you forget to enter a nix development shell.

```bash
nix develop
```

**Build fails with missing headers:**

Ensure all submodules are initialized.

```bash
git submodule update --init --recursive
```

---

## Citation

If you use M-Step in your research, please cite our paper:

```bibtex
@inproceedings{rodrigues2026mstep,
    title     = {{M-Step}: A Single-Stepping Framework for Side-Channel Analysis on {TrustZone-M}},
    author    = {Rodrigues, Cristiano and Bognar, Marton and Pinto, Sandro and Van Bulck, Jo},
    booktitle = {35th USENIX Security Symposium (USENIX Security 26)},
    year      = {2026},
    note      = {To appear},
    publisher = {USENIX Association}
}
```

---

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

---

**Questions or Issues?** Please open an [issue](https://github.com/M-Step-Framework/m-step/issues) on GitHub.
