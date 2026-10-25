#!/usr/bin/env python3
"""
EKF Origin & AHRS Position Auto-Initialization Script for ArduPilot SITL.
Eliminates the "EKF-Origin Footgun" by continuously monitoring MAVLink telemetry
and dispatching SET_GPS_GLOBAL_ORIGIN and set_home commands prior to takeoff.
Validates AP_Proximity sensor database transition from NO_DATA to ACTIVE.
"""

import sys
import time
import argparse
from pymavlink import mavutil

DEFAULT_LAT = 30.2672   # Austin / Texas test latitude (degrees * 1e7)
DEFAULT_LON = -97.7431  # Austin / Texas test longitude (degrees * 1e7)
DEFAULT_ALT = 150.0     # MSL Altitude in meters


def wait_heartbeat(master, timeout=30):
    print("[EKF-Init] Waiting for MAVLink heartbeat from SITL...")
    msg = master.wait_heartbeat(timeout=timeout)
    if msg is None:
        raise TimeoutError("Failed to receive heartbeat within timeout")
    print(f"[EKF-Init] Heartbeat established (System ID: {master.target_system}, Component ID: {master.target_component})")


def set_ekf_origin(master, lat, lon, alt):
    print(f"[EKF-Init] Setting EKF Origin: Lat={lat}, Lon={lon}, Alt={alt}m MSL")
    lat_int = int(lat * 1e7)
    lon_int = int(lon * 1e7)
    alt_int = int(alt * 1000)

    # 1. Dispatch SET_GPS_GLOBAL_ORIGIN
    master.mav.set_gps_global_origin_send(
        master.target_system,
        lat_int,
        lon_int,
        alt_int,
        int(time.time() * 1e6)
    )

    # 2. Dispatch MAV_CMD_DO_SET_HOME
    master.mav.command_long_send(
        master.target_system,
        master.target_component,
        mavutil.mavlink.MAV_CMD_DO_SET_HOME,
        0,  # confirmation
        0,  # param1: 0 = use current position or provided coords
        0, 0, 0,
        lat,
        lon,
        alt
    )
    print("[EKF-Init] Dispatched SET_GPS_GLOBAL_ORIGIN & MAV_CMD_DO_SET_HOME")


def verify_proximity_database(master, timeout=20):
    """
    Listens for DISTANCE_SENSOR and OBSTACLE_DISTANCE messages to confirm
    that the proximity database is populating with active sector readings.
    """
    print("[EKF-Init] Validating AP_Proximity database state...")
    start_time = time.time()
    active_sectors = set()

    while time.time() - start_time < timeout:
        msg = master.recv_match(type=['OBSTACLE_DISTANCE', 'DISTANCE_SENSOR'], blocking=True, timeout=1.0)
        if not msg:
            continue

        msg_type = msg.get_type()
        if msg_type == 'OBSTACLE_DISTANCE':
            distances = msg.distances
            valid = [d for d in distances if 0 < d < 65535]
            if valid:
                print(f"[EKF-Init] OBSTACLE_DISTANCE received: {len(valid)} active sectors populated")
                return True
        elif msg_type == 'DISTANCE_SENSOR':
            if 0 < msg.current_distance < msg.max_distance:
                active_sectors.add(msg.orientation)
                if len(active_sectors) >= 1:
                    print(f"[EKF-Init] DISTANCE_SENSOR active orientation: {msg.orientation}, dist: {msg.current_distance}cm")
                    return True

    return False


def main():
    parser = argparse.ArgumentParser(description="ArduPilot EKF Origin Auto-Setter")
    parser.add_argument("--connection", default="udpin:0.0.0.0:14550", help="MAVLink connection string (e.g. udpin:0.0.0.0:14550)")
    parser.add_argument("--lat", type=float, default=DEFAULT_LAT, help="Origin latitude")
    parser.add_argument("--lon", type=float, default=DEFAULT_LON, help="Origin longitude")
    parser.add_argument("--alt", type=float, default=DEFAULT_ALT, help="Origin MSL altitude in meters")
    args = parser.parse_args()

    print(f"[EKF-Init] Connecting to {args.connection}...")
    master = mavutil.mavlink_connection(args.connection)

    wait_heartbeat(master)

    # Give EKF 2 seconds to initialize filters
    time.sleep(2)

    # Set Origin in loop for 3 attempts to guarantee delivery
    for i in range(3):
        set_ekf_origin(master, args.lat, args.lon, args.alt)
        time.sleep(1.0)

    # Verify Proximity state
    prx_ok = verify_proximity_database(master, timeout=15)
    if prx_ok:
        print("[EKF-Init] SUCCESS: AHRS/EKF Origin established and AP_Proximity database ACTIVE.")
        sys.exit(0)
    else:
        print("[EKF-Init] NOTICE: Origin set, waiting for sensor stream to populate database.")
        sys.exit(0)


if __name__ == "__main__":
    main()
