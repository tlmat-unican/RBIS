# RBIS: Reinforcement Learning-Based Intelligent Scheduling for 5G NR Networks

[![ns-3](https://img.shields.io/badge/ns--3-3.x-green.svg)](https://www.nsnam.org/)
[![Python](https://img.shields.io/badge/python-3.8+-blue.svg)](https://www.python.org/)

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
- [DRL Algorithms](#drl-algorithms)
- [Environment Types](#environment-types)
- [Simulation Parameters](#simulation-parameters)

## Overview

RBIS (Reinforcement Learning-Based Intelligent Scheduling) is a research project that integrates Deep Reinforcement Learning (DRL) agents with ns-3's 5G NR MAC scheduler to optimize resource allocation in wireless networks. The system enables real-time communication between the ns-3 network simulator and external DRL agents via a REST API, allowing the agents to make intelligent scheduling decisions based on network conditions.

This project includes a **drift-plus-penalty algorithm (DPPA)** scheduler implemented in ns-3 5G-LENA responsible for the RB allocation among UEs, and an emulated xApp in which a DRL agent periodically adjusts the DPPA configuration to enhance performance and adaptability.

### Key Capabilities
- Real-time DRL-based MAC scheduling for 5G NR networks
- Support for multiple DRL algorithms (PPO, DQN)
- Multiple environment types (Basic, ISAC - Integrated Sensing and Communications)
- 3GPP-compliant traffic models for various application types
- Building-aware propagation models and channel conditions
- Comprehensive performance metrics and visualization tools

## Architecture

```
┌─────────────────────┐         REST API           ┌──────────────────────┐
│   ns-3 Simulator    │◄──────────────────────────►│   FastAPI Server     │
│                     │   HTTP (JSON payloads)     │                      │
│                     │                            │                      │
│  • MAC Scheduler    │   State (MCS, Queue, etc.) │  • DRL Agent         │
│  • Channel Models   │───────────────────────────►│  • Gymnasium Env     │
│  • Traffic Gen      │                            │  • Stable-Baselines3 │
│  • UE Mobility      │   Action (v, wq, wg)       │  • Model Training    │
│                     │◄───────────────────────────┤  • Policy Inference  │
└─────────────────────┘                            └──────────────────────┘
```

### Communication Flow
1. **ns-3 Simulator** executes network scenarios with multiple UEs and traffic types
2. **MAC Scheduler** collects network state information (MCS, queue status, UEs location, obstacles)
3. **REST API Client** sends state to the Python server via HTTP POST
4. **DRL Agent** processes the state and computes optimal scheduling parameters
5. **Server** returns action (scheduling weights) to ns-3
6. **ns-3** applies the decisions and continues execution

## Prerequisites
- ns-3 dependencies (build-essential, cmake)
- Stable-Baselines3 for DRL algorithms
- FastAPI and Uvicorn for REST API
- Gymnasium for environment interfaces
- NumPy, Pandas, Matplotlib for data processing and visualization

## Installation

### 1. Clone the Repository
```bash
git clone https://github.com/tlmat-unican/RBIS.git
cd RBIS
```

### 2. Install ns-3 Dependencies
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake python3-dev libsqlite3-dev \
    libboost-all-dev libgsl-dev git mercurial
```

### 3. Configure ns-3
```bash
./ns-3-dev/ns3 configure --build-profile=optimized --enable-examples --enable-tests
./ns-3-dev/ns3 build
```

### 4. Install Python Dependencies
```bash
pip install -r requirements.txt
```

### 5. Verify Installation
```bash
# Test ns-3 build
./ns-3-dev/ns3 run nr-sched-o-ran-buildings --help

# Test Python server
python3 -c "import fastapi, stable_baselines3, gymnasium; print('All modules loaded successfully')"
```

## Usage

### Option 1: Using the Jupyter Notebook (Recommended)

The easiest way to run simulations is using the provided Jupyter notebook, which automates the entire workflow:

```bash
jupyter notebook nr_sched_oran_buildings_test.ipynb
```

**The notebook performs:**
1. Launches the FastAPI server
2. Configures the simulation parameters
3. Runs the ns-3 simulation with DPPA scheduler
4. Collects and processes results
5. Generates performance visualizations

### Option 2: Manual Execution

#### Step 1: Start the FastAPI Server
```bash
uvicorn server:app --host 0.0.0.0 --port 8000
```

**Server Configuration Options** (edit `server.py`):
```python
LOAD_MODEL = True          # Load pre-trained model
PREDICT = True             # Inference mode (no training)
CONTINUE_TRAINING = False  # Continue training from checkpoint
model_type = "DQN"         # Choose: "PPO" or "DQN"
env_type = "Basic"         # Choose: "Basic" or "ISAC"
use_optimized_hyperparameters = True  # Use Optuna-optimized params
```

#### Step 2: Run ns-3 Simulation
```bash
./ns-3-dev/ns3 run "nr-sched-o-ran-buildings \
    --schedulerType=DPPA \
    --server_model=DQN \
    --envType=Basic \
    --arUeNum=1 \
    --vrUeNum=0 \
    --cgUeNum=0 \
    --voiceUeNum=0 \
    --appDuration=20 \
    --bandwidth=20 \
    --numerology=0 \
    --frequency=2100 \
    --enableFading=1 \
    --enableShadowing=1"
```

## DRL Algorithms

### Proximal Policy Optimization (PPO)
PPO is a policy gradient method that uses a clipped objective function to prevent large policy updates.

**Configuration:**
```python
model = PPO(
    "MlpPolicy",
    env,
    learning_rate=3e-4,
    n_steps=512,
    batch_size=64,
    n_epochs=10,
    gamma=0.99,
    verbose=1
)
```

**Best for:** Continuous state-action spaces, stable training

### Deep Q-Network (DQN)
DQN uses a neural network to approximate the Q-function for discrete action spaces.

**Configuration:**
```python
model = DQN(
    "MlpPolicy",
    env,
    learning_rate=1e-4,
    buffer_size=10000,
    learning_starts=100,
    batch_size=32,
    tau=0.005,
    gamma=0.99,
    verbose=1
)
```

**Best for:** Discrete action spaces, sample efficiency

## Environment Types

### Basic Environment
- **State Space**: `[MCS, Queue, Obstacle] × num_ues`
  - MCS: Modulation and Coding Scheme (0-2)
  - Queue: Buffer occupancy level (0-2)
  - Obstacle: Binary indicator (0-1)
- **Action Space**: 
  - PPO: `MultiDiscrete([4, 1, 1, ...])` for `v`, `w_q`, `w_g` per UE
  - DQN: `Discrete(4)` for parameter `v` only
- **Focus**: Basic scheduling without sensing capabilities

### Integrated Sensing and Communications (ISAC) Environment
- **State Space**: Enhanced with sensing information
- **Action Space**: Similar to Basic but with additional sensing-aware parameters
- **Focus**: Enhance scheduling with sensing

### Key Parameters
- **v**: Priority adjustment factor
- **w_q**: Queue weight
- **w_g**: Goodput weight

## Simulation Parameters

### Network Configuration
```bash
--bandwidth=20              # Channel bandwidth in MHz
--numerology=0              # 5G NR numerology
--frequency=6e9             # Carrier frequency in Hz
--enableFading=1            # Enable fast fading
--enableShadowing=1         # Enable shadow fading
```

### UE Configuration
```bash
--arUeNum=0                # Number of AR/VR UEs
--vrUeNum=2                # Number of virtual reality UEs
--cgUeNum=2                # Number of cloud gaming UEs
--voiceUeNum=0             # Number of VoIP UEs
--mobility=True            # Mobility model
--speed=1.5                # UE speed in m/s
```

### Traffic Configuration
```bash
--arGbrDl=2                # AR guaranteed bitrate (Mbps)
--vrGbrDl=2                # VR guaranteed bitrate (Mbps)
--cgGbrDl=4                # Cloud gaming guaranteed bitrate (Mbps)
--arDataRate=10            # AR data rate (Mbps)
--vrDataRate=10            # VR data rate (Mbps)
--cgDataRate=10            # CG data rate (Mbps)
```

### Simulation Duration
```bash
--appDuration=1e5          # Application duration in ms
--rngRun=1                 # Random number generator seed
```
