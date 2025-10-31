#!/usr/bin/env python3
"""
Script para superponer trayectorias de UEs sobre mapas REM SINR
Autor: AI Assistant
Fecha: 2025-07-08

Este script:
1. Lee el archivo position.txt con las posiciones de los UEs
2. Carga el mapa SINR (nr-rem-scheduler-sinr.png)
3. Superpone las trayectorias de los UEs sobre el mapa
4. Añade marcadores para posiciones inicial/final y gNB
5. Genera un plot combinado
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from matplotlib.patches import Circle
import os
import argparse
from pathlib import Path

class UETrajectoryPlotter:
    def __init__(self, base_path="/home/administrator/DRL_Sched_UNIBO/ns-3-dev"):
        self.base_path = Path(base_path)
        self.position_file = self.base_path / "position.txt"
        self.sinr_map_file = self.base_path / "nr-rem-scheduler-sinr.png"
        
        # Parámetros del mapa REM (ajustar según configuración)
        self.map_bounds = {
            'x_min': -400.0,
            'x_max': 400.0,
            'y_min': -400.0,
            'y_max': 400.0
        }
        
        # Colores para diferentes UEs
        self.ue_colors = ['red', 'blue', 'green', 'orange', 'purple', 'brown', 'pink', 'gray', 'olive', 'cyan']
        
    def load_position_data(self):
        """Carga y procesa los datos de posición de los UEs"""
        try:
            if not self.position_file.exists():
                print(f"Warning: {self.position_file} no encontrado")
                return None
                
            # Leer datos de posición
            df = pd.read_csv(self.position_file, sep='\t')
            print(f"Cargados {len(df)} puntos de posición")
            print(f"UEs encontrados: {sorted(df['ue'].unique())}")
            print(f"Rango temporal: {df['time'].min():.1f} - {df['time'].max():.1f} ms")
            
            return df
        except Exception as e:
            print(f"Error cargando position.txt: {e}")
            return None
    
    def load_gnb_positions(self):
        """Carga las posiciones de las estaciones base (gNB)"""
        return [(0.0, 0.0)]  # Posición por defecto
    
    def load_sinr_map(self):
        """Carga el mapa SINR como imagen de fondo"""
        try:
            if not self.sinr_map_file.exists():
                print(f"Warning: {self.sinr_map_file} no encontrado")
                return None
                
            img = mpimg.imread(self.sinr_map_file)
            print(f"Cargado mapa SINR: {img.shape}")
            return img
        except Exception as e:
            print(f"Error cargando mapa SINR: {e}")
            return None
    
    def plot_trajectories_on_sinr_map(self, save_path=None, show_time_evolution=True, time_step=100):
        """Genera el plot principal con trayectorias sobre el mapa SINR"""
        
        # Cargar datos
        position_df = self.load_position_data()
        gnb_positions = self.load_gnb_positions()
        sinr_img = self.load_sinr_map()
        
        if position_df is None:
            print("No se pueden cargar los datos de posición")
            return
        
        # Crear figura
        fig, ax = plt.subplots(1, 1, figsize=(12, 10))
        
        # Mostrar mapa SINR de fondo si está disponible
        if sinr_img is not None:
            extent = [self.map_bounds['x_min']-182, self.map_bounds['x_max']+255, 
                     self.map_bounds['y_min']-100, self.map_bounds['y_max']+30]
            ax.imshow(sinr_img, extent=extent, aspect='equal', alpha=0.8, origin='upper')
        
        # Plot trayectorias para cada UE
        ue_list = sorted(position_df['ue'].unique())
        for i, ue_id in enumerate(ue_list):
            ue_data = position_df[position_df['ue'] == ue_id].sort_values('time')
            
            color = self.ue_colors[i % len(self.ue_colors)]
            
            # Trayectoria completa
            ax.plot(ue_data['x'], ue_data['y'], 
                   color=color, linewidth=2, alpha=0.8, 
                   label=f'UE {ue_id} trayectoria')
            
            # Posición inicial
            start_x, start_y = ue_data.iloc[0]['x'], ue_data.iloc[0]['y']
            ax.scatter(start_x, start_y, 
                      color=color, s=100, marker='o', 
                      edgecolor='black', linewidth=2,
                      label=f'UE {ue_id} inicio', zorder=10)
            
            # Posición final
            end_x, end_y = ue_data.iloc[-1]['x'], ue_data.iloc[-1]['y']
            ax.scatter(end_x, end_y, 
                      color=color, s=100, marker='s', 
                      edgecolor='black', linewidth=2,
                      label=f'UE {ue_id} final', zorder=10)
            
            # Mostrar evolución temporal con puntos
            if show_time_evolution:
                time_points = ue_data[::time_step]  # Cada time_step puntos
                sizes = np.linspace(20, 60, len(time_points))  # Tamaño creciente
                ax.scatter(time_points['x'], time_points['y'], 
                          color=color, s=sizes, alpha=0.6, 
                          edgecolor='white', linewidth=1, zorder=5)
                # Buscar indices donde time sea 2000, 4000, 6000, etc.
                # Filtrar directamente sobre time_points
                marks = time_points[time_points['time'] % 2000 == 0]

                # Extraer coordenadas
                x_marks = marks['x'].values
                y_marks = marks['y'].values

                # Dibujar los marcadores
                ax.plot(x_marks, y_marks, marker='x', linestyle='None', 
                        color='black', markersize=15, zorder=10)
        
        # Plot posiciones de gNB
        if gnb_positions:
            for i, (gnb_x, gnb_y) in enumerate(gnb_positions):
                ax.scatter(gnb_x, gnb_y, 
                          color='black', s=200, marker='^', 
                          edgecolor='white', linewidth=3,
                          label=f'gNB {i+1}' if i == 0 else "", zorder=15)
                
                # Círculo alrededor del gNB
                # circle = Circle((gnb_x, gnb_y), radius=10, 
                #                fill=False, color='black', linewidth=2, linestyle='--')
                # ax.add_patch(circle)

        # Plot obstacle
        circle = Circle((200, 0), radius=50, fill=False, color='white', linewidth=2, zorder=20)
        ax.add_patch(circle)
        
        # Configurar ejes y labels
        ax.set_xlim(self.map_bounds['x_min'], self.map_bounds['x_max'])
        ax.set_ylim(self.map_bounds['y_min'], self.map_bounds['y_max'])
        ax.set_xlabel('X Position (m)', fontsize=12)
        ax.set_ylabel('Y Position (m)', fontsize=12)
        ax.set_title('UE Trajectories over SINR Map', fontsize=14, fontweight='bold')
        ax.grid(True, alpha=0.3)
        # # ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        
        # Añadir información estadística
        stats_text = f"Total UEs: {len(ue_list)}\n"
        stats_text += f"Total points: {len(position_df)}\n"
        stats_text += f"Time range: {position_df['time'].min():.0f}-{position_df['time'].max():.0f} ms"
        ax.text(0.02, 0.98, stats_text, transform=ax.transAxes, 
                verticalalignment='top', bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        plt.tight_layout()
        
        # Guardar si se especifica
        if save_path:
            print(f"Guardando plot en: {save_path}")
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Plot guardado en: {save_path}")
        
        plt.show()
        return fig, ax
    
    def create_time_animation_frames(self, output_dir="animation_frames", frame_interval=50):
        """Crea frames para una animación temporal de las trayectorias"""
        
        position_df = self.load_position_data()
        gnb_positions = self.load_gnb_positions()
        sinr_img = self.load_sinr_map()
        
        if position_df is None:
            return
        
        output_path = Path(output_dir)
        output_path.mkdir(exist_ok=True)
        
        time_points = sorted(position_df['time'].unique())
        ue_list = sorted(position_df['ue'].unique())
        
        print(f"Creando {len(time_points)//frame_interval} frames...")
        
        for i, current_time in enumerate(time_points[::frame_interval]):
            fig, ax = plt.subplots(1, 1, figsize=(10, 8))
            
            # Mapa de fondo
            if sinr_img is not None:
                extent = [self.map_bounds['x_min'], self.map_bounds['x_max'], 
                         self.map_bounds['y_min'], self.map_bounds['y_max']]
                ax.imshow(sinr_img, extent=extent, aspect='equal', alpha=0.7, origin='upper')
            
            # Plot trayectorias hasta el tiempo actual
            for j, ue_id in enumerate(ue_list):
                ue_data = position_df[(position_df['ue'] == ue_id) & 
                                     (position_df['time'] <= current_time)].sort_values('time')
                
                if len(ue_data) > 0:
                    color = self.ue_colors[j % len(self.ue_colors)]
                    
                    # Trayectoria hasta ahora
                    if len(ue_data) > 1:
                        ax.plot(ue_data['x'], ue_data['y'], 
                               color=color, linewidth=2, alpha=0.8)
                    
                    # Posición actual
                    current_x, current_y = ue_data.iloc[-1]['x'], ue_data.iloc[-1]['y']
                    ax.scatter(current_x, current_y, 
                              color=color, s=150, marker='o', 
                              edgecolor='black', linewidth=2, zorder=10)
            
            # gNB
            if gnb_positions:
                for gnb_x, gnb_y in gnb_positions:
                    ax.scatter(gnb_x, gnb_y, color='black', s=200, marker='^', 
                              edgecolor='white', linewidth=3, zorder=15)
            
            ax.set_xlim(self.map_bounds['x_min'], self.map_bounds['x_max'])
            ax.set_ylim(self.map_bounds['y_min'], self.map_bounds['y_max'])
            ax.set_title(f'UE Trajectories at t={current_time:.0f} ms', fontsize=14)
            ax.grid(True, alpha=0.3)
            
            frame_path = output_path / f"frame_{i:04d}.png"
            print(f"Guardando frame: {frame_path}")
            plt.savefig(frame_path, dpi=150, bbox_inches='tight')
            plt.close()
            
            if i % 10 == 0:
                print(f"Creado frame {i}/{len(time_points)//frame_interval}")
        
        print(f"Frames guardados en: {output_path}")
        print(f"Para crear video: ffmpeg -r 10 -i {output_path}/frame_%04d.png -c:v libx264 ue_trajectories.mp4")

def main():
    parser = argparse.ArgumentParser(description='Plot UE trajectories on SINR map')
    parser.add_argument('--base-path', default='/home/administrator/DRL_Sched_UNIBO/ns-3-dev',
                       help='Base path to ns-3 simulation files')
    parser.add_argument('--save', help='Save plot to file')
    parser.add_argument('--animation', action='store_true', help='Create animation frames')
    parser.add_argument('--time-step', type=int, default=100, 
                       help='Time step for showing evolution points')
    
    args = parser.parse_args()
    
    plotter = UETrajectoryPlotter(args.base_path)
    
    if args.animation:
        plotter.create_time_animation_frames()
    else:
        plotter.plot_trajectories_on_sinr_map(save_path=args.save, time_step=args.time_step)

if __name__ == "__main__":
    main()
