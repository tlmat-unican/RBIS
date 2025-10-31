from fastapi import FastAPI, Request
from pydantic import BaseModel
from threading import Condition, Thread
from stable_baselines3 import PPO, DQN
import random as rand
import numpy as np
import json
import os
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.evaluation import evaluate_policy
from stable_baselines3.common.callbacks import BaseCallback
import csv
import time
from env import CustomEnv as CustomEnvBasic
from env import MeanRewardLogCallback as MeanRewardLogCallbackBasic
from env_ISAC import CustomEnv as CustomEnvISAC
from env_ISAC import MeanRewardLogCallback as MeanRewardLogCallbackISAC
from env_DQN_ISAC import CustomEnvDQN, MeanRewardLogCallbackDQN
from env_DQN_Basic import CustomEnvDQN as CustomEnvDQNBasic
from env_DQN_Basic import MeanRewardLogCallbackDQN as MeanRewardLogCallbackDQNBasic

app = FastAPI()

LOAD_MODEL = True  # Set to True to load a pretrained model, False to train from scratch
PREDICT = True  # Set to True to use the model for prediction only, False to train
CONTINUE_TRAINING = False  # Set to True to continue training from last "configure" if possible

total_timesteps = 100_000
# n_eval_episodes = 2
n_steps = 512 # By default
batch_size = 64 # By default
model_type = None
model = None
env = None
condition = None
agent_thread = None
use_optimized_hyperparameters = True # Use optimized hyperparameters from Optuna
env_type = "Basic"  # Default environment type
callback_class = MeanRewardLogCallbackBasic # By default

steps_counter = 0
evaluation_mode = False
evaluation_steps_remaining = 0
last_evaluation_step = 0
pending_reward = None
pending_state = None
csv_index = 1

transition_mode = False
transition_steps_remaining = 0
transition_action = None

def generate_timestamp(endpoint: str, step: int = None, note: str = None):
    """Guardar un timestamp simple en request_timing.csv (no modifica payload)."""
    try:
        header_needed = not os.path.exists('request_timing.csv')
        with open('request_timing.csv', 'a', newline='') as tf:
            tw = csv.writer(tf)
            if header_needed:
                tw.writerow(['log_time', 'endpoint', 'step', 'note'])
            log_time_us = int(time.time() * 1_000_000)  # microsegundos
            tw.writerow([log_time_us, endpoint, step if step is not None else '', note if note else ''])
    except Exception as e:
        print(f"[TIMESTAMP LOG ERROR] {e}")

class ConfigureRequest(BaseModel):
    model: str
    num_ues: int
    env_type: str

class InferRequest(BaseModel):
    state: dict
    reward: float

def random_tuning():
    global env
    # TODO action = env.action_space.sample()
    num_ues = env.get_num_ues()
    return {
        "v": rand.randint(0, 10),
        "wq": [rand.randint(0, 3) for _ in range(num_ues)],
        "wg": [rand.randint(0, 3) for _ in range(num_ues)]
    }

def static_tuning(type, mcs):
    if type == "Test":
        if mcs == 1:
            return {
                "v": 3,
                "wq": 0,
                "wg": 1
            }
        else:
            return {
                "v": 2,
                "wq": 0,
                "wg": 1
            }
    if type == "Static_0_0_0":
        return {
            "v":  0,
            "wq": 0,
            "wg": 0
        }
    if type == "Static_1_0_0":
        return {
            "v":  1,
            "wq": 0,
            "wg": 0
        }
    if type == "Static_2_0_0":
        return {
            "v":  2,
            "wq": 0,
            "wg": 0
        }
    elif type == "Static_3_0_0":
        return {
            "v":  3,
            "wq": 0,
            "wg": 0
        }
    elif type == "Static_4_0_0":
        return {
            "v":  4,
            "wq": 0,
            "wg": 0
        }
    elif type == "Static_0_0_1":
        return {
            "v":  0,
            "wq": 0,
            "wg": 1
        }
    elif type == "Static_1_0_1":
        return {
            "v":  1,
            "wq": 0,
            "wg": 1
        }
    elif type == "Static_2_0_1":
        return {
            "v":  2,
            "wq": 0,
            "wg": 1
        }
    elif type == "Static_3_0_1":
        return {
            "v":  3,
            "wq": 0,
            "wg": 1
        }
    elif type == "Static_4_0_1":
        return {
            "v":  4,
            "wq": 0,
            "wg": 1
        }
    elif type == "Static_50_0_1":
        return {
            "v":  50,
            "wq": 0,
            "wg": 1
        }

def ppo(state, reward):
    global model, env, condition
    with condition:
        # print(f'env.update_data({state}, {reward})') 
        env.update_data((np.array(state), reward))
        if PREDICT: # TODO revisar
            action, _ = model.predict(np.array(state), deterministic=True)
            return action
        # print('Notifying agent loop to process new data...')
        condition.notify()
        # print('Waiting for agent to predict action...')
        condition.wait()
        return env.get_action()

def agent_loop():
    global model, condition, callback_class
    # print('Starting agent loop...')
    with condition:
        model.learn(total_timesteps=total_timesteps, reset_num_timesteps=False, callback=callback_class(verbose=1))

def load_training_state():
    """Carga solo el estado del entrenamiento (steps y csv_index) desde el CSV."""
    global steps_counter, csv_index, env_type
    
    csv_filename = f'learning_curve_{env_type}_0.csv'
    if os.path.exists(csv_filename):
        try:
            with open(csv_filename, 'r') as f:
                lines = f.readlines()
                if len(lines) > 1:
                    last_line = lines[-1].strip()
                    if last_line:
                        parts = last_line.split(',')
                        csv_index = int(parts[0]) + 1
                        steps_counter = int(parts[1])
                        print(f"Continuando entrenamiento desde: csv_index={csv_index}, steps_counter={steps_counter}")
                        return True
        except Exception as e:
            print(f"Error leyendo estado del CSV: {e}")
    
    # Si no se puede cargar, usar valores por defecto
    csv_index = 0
    steps_counter = 0
    print(f"ℹIniciando nuevo entrenamiento: csv_index={csv_index}, steps_counter={steps_counter}")
    return False

def is_agent_thread_running():
    """Verifica si el hilo de entrenamiento está activo."""
    global agent_thread
    return agent_thread is not None and agent_thread.is_alive()

@app.post("/configure")
async def configure(req: ConfigureRequest):
    global model_type, model, env_type, env, condition, agent_thread, callback_class
    global steps_counter, csv_index
    global transition_mode, transition_steps_remaining, transition_action
    
    model_type = req.model
    num_ues = req.num_ues
    new_env_type = req.env_type
    
    print(f"Configuring model: {model_type} with {num_ues} UE(s)")
    
    if is_agent_thread_running() and CONTINUE_TRAINING:
        print(f"Detectado hilo de entrenamiento activo")
        
        if (env is not None and 
            env_type == new_env_type and 
            hasattr(env, 'get_num_ues') and 
            env.get_num_ues() == num_ues and
            model_type == "PPO"):
            
            print(f"Configuración compatible - continuando entrenamiento existente")
            print(f"Estado actual: steps_counter={steps_counter}, csv_index={csv_index}")
            
            transition_mode = True
            transition_steps_remaining = 20
            transition_action = {
                "v": 0,
                "wq": [0 for _ in range(num_ues)],
                "wg": [0 for _ in range(num_ues)]
            }
            print(f"ACTIVANDO MODO TRANSICIÓN: 20 steps con acción {transition_action}")
            
            load_training_state()
            
            return {
                "status": "success", 
                "message": f"Entrenamiento en curso continuado desde step {steps_counter} con transición de 10 steps",
                "training_active": True,
                "current_step": steps_counter,
                "transition_mode": True,
                "transition_steps": 10
            }
        else:
            print(f"Configuración incompatible - necesario reiniciar entrenamiento")
            print(f"   Env type: {env_type} -> {new_env_type}")
            print(f"   UE count: {env.get_num_ues() if env else 'None'} -> {num_ues}")
            print(f"   Model: {model_type}")
            
            transition_mode = False
            transition_steps_remaining = 0
            transition_action = None
    
    env_type = new_env_type
    
    if (env is not None and 
        hasattr(env, 'get_num_ues') and 
        env.get_num_ues() == num_ues):
        print(f"Reutilizando entorno existente ({env_type})")
        training_continues = load_training_state()
    else:
        print(f"Creando nuevo entorno ({env_type})")
        training_continues = load_training_state()
        
        # Solo crear nuevo entorno si es necesario
        if model_type == "PPO":
            condition = Condition()
            if env_type == "ISAC":
                env = CustomEnvISAC(num_ues, condition)
                callback_class = MeanRewardLogCallbackISAC
                print("Using ISAC environment")
            elif env_type == "Basic":
                env = CustomEnvBasic(num_ues, condition)
                callback_class = MeanRewardLogCallbackBasic
                print("Using basic environment")
            else:
                print(f"Invalid env_type: {env_type}")
                return {"status": "error", "message": f"Invalid env_type: {env_type}"}
        elif model_type == "DQN":
            condition = Condition()
            if env_type == "ISAC":
                env = CustomEnvDQN(num_ues, condition)
                callback_class = MeanRewardLogCallbackDQN
                print("Using DQN ISAC environment")
            elif env_type == "Basic":
                env = CustomEnvDQNBasic(num_ues, condition)
                callback_class = MeanRewardLogCallbackDQNBasic
                print("Using DQN Basic environment")
        else:
            return {"status": "error", "message": f"Invalid model_type: {model_type}"}
    
    # Configurar archivo CSV
    csv_filename = f'learning_curve_{env_type}_0.csv'
    if not training_continues:
        with open(csv_filename, 'w') as f:
            f.write('index,step,reward\n')
        print(f"Creado nuevo archivo CSV: {csv_filename}")
    else:
        print(f"Continuando con archivo CSV existente: {csv_filename}")

    if model_type == "PPO" or model_type == "DQN":
        # REUTILIZAR MODELO si existe y es compatible
        if (model is not None and 
            hasattr(model, 'env') and 
            not is_agent_thread_running()):  # Solo reutilizar si no hay entrenamiento activo
            print(f"Reutilizando modelo existente")
            model.env = env
            if training_continues:
                model.num_timesteps = steps_counter
                print(f"Modelo configurado para continuar desde step {steps_counter}")
        elif not is_agent_thread_running():  # Solo crear nuevo modelo si no hay entrenamiento activo
            print(f"Creando nuevo modelo")
            
            # Carga de modelo preentrenado (código existente)
            if LOAD_MODEL:
                if env_type == "Basic":
                    # pretrained_model_path = f"./dqn_20k_Basic"
                    pretrained_model_path = f"./saved_models/dqn_model_20k_steps_Basic_sample"
                elif env_type == "ISAC":
                    if model_type == "DQN":
                        pretrained_model_path = f"./saved_models/dqn_model_20k_steps_ISAC_sample"
                    elif model_type == "PPO":
                        pretrained_model_path = f"./saved_models/ppo_model_20k_steps_ISAC_sample"
                print(f"Attempting to load pretrained model from: {pretrained_model_path}")
                try:
                    if model_type == "PPO":
                        model = PPO.load(pretrained_model_path, env=env)
                    elif model_type == "DQN":
                        model = DQN.load(pretrained_model_path, env=env)
                    print(f"Modelo preentrenado cargado desde: {pretrained_model_path}")
                    if not PREDICT:
                        agent_thread = Thread(target=agent_loop, daemon=True)
                        agent_thread.start()
                    return {"status": "success", "message": "Pretrained model loaded"}
                except Exception as e:
                    print(f"Error cargando modelo preentrenado: {e}")
                    print("Procediendo con entrenamiento desde cero...")
            
            # Create new model
            if model_type == "DQN":
                model = DQN(
                    "MlpPolicy", 
                    env, 
                    learning_rate=1e-4,
                    buffer_size=50000,
                    learning_starts=1000,
                    batch_size=64,
                    tau=1.0,
                    gamma=0.99,
                    train_freq=4,
                    gradient_steps=1,
                    target_update_interval=1000,
                    exploration_fraction=0.1,
                    exploration_initial_eps=1.0,
                    exploration_final_eps=0.02,
                    verbose=0
                )
                print("Modelo DQN creado")
            elif use_optimized_hyperparameters and model_type == "PPO":
                try:
                    best_params_file = "./optuna_results/icc_params.json"
                    if os.path.exists(best_params_file):
                        with open(best_params_file, 'r') as f:
                            best_params = json.load(f)
                        
                        policy_kwargs = best_params.pop("policy_kwargs", {})

                        if "activation_fn" in best_params:
                            best_params.pop("activation_fn")

                        if "activation_fn" in policy_kwargs:
                            act_fn_name = policy_kwargs["activation_fn"]
                            if act_fn_name == "tanh":
                                from torch.nn import Tanh
                                policy_kwargs["activation_fn"] = Tanh
                            elif act_fn_name == "relu":
                                from torch.nn import ReLU
                                policy_kwargs["activation_fn"] = ReLU
                            elif act_fn_name == "elu":
                                from torch.nn import ELU
                                policy_kwargs["activation_fn"] = ELU
                        
                        model = PPO("MlpPolicy", env, policy_kwargs=policy_kwargs, **best_params, verbose=0)
                        print("Modelo creado con hiperparámetros optimizados")
                    else:
                        model = PPO("MlpPolicy", env, n_steps=n_steps, batch_size=batch_size, verbose=0)
                        print("Modelo creado con parámetros por defecto")
                except Exception as e:
                    print(f"Error con hiperparámetros optimizados: {e}")
                    model = PPO("MlpPolicy", env, n_steps=n_steps, batch_size=batch_size, verbose=0)
            else:
                model = PPO("MlpPolicy", env, n_steps=n_steps, batch_size=batch_size, verbose=0)
            
            if training_continues:
                model.num_timesteps = steps_counter
                print(f"Modelo configurado para continuar desde step {steps_counter}")
        
        if not PREDICT and not is_agent_thread_running():
            agent_thread = Thread(target=agent_loop, daemon=True)
            agent_thread.start()
            print(f"Training thread initialized {'(continuing)' if training_continues else '(new)'}")
        elif is_agent_thread_running():
            print(f"Training thread already active — no new thread started")

    elif model_type in ["Random", "Static"]:
        print(f"Using {model_type.lower()} model")

    if not (is_agent_thread_running() and CONTINUE_TRAINING):
        transition_mode = False
        transition_steps_remaining = 0
        transition_action = None
    
    # Mensaje de estado final
    training_status = "active" if is_agent_thread_running() else "initialized"
    status_message = f"Training {training_status} since step {steps_counter}"
    
    return {
        "status": "success", 
        "message": status_message,
        "training_active": is_agent_thread_running(),
        "current_step": steps_counter,
        "transition_mode": transition_mode
    }

@app.post("/infer_sched_config")
async def infer_sched_config(req: InferRequest):
    global model_type, env_type
    global steps_counter
    global evaluation_mode, evaluation_steps_remaining, last_evaluation_step
    global pending_reward, pending_state, csv_index
    global transition_mode, transition_steps_remaining, transition_action
    
    # # generate_timestamp('infer_sched_config', steps_counter, 'recv')

    # print(f"[DEBUG] Step {steps_counter} -> Received state: {req.state}")
    if len(req.state["mcs"]) < 4: # NOTE: hardcoded for testing with 4 UEs
        print(f"[WARNING] MCS values length {len(req.state['mcs'])} less than number of UEs 4. Returning default action.")
        return {
            "v": 0,
            "wq": [0 for _ in range(4)],
            "wg": [0 for _ in range(4)]
        }
    mcs_values = np.array(req.state["mcs"], dtype=np.int32)
    q_values = np.array(req.state["q"], dtype=np.int32)
    g_values = np.array(req.state["g"], dtype=np.int32)
    obs_values = np.array(req.state["closeToObstacle"], dtype=np.int32)
    
    ### For testing. Remove later ###
    if env_type == "ISAC" or env_type == "Test":
        if len(mcs_values) >= 1: 
            if obs_values[0] == 1:
                mcs_values[0] = 0
            elif obs_values[0] == 0:
                mcs_values[0] = 1
        if len(mcs_values) >= 2:
            if obs_values[1] == 1:
                mcs_values[1] = 0
            elif obs_values[1] == 0:
                mcs_values[1] = 1
        if len(mcs_values) >= 3:
            if obs_values[2] == 1:
                mcs_values[2] = 0
            elif obs_values[2] == 0:
                mcs_values[2] = 1
        if len(mcs_values) >= 4:
            if obs_values[3] == 1:
                mcs_values[3] = 0
            elif obs_values[3] == 0:
                mcs_values[3] = 1
    
    obs_values = np.zeros(len(mcs_values), dtype=np.int32)
    q_values = np.zeros(len(mcs_values), dtype=np.int32)
    g_values = np.zeros(len(mcs_values), dtype=np.int32)
    
    if env_type == "ISAC":
        # print(f"[DEBUG] Step {steps_counter} -> Constructing ISAC state")
        state = np.concatenate([mcs_values, q_values, g_values, obs_values])
    elif env_type == "Basic":
        # print(f"[DEBUG] Step {steps_counter} -> Constructing Basic state")
        state = np.concatenate([mcs_values, q_values, g_values])
    else:
        print(f"Invalid env_type: {env_type}")
        state = "<invalid>"
    
    if pending_reward is not None:
        print(f"[PENDING REWARD] Using pending reward: {pending_reward} instead of current reward: {req.reward} in step {steps_counter}")
        reward = pending_reward
    else:
        reward = req.reward
    
    steps_counter += 1
    
    if transition_mode:
        transition_steps_remaining -= 1
        print(f"[TRANSITION] Step {steps_counter}: Devolviendo acción estática {transition_action} (quedan {transition_steps_remaining} steps)")
        
        with open(f'learning_curve_{env_type}_0.csv', 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([csv_index, steps_counter, reward])
            csv_index += 1
        
        if transition_steps_remaining <= 0:
            transition_mode = False
            transition_action = None
            print(f"[TRANSITION] TERMINADA en step {steps_counter}. Reanudando entrenamiento normal.")
        
        if model_type == "PPO" and env is not None:
            num_ues = env.get_num_ues()
        else:
            num_ues = len(mcs_values)
        
        return {
            "v": 0,
            "wq": [0 for _ in range(num_ues)],
            "wg": [0 for _ in range(num_ues)]
        }
    
    if False and (steps_counter % 5500 == 1 and not evaluation_mode and steps_counter >= 1001):
        pending_reward = reward
        print(f"!!! GUARDANDO reward pendiente: {pending_reward} del step {steps_counter}")
        evaluation_mode = True
        evaluation_steps_remaining = 500
        last_evaluation_step = steps_counter
        print(f"!!! INICIANDO EVALUACIÓN en step {steps_counter} por 500 steps")
        print(f"!!! GUARDANDO reward {reward} y state para procesar después de evaluación")
    
    if evaluation_mode:
        evaluation_steps_remaining -= 1
        
        if evaluation_steps_remaining <= 0:
            evaluation_mode = False
            print(f"!!! TERMINANDO EVALUACIÓN en step {steps_counter}")
            
            if pending_reward is not None:
                print(f"!!! PROCESANDO reward pendiente: {pending_reward} del step {steps_counter - 499}")
                
                if model_type == "PPO":
                    action = ppo(state, pending_reward)
                    print(f'[DELAYED_TRAIN] Action from delayed processing: {action}')
        
        print(f"[EVAL] Step {steps_counter}: Returning action [0,0,0] (remaining: {evaluation_steps_remaining})")
        
        if model_type == "PPO":
            num_ues = env.get_num_ues()
        else:
            num_ues = len(mcs_values)
        
        return {
            "v": 0,
            "wq": [1 for _ in range(num_ues)],
            "wg": [0 for _ in range(num_ues)]
        }
    
    print(f'[TRAIN] state: {state}, reward: {reward}')
    pending_reward = None
    
    # Save model at specific steps
    if csv_index == 20_000 and model_type == "PPO":
        model_save_path = f"./saved_models/ppo_model_20k_steps_{env_type}.zip"
        os.makedirs("./saved_models", exist_ok=True)
        model.save(model_save_path)
        print(f"Modelo guardado en {model_save_path} a los 20_000 steps")
    
    if csv_index == 20_000 and model_type == "DQN":
        model_save_path = f"./saved_models/dqn_model_20k_steps_{env_type}.zip"
        os.makedirs("./saved_models", exist_ok=True)
        model.save(model_save_path)

    with open(f'learning_curve_{env_type}_0.csv', 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([csv_index, steps_counter, reward])
        csv_index += 1

    # Process action based on model type
    if model_type == "PPO":
        action = ppo(state, reward)
        print(f'Action: {action}')
        num_ues = env.get_num_ues()
        # # generate_timestamp('infer_sched_config', steps_counter, 'resp')
        return {
            "v": int(action[0]),
            "wq": [int(x) for x in action[1 : 1 + num_ues]],
            "wg": [int(x) for x in action[1 + num_ues : 1 + 2 * num_ues]]
        }
    elif model_type == "DQN":
        action = ppo(state, reward)  # Usa la misma función pero DQN devuelve solo v
        print(f'DQN Action: {action}')
        
        if PREDICT:
            v, wq_list, wg_list = env.decode_action(action)
            print(f'DQN Full Action from env: v={v}, wq={wq_list}, wg={wg_list}')
            # # generate_timestamp('infer_sched_config', steps_counter, 'resp')
            return {
                "v": int(v),
                "wq": [int(x) for x in wq_list],
                "wg": [int(x) for x in wg_list]
            } 

        action_full = env.get_action()
        print(f'DQN Full Action from env: {action_full}')
        
        if action_full is not None:
            num_ues = env.get_num_ues()

            # # generate_timestamp('infer_sched_config', steps_counter, 'resp') 
            
            return {
                "v": int(action_full[0]),
                "wq": [int(x) for x in action_full[1 : 1 + num_ues]],
                "wg": [int(x) for x in action_full[1 + num_ues : 1 + 2 * num_ues]]
            }
        else:
            num_ues = len(mcs_values)
            return {
                "v": 0,
                "wq": [0 for _ in range(num_ues)],
                "wg": [0 for _ in range(num_ues)]
            }
    elif model_type == "Random":
        return random_tuning()
    elif model_type == "Static":
        return static_tuning(env_type, mcs_values[0])

@app.get("/training_status")
async def training_status():
    """Endpoint para verificar el estado actual del entrenamiento."""
    global steps_counter, csv_index, model, env_type, agent_thread
    global transition_mode, transition_steps_remaining
    
    return {
        "status": "active" if is_agent_thread_running() else "inactive",
        "env_type": env_type,
        "current_steps": steps_counter,
        "csv_index": csv_index,
        "model_loaded": model is not None,
        "thread_alive": is_agent_thread_running(),
        "thread_id": agent_thread.ident if is_agent_thread_running() else None,
        "evaluation_mode": evaluation_mode,
        "transition_mode": transition_mode,
        "transition_steps_remaining": transition_steps_remaining
    }

@app.post("/stop_training")
async def stop_training():
    """Fuerza la parada del entrenamiento."""
    global agent_thread, model, env_type, csv_index
    
    if is_agent_thread_running():
        # Opcional: Guardar modelo antes de parar
        if model is not None and model_type == "PPO":
            try:
                model_save_path = f"./saved_models/ppo_model_stop_{csv_index}_steps_{env_type}.zip"
                os.makedirs("./saved_models", exist_ok=True)
                model.save(model_save_path)
                print(f"Modelo guardado antes de parar: {model_save_path}")
            except Exception as e:
                print(f"Error guardando modelo: {e}")
        agent_thread = None
        
        return {"status": "success", "message": "Training stopped"}
    else:
        return {"status": "info", "message": "No training in progress to stop"}