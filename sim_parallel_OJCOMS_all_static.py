# %%
import os
import numpy as np
import csv
from matplotlib import pyplot as plt
import pandas as pd
import itertools
import time
import subprocess

# %% [markdown]
# # Run scheduler with flexible reconfiguration

# %%
# os.system("python3 ../server.py &")
# os.system("python3 sim.py")

def simulate(id_sim, serverModel, restport, envType, mcsVector, mcsAsUniformVar, mcsMinValue, rlc_buffer_size, num_sim, t, vs, qs, gs, s, d, m, speed, fixMcs, fixMcsValue, arGbr, vrGbr, cgGbr, voipGbr, arNum, vrNum, cgNum, voiceNum, arDR, vrDR, cgDR, arFps, vrFps, cgFps, bw, rngRun, numerology, frequency, enableFading, enableShadowing, logging, enableVirtualQueue, scheds):
    for sched in scheds:
        for n in range(0, num_sim): # n es el rngRun <-- Ahora lo dejo con rngRun para hacer pruebas, no cambio esto por no liarla con los nombres de los ficheros
            if sched == 'DPPA':
                for v in vs:
                    for q in qs:
                        for g in gs:
                            # cmd = f'./ns-3-dev/ns3 run nr-sched-o-ran-buildings -- \
                            # --appDuration={t} \
                            # --schedulerType={sched} \
                            # --dppV={v} --dppWeightQ={q} --dppWeightG={g} --enableVirtualQueue={enableVirtualQueue} \
                            # --vrGbrDl={vrGbr} --cgGbrDl={cgGbr} --arGbrDl={arGbr} \
                            # --scenario={s} --distance={d} --mobility={m} --speed={speed} \
                            # --vrUeNum={vrNum} --cgUeNum={cgNum} --arUeNum={arNum} --voiceUeNum={voiceNum} \
                            # --arDataRate={arDR} --vrDataRate={vrDR} --cgDataRate={cgDR} \
                            # --arFps={arFps} --vrFps={vrFps} --cgFps={cgFps} \
                            # --bandwidth={bw} --rngRun={rngRun} --numerology={numerology} --frequency={frequency} \
                            # --enableFading={enableFading} --enableShadowing={enableShadowing} --logging={logging} \
                            # --rlcBufferSize={rlc_buffer_size} --fixMcs={fixMcs} --fixMcsValue={fixMcsValue} \
                            # --mcsVector={mcsVector} --mcsAsUniformVar={mcsAsUniformVar} --mcsMinValue={mcsMinValue} \
                            # --server_model={serverModel} --restPort=8000 --envType=ISAC \
                            # > output.out 2>&1'
                            cmd = f"""parallel './ns-3-dev/ns3 run nr-sched-o-ran-buildings -- \
                            --appDuration={t} \
                            --schedulerType={sched} \
                            --dppV={v} --dppWeightQ={q} --dppWeightG={g} --enableVirtualQueue={enableVirtualQueue} \
                            --vrGbrDl={vrGbr} --cgGbrDl={cgGbr} --arGbrDl={arGbr} \
                            --scenario={s} --distance={d} --mobility={m} --speed={speed} \
                            --vrUeNum={vrNum} --cgUeNum={cgNum} --arUeNum={arNum} --voiceUeNum={voiceNum} \
                            --arDataRate={arDR} --vrDataRate={vrDR} --cgDataRate={cgDR} \
                            --arFps={arFps} --vrFps={vrFps} --cgFps={cgFps} \
                            --bandwidth={bw} --rngRun={rngRun} --numerology={numerology} --frequency={frequency} \
                            --enableFading={enableFading} --enableShadowing={enableShadowing} --logging={logging} \
                            --rlcBufferSize={rlc_buffer_size} --fixMcs={fixMcs} --fixMcsValue={fixMcsValue} \
                            --mcsVector={mcsVector} --mcsAsUniformVar={mcsAsUniformVar} --mcsMinValue={mcsMinValue} \
                            --server_model={serverModel} --restPort={{1}} --envType={{2}} \
                            > output_{{1}}_{{2}}.out 2>&1' \
                            ::: 8000 8001 8002 8003 :::+ Static_2_0_0 Static_3_0_0 Static_2_0_1 Static_3_0_1"""
                            # ::: 8000 8001 8002 :::+ Static_0_0_1 Static_1_0_1 Static_2_0_1"""
                            # ::: 8000 :::+ Test"""
                            os.system(cmd)
            else:
                pass

## Servers
# Lanzar procesos de uvicorn
pid1 = int(os.popen("nohup uvicorn server:app --port=8000 > server1.out & echo $!").read().strip())
pid2 = int(os.popen("nohup uvicorn server:app --port=8001 > server2.out & echo $!").read().strip())
pid3 = int(os.popen("nohup uvicorn server:app --port=8002 > server3.out & echo $!").read().strip())
pid4 = int(os.popen("nohup uvicorn server:app --port=8003 > server4.out & echo $!").read().strip())
print("Servers started")
time.sleep(10) # Wait for the servers to start

## Simulation parameters
num_sim = 1
logging = "false"
rngRun = 0
# t = 2e7 # ms. App duration --- 2e7 con 2 UEs son 6 horas aprox.
t = 4e5
## Scenario. Rural macrocell (RMa), urban macrocell (UMa), and urban microcell (UMi). 'RMa', 'UMa', 'UMi-StreetCanyon', 'InH-OfficeMixed', and 'InH-OfficeOpen'
# s = "InH_OfficeOpen_LoS"
s = "UMa"
d = 25
speed = 2 # m/s
m = "false"
numerology = 0
# bws = [100e6]
bws = [20e6]
frequency = 3e9
enableFading = "false"
enableShadowing = "false"
## Number of UEs
arNum = 0 # 3 flows in each UE with AR_M3
vrNum = 2
cgNum = 2
voiceNum = 0
ueNum = arNum + vrNum + cgNum + voiceNum
flowNum = 3*arNum + vrNum + cgNum + voiceNum
## QoS thput requirements
arGbr = 0
vrGbr = 2e6
cgGbr = 4e6
voipGbr = 0
## Traffic parameters
arDR = 20
# vrDR = 45
vrDR = 10
cgDR = 10
arFps = 60
vrFps = 60 # fps
cgFps = 60
## Scheduler configuration
scheds = ['DPPA']
# scheds = ['PF']
vs = [0]
qs = [1]
gs = [1]
enableVirtualQueue = "true"
## RLC buffer size
rlc_buffer_size = 999999999 # Infinite

## MCS configuration
fixMcs = "true" # If true, the mcs is fixed to mcsVector or fixMcsValue FIXME
# We define 3 ranges of MCS values: [14, 18] (bad), [19, 23] (moderate) and [24, 28] (good). In the dataset they are represented as 0, 1 and 2 respectively.
# Combinatorial of 16, 22, 26. 81 combinations
elements = [16, 21, 26]
combinations = list(itertools.product(elements, repeat=4))
combo_strings_mcs = [",".join(map(str, combo)) for combo in combinations] # Pass the MCS to ns3 in a string

# For the case of mcsAsUniformVar = true
mcsAsUniformVar = "false" # If true, the MCS is selected randomly between mcsMinValue and fixMcsValue every 2 seconds. NOT AVAILABLE FOR THIS VERSION OF THE CODE
fixMcsValue = 28 # If fixMcs is true, the mcs is fixed to this value
mcsMinValue = 14 # If mcsAsUniformVar is true, the mcs is selected randomly between mcsMinValue and fixMcsValue

# serverModel = "PPO"
# serverModel = "Random"
serverModel = "Static"

# envType = "ISAC"
# envType = "Basic"

restport = 8000

# %%
print('Starting simulation...')
start_time = time.time()
i = 0 # Simulation id. sim_results/sim_results{i}/...
for bw in bws:
    # for envType in ["Basic", "ISAC"]:
    envType = ["Basic", "ISAC"]
    os.system("> ns-3-dev/sinr_log.txt")
    for i in range(1):
        print(f'[{i+1}/1]')
        simulate(i, serverModel, restport, envType, "28,28,28,28,28,28", mcsAsUniformVar, mcsMinValue, rlc_buffer_size, num_sim, t, vs, qs, gs, s, d, m, 1, fixMcs, fixMcsValue, arGbr, vrGbr, cgGbr, voipGbr, arNum, vrNum, cgNum, voiceNum, arDR, vrDR, cgDR, arFps, vrFps, cgFps, bw, rngRun, numerology, frequency, enableFading, enableShadowing, logging, enableVirtualQueue, scheds)
end_time = time.time()
simulation_time = end_time - start_time
print(f'Simulation finished.')
print(f'Simulation time: {simulation_time:.2f} seconds ({simulation_time/60:.2f} minutes)')
import signal
os.kill(pid1, signal.SIGTERM)
os.kill(pid2, signal.SIGTERM)
os.kill(pid3, signal.SIGTERM)
os.kill(pid4, signal.SIGTERM)