#ifndef NS3_DRL_INTERFACE_H
#define NS3_DRL_INTERFACE_H

#include <vector>
#include <unordered_map>
#include <ns3/vector.h>
#include "ns3/simulator.h"
#include <cmath>

namespace ns3
{

/**
 * @brief Class representing the scheduling state in NR MAC.
 *
 * Stores and manages UE state information, including 
 * Modulation and Coding Scheme (MCS), RLC queue (Q), virtual thput queue (G), Guaranteed Flow Bit Rate (GFBR), 
 * and location.
 */
class Ns3DrlInterface
{
  public:

    /**
     * @brief Private constructor for Singleton pattern.
     */
    Ns3DrlInterface();
    
    /**
     * @brief Private destructor for Singleton pattern.
     */
    ~Ns3DrlInterface();

    /**
     * @brief Resets the state, clearing all stored data.
     */
    void Reset();

    /**
     * @brief Get the singleton instance of Ns3DrlInterface
     * 
     * @return Reference to the singleton instance
     */
    static Ns3DrlInterface& GetInstance();

    /**
     * @brief Updates the state of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @param mcs Current Modulation and Coding Scheme (MCS) of the UE.
     * @param q RLC queue.
     * @param gfbr Guaranteed Flow Bit Rate (GFBR).
     * @param pos User position in the simulated space.
      * @param timeSlot Time slot duration.
     */
    void UpdateState(int rnti, double mcs, double q, double g, double gfbr, Vector pos, double timeSlot, double t_last_update);

    /**
     * @brief Calculates the reward.
     */
    void CalculateReward();

    /** 
     * @brief Updates the metrics of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @param thput Throughput.
     * @param rbs Number of Resource Blocks (RBs).
    */
    void UpdateMetrics(int rnti, double thput, double rbs, double total_rbs);

    void UpdateMetricsThput(int rnti, double thput);
    void UpdateMetricsRbs(int rnti, double rbs, double total_rbs);

    /**
     * @brief Retrieves the total number of UEs stored in the state.
     * 
     * @return Number of UEs.
     */
     int GetUEsNumber() const;

    /**
     * @brief Retrieves the state size.
     * 
     * @return State size.
     */
    int GetStateSize() const;

    /**
     * @brief Retrieves the MCS of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The MCS value associated with the UE.
     */
    double GetMcs(int rnti) const;

    /**
     * @brief Retrieves the RLC queue (Q) value of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The RLC queue (Q) value of the UE.
     */
    double GetQ(int rnti) const;

    /**
     * @brief Retrieves the virtual thput queue (G) value of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The virtual thput queue (G) value of the UE.
     */
     double GetG(int rnti) const;

    /**
     * @brief Retrieves the Guaranteed Flow Bit Rate (GFBR) of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The GFBR value of the UE.
     */
    double GetGfbr(int rnti) const;

    /**
     * @brief Retrieves the position of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The UE position in the simulated space.
     */
    Vector GetPos(int rnti) const;

    /**
     * @brief Retrieves the velocity of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The UE velocity.
     */
    Vector GetVelocity(int rnti) const;

    /**
     * @brief Retrieves the distance traveled by a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The distance traveled by the UE.
     */
    double GetDistance(int rnti) const;

    /**
     * @brief Retrieves the last time the state was updated for a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return The last update time of the UE.
     */
    double GetLastUpdateTime(int rnti) const;

    /**
     * @brief Retrieves the reward value.
     * 
     * @return The reward value.
     */
    float GetReward() const;

    float GetRewardThput() const; // only for dataset

    /**
     * @brief Checks if a UE is close to an obstacle or will pass through it.
     * 
     * Predicts if the UE will pass through the obstacle area (circle with center at (180,0) and radius 50m)
     * based on its current position and velocity.
     * 
     * @param rnti UE identifier.
     * @return True if the UE is close to or will pass through the obstacle area.
     */
    bool isCloseToObs(int rnti) const;

    /**
     * @brief Retrieves the close to obstacle status of a UE identified by its RNTI.
     * 
     * @param rnti UE identifier.
     * @return True if the UE is close to an obstacle.
     */
    bool GetCloseToObs(int rnti) const;


    /**
     * @brief Overloads the output stream operator to print the state.
     * 
     * @param os Output stream.
     * @param itf Ns3DrlInterface object to print.
     * @return Updated output stream.
     */
    friend std::ostream& operator<<(std::ostream& os, const Ns3DrlInterface& itf);

    // Estructuras para obstáculos híbridos (circulares y rectangulares)
    
    /**
     * @brief Estructura para obstáculos circulares.
     */
    struct CircularObstacle {
        Vector center;     ///< Centro del círculo
        double radius;     ///< Radio del círculo
        
        CircularObstacle(Vector c, double r) : center(c), radius(r) {}
        CircularObstacle(double x, double y, double z, double r) : center(x, y, z), radius(r) {}
    };
    
    /**
     * @brief Estructura para obstáculos rectangulares.
     */
    struct RectangularObstacle {
        double centerX, centerY;        ///< Centro del rectángulo
        double width, height;           ///< Ancho y alto del rectángulo
        double rotationAngle;           ///< Ángulo de rotación en radianes (0 = sin rotación)
        double minZ, maxZ;              ///< Límites en Z (opcional)
              
        // Constructor con centro, dimensiones y rotación
        RectangularObstacle(double centerX, double centerY, double width, double height,
                           double rotationAngle = 0.0, double minZ = -1.0, double maxZ = 1.0)
            : centerX(centerX), centerY(centerY), width(width), height(height),
              rotationAngle(rotationAngle), minZ(minZ), maxZ(maxZ) {}
    };

  private:

    // Funciones auxiliares para detección de obstáculos rectangulares
    bool IsPointInRectangle(const Vector& point, 
                           double minX, double maxX, 
                           double minY, double maxY) const;
                           
    bool DoesLineIntersectRectangle(const Vector& start, const Vector& end,
                                   double minX, double maxX,
                                   double minY, double maxY) const;
                                   
    // Funciones para rectángulos con rotación
    bool IsPointInRotatedRectangle(const Vector& point, const RectangularObstacle& obstacle) const;
    bool DoesLineIntersectRotatedRectangle(const Vector& start, const Vector& end,
                                          const RectangularObstacle& obstacle) const;
    Vector RotatePoint(const Vector& point, double centerX, double centerY, double angle) const;
    Vector InverseRotatePoint(const Vector& point, double centerX, double centerY, double angle) const;
                                   
    bool ClipLine(double denom, double num, double& tMin, double& tMax) const;
    
    bool DoLinesIntersect(const Vector& p1, const Vector& q1,
                         const Vector& p2, const Vector& q2) const;
    
    // Funciones auxiliares para detección de obstáculos circulares
    bool IsPointInCircle(const Vector& point, const Vector& center, double radius) const;
    bool DoesLineIntersectCircle(const Vector& start, const Vector& end,
                                const Vector& center, double radius) const;

    static Ns3DrlInterface* m_instance; ///< Static pointer to the singleton instance
    
    std::unordered_map<int, double> m_mcs;            ///< Stores the MCS of each UE.
    std::unordered_map<int, double> m_q;              ///< Stores the RLC queue (Q) of each UE.
    std::unordered_map<int, double> m_g;              ///< Stores the virtual thput queue (G) of each UE.
    std::unordered_map<int, double> m_gfbr;           ///< Stores the GFBR of each UE.
    std::unordered_map<int, double> m_distance;       ///< Stores the distance traveled by each UE.
    std::unordered_map<int, Vector> m_position;       ///< Stores the position of each UE.
    std::unordered_map<int, Vector> m_prev_position;  ///< Stores the previous slot position of each UE to calculate the velocity.
    std::unordered_map<int, Vector> m_velocity;       ///< Stores the velocity of each UE.
    std::unordered_map<int, bool> m_closeToObs;      ///< Stores if the UE is close to an obstacle.
    std::unordered_map<int, double> m_t_last_update;     ///< Stores the last update time for each UE.
    std::unordered_map<int, int> m_count;             ///< Internal counter for each UE.
    std::unordered_map<int, int> m_count_reward;      ///< Internal counter for each UE.

    std::unordered_map<int, double> m_thput;          ///< Stores the throughput of each UE.
    std::unordered_map<int, double> m_rbs;            ///< Stores the number of RBs of each UE.
    double m_total_rbs;                        ///< Stores the total number of RBs assigned to each UE.
  
    double m_reward;                                  ///< Reward value.
    double m_reward_thput;                            ///< only for dataset.
public:
    double m_mcs_prueba[4];                          ///< Variable for testing purposes.
};

} // namespace ns3

#endif // NS3_DRL_INTERFACE_H