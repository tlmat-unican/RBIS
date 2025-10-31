import gymnasium as gym
from gymnasium import spaces
import numpy as np

class CustomEnv(gym.Env):

    STATE_SPACE = 3
    """Number of components in the state per UE: MCS, Q, G, Obstacle
    """

    NUM_SETTINGS = 3
    """Number of settings that are to be set up by the environment.
    Effectively defines the size of the action space.
    """
    MCS_STATES = 3
    """Number of MCS states recognized. Assumed to be 0-indexed.
    """
    # 1 UE:
    V_STATES = 3
    # 2 UEs:
    # V_STATES = 5
    """Number of values for V, 0-indexed.
    """
    # WQ_STATES = [0, 1, 2, 3]
    WQ_STATES = 1
    """Values for w_q in a list.
    """
    # 1 UE:
    WG_STATES = 1
    # 2 UEs:
    # WG_STATES = 3
    """Values for w_g in a list.
    """

    # Rangos para cada componente del estado
    MCS_MIN, MCS_MAX = 0, 2          # MCS: [0, 1, 2]
    QUEUE_MIN, QUEUE_MAX = 0, 2      # Queue: [0, 1, 2] 
    OBSTACLE_MIN, OBSTACLE_MAX = 0, 1 # Obstacle: [0, 1] (binario)

    def __init__(self, num_ues, condition):
        super(CustomEnv, self).__init__()

        self.num_ues = num_ues

        # Environment parameters
        self.condition = condition
        self.state = np.zeros(self.STATE_SPACE*num_ues, dtype=np.int32)
        self.reward = 0

        # Observation space
        # The observation space is a box with shape (num_ues,)
        # self.observation_space = spaces.Box(low=-np.inf, high=np.inf, shape=(num_ues,), dtype=np.int32)
        # self.observation_space = spaces.Box(low=-np.inf, high=np.inf, shape=(4*num_ues, ), dtype=np.int32)
        low_bounds = np.array(
            [self.MCS_MIN] * num_ues +      # MCS para cada UE
            [self.QUEUE_MIN] * num_ues +    # Q para cada UE
            [self.QUEUE_MIN] * num_ues      # G para cada UE
        )
        
        high_bounds = np.array(
            [self.MCS_MAX] * num_ues +      # MCS para cada UE
            [self.QUEUE_MAX] * num_ues +    # Q para cada UE
            [self.QUEUE_MAX] * num_ues      # G para cada UE  
        )

        self.observation_space = spaces.Box(
            low=low_bounds, 
            high=high_bounds, 
            shape=(self.STATE_SPACE*num_ues,), 
            dtype=np.int32  # Cambiar a int32 para consistencia
        )

        # Action space
        # The action space is a multi-discrete space with 3 discrete actions (v, w_q, w_g)
        ## Pesos generales
        # self.action_space = spaces.MultiDiscrete([self.V_STATES, len(self.WQ_STATES), len(self.WG_STATES)])
        ## Pesos individuales por UE
        self.action_space = spaces.MultiDiscrete(
            [self.V_STATES] + 
            [self.WQ_STATES] * num_ues + 
            [self.WG_STATES] * num_ues
        )

    def get_num_ues(self):
        """Return the number of UEs in this environment"""
        return self.num_ues
    
    def update_data(self, data):
        print(f'Updating data: {data}')
        self.state, self.reward = data 
    
    def _get_obs(self) -> np.array:
        return self.state
    
    def _get_reward(self) -> float:
        return self.reward

    def get_action(self):
        return self.action

    def reset(self, seed=None, options=None):
        self.current_idx = 0
        state = self._get_obs()
        print(f'current_state: {self.state}, current_reward: {self.reward}')
        return state, {}  # Devuelve un tuple (state, info)

    def step(self, action=None):
        # Wait for the /process_json endpoint to receive state and reward from ns3
        print('Waiting for ns3 to simulate next state and calculate reward...')
        self.condition.wait()
        next_state = self._get_obs()
        reward = self._get_reward()
        done = False
        truncated = False  # Not used
        info = {}
        self.action = action
        self.condition.notify()

        return next_state, reward, done, truncated, info

    def render(self, mode='human'):
        pass

# --- Custom Callback for logging rewards ---
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.vec_env import VecEnv

class MeanRewardLogCallback(BaseCallback):
    """
    Un callback custom para registrar la recompensa promedio por paso
    en un entorno continuo.
    """
    def __init__(self, verbose: int = 0):
        super().__init__(verbose)

    def _on_step(self) -> bool:
        """
        Este método se llama después de cada paso en el entorno.
        No lo usamos para logging principal aquí.
        """
        return True

    def _on_rollout_end(self) -> None:
        """
        This method is called after each step in the environment.  
        We do not use it for main logging here.
        """
        rollout_buffer = self.locals.get("rollout_buffer")
        if rollout_buffer is not None:
            rewards_buffer = rollout_buffer.rewards.flatten()
            actions_buffer = rollout_buffer.actions  # shape: (n_steps, action_dim)

            if len(rewards_buffer) > 0:
                mean_step_reward = np.mean(rewards_buffer)
                self.logger.record("rollout/mean_step_reward", mean_step_reward)
                std_step_reward = np.std(rewards_buffer)
                self.logger.record("rollout/std_step_reward", std_step_reward)
            
            if len(rewards_buffer) > 0:
                mean_action = np.mean(actions_buffer, axis=0)
                std_action = np.std(actions_buffer, axis=0)
                self.logger.record("rollout/mean_action", mean_action)
                self.logger.record("rollout/std_action", std_action)

                if self.verbose > 0:
                     print(f"Timestep: {self.num_timesteps}, Mean Step Reward (Rollout): {mean_step_reward:.4f}, STD Step Reward (Rollout): {std_step_reward:.4f}")
                     print(f"Timestep: {self.num_timesteps}, Mean Action (Rollout): {mean_action}, STD Action (Rollout): {std_action}")