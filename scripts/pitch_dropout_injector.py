#!/usr/bin/env python3
"""
Pitch-Induced Flight Dropout & Body-Occlusion Injector for SITL.
Models the physical behavior of RPLidar S2L on the Quiver quadcopter:
- Monitors drone altitude (AGL) and Pitch angle via ATTITUDE & GLOBAL_POSITION_INT.
- When AGL > 5m and Pitch < -15 deg (forward flight tilt), physical occlusion occurs:
  Frontal sectors (315 deg to 45 deg) are zeroed or masked out.
- During Yaw rotation (yaw rate > 15 deg/s), recovery of 30-40% of points occurs.
- Injects simulated sensor data via MAVLink OBSTACLE_DISTANCE stream.
"""

import math
import time
import argparse
from pymavlink import mavutil


def main():
    parser = argparse.ArgumentParser(description="Quiver SITL Flight Dropout Injector")
    parser.add_argument("--connection", default="udpin:0.0.0.0:14552", help="MAVLink connection port")
    parser.add_argument("--target-system", type=int, default=1, help="Target system ID")
    parser.add_argument("--target-component", type=int, default=1, help="Target component ID")
    args = parser.parse_args()

    print(f"[Dropout-Injector] Connecting to {args.connection}...")
    master = mavutil.mavlink_connection(args.connection)

    print("[Dropout-Injector] Waiting for heartbeat...")
    master.wait_heartbeat()
    print("[Dropout-Injector] Heartbeat received. Monitoring flight dynamics...")

    current_pitch_deg = 0.0
    current_yaw_rate_deg = 0.0
    current_rel_alt_m = 0.0

    dropout_active = False

    while True:
        msg = master.recv_match(type=['ATTITUDE', 'GLOBAL_POSITION_INT'], blocking=True, timeout=0.1)
        if msg:
            m_type = msg.get_type()
            if m_type == 'ATTITUDE':
                # ArduPilot pitch: positive nose up, negative nose down (forward flight)
                current_pitch_deg = math.degrees(msg.pitch)
                current_yaw_rate_deg = abs(math.degrees(msg.yawspeed))
            elif m_type == 'GLOBAL_POSITION_INT':
                current_rel_alt_m = msg.relative_alt / 1000.0

        # Physical condition: Altitude > 5m and Pitch <= -15 deg (or >= 15 deg forward tilt)
        forward_tilt = abs(current_pitch_deg) if current_pitch_deg < 0 else 0
        is_occluded = (current_rel_alt_m > 5.0) and (forward_tilt >= 15.0)

        # Baseline obstacle distance array (72 sectors, 5 degrees each = 360 deg)
        # Default clear detection at 10 meters (1000 cm)
        distances = [1000] * 72

        if is_occluded:
            if not dropout_active:
                print(f"[Dropout-Injector] >> PHYSICAL DROPOUT TRIGGERED: Alt={current_rel_alt_m:.1f}m, Pitch={current_pitch_deg:.1f} deg")
                dropout_active = True

            # Frontal sectors: 0 deg +/- 45 deg -> indices 0..9 and 63..71
            # Check if yaw rotation gives 35% partial recovery
            recovery_ratio = 0.35 if current_yaw_rate_deg > 15.0 else 0.0

            for i in list(range(0, 10)) + list(range(63, 72)):
                if recovery_ratio > 0 and (i % 3 == 0):
                    distances[i] = 1000  # recovered ray
                else:
                    distances[i] = 65535  # out-of-range / blind / no return
        else:
            if dropout_active:
                print(f"[Dropout-Injector] << SENSOR RECOVERED: Alt={current_rel_alt_m:.1f}m, Pitch={current_pitch_deg:.1f} deg")
                dropout_active = False

        # Transmit OBSTACLE_DISTANCE packet back to ArduPilot AP_Proximity backend
        master.mav.obstacle_distance_send(
            int(time.time() * 1e6), # time_usec
            0,                      # sensor_type: MAV_DISTANCE_SENSOR_LASER
            distances,              # distances in cm (array of 72)
            5,                      # angular_width (5 deg per sector)
            20,                     # min_distance (20 cm)
            1800,                   # max_distance (1800 cm = 18m)
            0.0,                    # increment_f
            0.0,                    # angle_offset
            12                      # frame: MAV_FRAME_BODY_FRD
        )

        time.sleep(0.05)  # 20 Hz update rate


if __name__ == "__main__":
    main()
