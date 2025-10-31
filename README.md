## Overview
This project allows ns-3's MAC scheduler to communicate with an external DRL agent via a REST API. The MAC scheduler sends real-time network metrics to the DRL agent, which processes the information and returns optimal scheduling decisions.

## Usage

#### Manual Execution

First, you need to run the server to handle incoming requests from the ns-3 MAC scheduler.

```python
uvicorn server:app
```
Once the server is up and running, you can execute ns-3 configured with the DPPA scheduler.

```python
./ns-3-dev/ns3 run nr-sched-o-ran-buildings
```

#### Using the Jupyter Notebook

For convenience, a Jupyter Notebook is provided to automate the entire simulation workflow.
Open nr_sched_oran_buildings_test.ipynb and execute all cells sequentially.

The notebook performs the following steps:

1. Launches the server.

2. Configures and runs the ns-3 simulation.

3. Generates illustrative figures summarizing the simulation results.