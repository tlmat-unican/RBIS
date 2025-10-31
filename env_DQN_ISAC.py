import gymnasium as gym
from gymnasium import spaces
import numpy as np
from stable_baselines3.common.callbacks import BaseCallback

class CustomEnvDQN(gym.Env):
    """
    Environment compatible with DQN (Discrete action space).
    
    En lugar de MultiDiscrete([3, 1, 1, 1, ...]), 
    usa Discrete(3) donde solo se controla el parámetro 'v'.
    
    Los pesos w_q y w_g se fijan a 0 (o podrían ser parte del state si es necesario).
    """

    STATE_SPACE = 4
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
    V_STATES = 4
    # 2 UEs:
    # V_STATES = 5
    """Number of values for V, 0-indexed.
    """
    # WQ_STATES = [0, 1, 2, 3]
    WQ_STATES = 1
    """Values for w_q in a list.
    """
    # 1 UE:
    WG_STATES = 2
    # 2 UEs:
    # WG_STATES = 3
    """Values for w_g in a list.
    """

    # Rangos para cada componente del estado
    MCS_MIN, MCS_MAX = 0, 2          # MCS: [0, 1, 2]
    QUEUE_MIN, QUEUE_MAX = 0, 2      # Queue: [0, 1, 2] 
    OBSTACLE_MIN, OBSTACLE_MAX = 0, 1 # Obstacle: [0, 1] (binario)

    def __init__(self, num_ues, condition):
        super(CustomEnvDQN, self).__init__()

        self.num_ues = num_ues
        self.condition = condition
        self.state = np.zeros(self.STATE_SPACE * num_ues, dtype=np.int32)
        self.reward = 0
        self.action = None

        # Observation space:
        low_bounds = np.array(
            [self.MCS_MIN] * num_ues +      # MCS para cada UE
            [self.QUEUE_MIN] * num_ues +    # Q para cada UE  
            [self.QUEUE_MIN] * num_ues +     # G para cada UE
            [self.OBSTACLE_MIN] * num_ues   # Obstacle para cada UE
        )
        high_bounds = np.array(
            [self.MCS_MAX] * num_ues +      # MCS para cada UE
            [self.QUEUE_MAX] * num_ues +    # Q para cada UE
            [self.QUEUE_MAX] * num_ues +     # G para cada UE  
            [self.OBSTACLE_MAX] * num_ues   # Obstacle para cada UE
        )

        self.observation_space = spaces.Box(
            low=low_bounds, 
            high=high_bounds, 
            shape=(self.STATE_SPACE * num_ues,), 
            dtype=np.int32
        )

        # Action space: Producto cartesiano de v y todas las combinaciones de wg
        # Total = V_STATES × (WG_STATES ^ num_ues)
        # Ejemplo con 4 UEs: 4 × 3^4 = 4 × 81 = 324 acciones
        total_wg_combinations = self.WG_STATES ** num_ues
        self.action_space = spaces.Discrete(self.V_STATES * total_wg_combinations)
        
        print(f"Action space size: {self.action_space.n} "
              f"(v={self.V_STATES} × wg_combinations={total_wg_combinations})")

    def decode_action(self, action_idx):
        """
        Decodifica el índice de acción a valores individuales (v, [wq0, wq1, ...], [wg0, wg1, ...]).
        
        Args:
            action_idx: int en [0, V_STATES × WG_STATES^num_ues - 1]
        
        Returns:
            tuple: (v, wq_list, wg_list)
                - v: int valor común
                - wq_list: list de num_ues valores wq (siempre 0 por ahora)
                - wg_list: list de num_ues valores wg
        
        Ejemplo con 4 UEs y WG_STATES=2:
            action_idx=0   → v=0, wq=[0,0,0,0], wg=[0,0,0,0]
            action_idx=1   → v=0, wq=[0,0,0,0], wg=[0,0,0,1]
            action_idx=2   → v=0, wq=[0,0,0,0], wg=[0,0,1,0]
            ...
            action_idx=16  → v=1, wq=[0,0,0,0], wg=[0,0,0,0]
            ...
        """
        total_wg_combinations = self.WG_STATES ** self.num_ues
        
        # Extraer v
        v = action_idx // total_wg_combinations
        
        # Extraer combinación de wg
        wg_combination_idx = action_idx % total_wg_combinations
        
        # Decodificar combinación de wg (base-N a lista de valores)
        wg_list = []
        for _ in range(self.num_ues):
            wg_list.append(wg_combination_idx % self.WG_STATES)
            wg_combination_idx //= self.WG_STATES
        
        # Invertir orden (por cómo funciona la decodificación base-N)
        wg_list = wg_list[::-1]
        
        # TODO wq siempre es 0 por ahora (futuro: podría ser parte del action_space)
        wq_list = [0] * self.num_ues
        # print(f"DECODE_ACTION: action_idx={action_idx} → v={v}, wq={wq_list}, wg={wg_list}")    
        return int(v), [int(wq) for wq in wq_list], [int(wg) for wg in wg_list]

    def get_num_ues(self):
        """Return the number of UEs in this environment"""
        return self.num_ues
    
    def update_data(self, data):
        """Update state and reward from external source (NS-3 or training_client)"""
        self.state, self.reward = data 
    
    def _get_obs(self) -> np.array:
        return self.state
    
    def _get_reward(self) -> float:
        return self.reward

    def get_action(self):
        """
        Return the last action taken (converted to expected format).

        Formato esperado: [v, wq0, wq1, wq2, wq3, wg0, wg1, wg2, wg3]
        - v: valor global común
        - wg_i: valor individual por UE
        """
        if self.action is None:
            return None
        
        # Decodificar acción
        v, wg_list, wq_list = self.decode_action(self.action)
        
        # Formato: [v, wg0, wg1, wg2, wg3]
        action_array = [v]
        for i in range(self.num_ues):
            action_array.append(wg_list[i]) # wg individual
        for i in range(self.num_ues):
            action_array.append(wq_list[i]) # wq individual
        
        # print(f"GET_ACTION: action_idx={self.action} → v={v}, wq={wq_list}, wg={wg_list} → {action_array}")
        return np.array(action_array)

    def reset(self, seed=None, options=None):
        """Reset the environment"""
        if seed is not None:
            np.random.seed(seed)
        
        self.current_idx = 0
        self.action = None
        state = self._get_obs()
        return state, {}

    def step(self, action):
        """
        Execute one step in the environment.
        
        Args:
            action: int en [0, V_STATES × WG_STATES^num_ues - 1]
        
        Returns:
            tuple: (next_state, reward, done, truncated, info)
        """
        # Guardar acción
        self.action = action
        
        # Esperar a que NS-3 o training_client envíe el siguiente estado
        self.condition.wait()
        
        next_state = self._get_obs()
        reward = self._get_reward()
        done = False
        truncated = False
        
        # ✅ Info adicional con acción decodificada
        v, wg_list, wq_list = self.decode_action(action)
        info = {
            'v': v,
            'wq': wq_list,
            'wg': wg_list,
            'action_idx': action
        }
        
        # Notificar que se procesó el estado
        self.condition.notify()

        return next_state, reward, done, truncated, info

    def close(self):
        """Clean up resources"""
        pass


class MeanRewardLogCallbackDQN(BaseCallback):
    """
    Callback para logging durante entrenamiento con DQN.
    """
    def __init__(self, verbose=0, log_interval=100):
        super(MeanRewardLogCallbackDQN, self).__init__(verbose)
        self.log_interval = log_interval
        self.episode_rewards = []
        self.episode_lengths = []
        self.current_episode_reward = 0
        self.current_episode_length = 0

    def _on_step(self) -> bool:
        """
        Se llama después de cada paso en el entrenamiento.
        """
        # Acumular reward del paso actual
        reward = self.locals.get('rewards', [0])[0]
        self.current_episode_reward += reward
        self.current_episode_length += 1

        # Verificar si el episodio terminó
        done = self.locals.get('dones', [False])[0]
        if done:
            self.episode_rewards.append(self.current_episode_reward)
            self.episode_lengths.append(self.current_episode_length)
            self.current_episode_reward = 0
            self.current_episode_length = 0

        # Logging periódico
        if self.n_calls % self.log_interval == 0:
            if len(self.episode_rewards) > 0:
                mean_reward = np.mean(self.episode_rewards[-100:])
                mean_length = np.mean(self.episode_lengths[-100:])
                
                if self.verbose > 0:
                    print(f"Step: {self.n_calls}, "
                          f"Mean Reward (last 100 eps): {mean_reward:.2f}, "
                          f"Mean Length: {mean_length:.1f}")

        return True

    def _on_training_end(self) -> None:
        """Se llama al final del entrenamiento"""
        if self.verbose > 0:
            print(f"\nEntrenamiento completado!")
            print(f"Total episodes: {len(self.episode_rewards)}")
            if len(self.episode_rewards) > 0:
                print(f"Mean reward: {np.mean(self.episode_rewards):.2f}")
